/*
 * vbcrender - Command line tool to render videos from VBC files.
 * Copyright (C) 2019 Mirko Hahn
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <cassert>
#include <cmath>
#include <condition_variable>
#include <exception>
#include <future>
#include <iostream>
#include <list>
#include <mutex>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "OpenGLRenderer.hpp"
#include "OpenGLShaders.hpp"
#include "../../Styles.hpp"


namespace {
    typedef struct {
        size_t  memory;
        Rect bbox;

        std::vector<GLfloat> vertices;
        std::vector<GLuint>  categories;

        std::vector<GLuint>  fill_indices;
        std::vector<GLuint>  stroke_indices;
        std::vector<GLuint>  line_indices;
    } ProcessedTree;


    std::mutex glfw_lock;
    bool glfw_initialized = false;


    bool is_big_endian() {
        union {
            uint32_t i;
            char c[4];
        } bint = {0x01020304};

        return bint.c[0] == 1;
    }


    void draw_processed_tree(const ProcessedTree& tree, Rect viewport, OpenGLShaders& shaders) {
        // Names for temporarily created OpenGL objects
        GLuint buf[] = { 0, 0 };

        std::exception_ptr eptr;
        try {
            // Generate new buffers
            glGenBuffers(2, buf);
            glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);

            // Fill array buffer
            constexpr size_t stride = 2 * sizeof(GLfloat) + sizeof(GLuint);
            GLbyte *array_data = new GLbyte[stride * tree.categories.size()];
            for(size_t i = 0; i < tree.categories.size(); ++i) {
                *((GLfloat*)(array_data + stride * i))                      = tree.vertices[2 * i];
                *((GLfloat*)(array_data + stride * i + sizeof(GLfloat)))    = tree.vertices[2 * i + 1];
                *((GLuint*)(array_data + stride * i + 2 * sizeof(GLfloat))) = tree.categories[i];
            }
            glBufferData(GL_ARRAY_BUFFER, stride * tree.categories.size(), array_data, GL_STATIC_DRAW);
            delete[] array_data;

            // Specify location of buffer attributes
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, 0);
            glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, stride, (GLvoid*)(2 * sizeof(GLfloat)));

            // Calculate offsets in index array
            const size_t offs_fill = tree.line_indices.size();
            const size_t offs_stroke = offs_fill + tree.fill_indices.size();
            const size_t size_index = offs_stroke + tree.stroke_indices.size();

            // Fill element array buffer
            GLuint* index_data = new GLuint[size_index];
            std::copy(tree.line_indices.cbegin(), tree.line_indices.cend(), index_data);
            std::copy(tree.fill_indices.cbegin(), tree.fill_indices.cend(), index_data + offs_fill);
            std::copy(tree.stroke_indices.cbegin(), tree.stroke_indices.cend(), index_data + offs_stroke);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, size_index * sizeof(GLuint), index_data, GL_STATIC_DRAW);
            delete[] index_data;

            // Calculate transformation
            const Rect window { viewport.x0 + 10, viewport.y0 + 10, viewport.x1 - 10, viewport.y1 - 10 };
            const Rect bbox { tree.bbox.x0 - tree_node_radius, tree.bbox.y0 - tree_node_radius, tree.bbox.x1 + tree_node_radius, tree.bbox.y1 + tree_node_radius };
            const GLfloat scale = std::min((window.x1 - window.x0) / (bbox.x1 - bbox.x0), (window.y1 - window.y0) / (bbox.y1 - bbox.y0));
            const GLfloat xtrans = 0.5 * (viewport.x0 + viewport.x1) - 0.5 * scale * (bbox.x0 + bbox.x1);
            const GLfloat ytrans = 0.5 * (viewport.y0 + viewport.y1) - 0.5 * scale * (bbox.y0 + bbox.y1);

            // Calculate shape parameters
            const GLfloat scaled_radius = scale * GLfloat(tree_node_radius);
            GLfloat radius = GLfloat(tree_node_radius);
            GLuint segments = M_PI / std::asin(0.5 / scaled_radius) - 1;
            if(segments < 4) {
                segments = 4;
                radius = std::max(radius, GLfloat(1.0) / scale);
            }
            else if(segments > 64) {
                segments = 64;
            }

            // Update shader parameters
            shaders.set_transform(scale, xtrans, ytrans);
            shaders.update_shapes(radius, segments);

            // Draw lines
            if(!tree.line_indices.empty()) {
                shaders.use_line_program();
                glDrawElements(GL_LINES, tree.line_indices.size(), GL_UNSIGNED_INT, 0);
            }

            // Draw filled markers
            if(!tree.fill_indices.empty()) {
                shaders.use_fill_program();
                glDrawElements(GL_POINTS, tree.fill_indices.size(), GL_UNSIGNED_INT, (GLvoid*)(offs_fill * sizeof(GLuint)));
            }

            // Draw stroked markers
            if(!tree.stroke_indices.empty()) {
                shaders.use_stroke_program();
                glDrawElements(GL_POINTS, tree.stroke_indices.size(), GL_UNSIGNED_INT, (GLvoid*)(offs_stroke * sizeof(GLuint)));
            }
        } catch(...) {
            eptr = std::current_exception();
        }

        // Delete index buffer
        glDeleteBuffers(2, buf);

        if(eptr) {
            std::rethrow_exception(eptr);
        }
    }


    void glfw_error_handler(int error_code, const char* error_message) {
        std::cerr << "GLFW ERROR (" << std::hex << error_code << "): " << error_message << std::endl;
        std::terminate();
    }


    GLFWwindow* create_opengl_context(size_t width, size_t height) {
        // Lock access to GLFW for this operation
        std::lock_guard<std::mutex> guard(glfw_lock);

        // Initialize GLFW if necessary
        if(!glfw_initialized) {
            if(!glfwInit()) {
                throw std::runtime_error("GLFW initialization failed");
            }
            std::atexit(glfwTerminate);
            glfw_initialized = true;
        }

        // Set window hints
        glfwWindowHint(GLFW_VISIBLE              , GLFW_FALSE              );
        glfwWindowHint(GLFW_RESIZABLE            , GLFW_FALSE              );
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3                       );
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3                       );
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE               );
        glfwWindowHint(GLFW_OPENGL_PROFILE       , GLFW_OPENGL_CORE_PROFILE);

        // Set error handler
        glfwSetErrorCallback(glfw_error_handler);

        // Create hidden window for OpenGL context
        GLFWwindow* const window = glfwCreateWindow(640, 480, "VbcRenderWindow", NULL, NULL);
        if(window == NULL) {
            throw std::runtime_error("could not create GLFW render window");
        }

        // Make context current
        glfwMakeContextCurrent(window);

        // Invoke GLAD to load OpenGL
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

        // Relinquish OpenGL context
        glfwMakeContextCurrent(NULL);

        return window;
    }
}


struct OpenGLRenderer::Data {
    std::mutex lock;
    std::mutex render_lock;
    std::condition_variable push_block;
    std::condition_variable pull_block;
    bool flush;

    size_t task_queue_memory;
    std::list<ProcessedTree> task_queue;

    GLFWwindow                     *window;
    std::shared_ptr<OpenGLShaders> shaders;
    GLuint                         fbo[2];
    GLuint                         rbo[2];
    GLuint                         vao;

    Data()
        : flush(false),
          task_queue_memory(0),
          window(NULL),
          fbo { 0, 0 },
          rbo { 0, 0 },
          vao(0)
    {}

    ~Data() {
        glDeleteVertexArrays(1, &vao);
        glDeleteRenderbuffers(2, rbo);
        glDeleteFramebuffers(2, fbo);
        shaders.reset();
        if(window != NULL) {
            glfwDestroyWindow(window);
        }
    }
};


OpenGLRenderer::OpenGLRenderer(size_t width, size_t height)
    : Renderer(width, height),
      d_(new Data())
{
    d_->window = create_opengl_context(width, height);
    glfwMakeContextCurrent(d_->window);

    glGenFramebuffers(2, d_->fbo);
    glGenRenderbuffers(2, d_->rbo);
    glGenVertexArrays(1, &d_->vao);

    glBindFramebuffer(GL_FRAMEBUFFER, d_->fbo[0]);
    glBindRenderbuffer(GL_RENDERBUFFER, d_->rbo[0]);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 8, GL_RGB8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, d_->rbo[0]);

    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER, d_->fbo[1]);
    glBindRenderbuffer(GL_RENDERBUFFER, d_->rbo[1]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, d_->rbo[1]);

    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glBindVertexArray(d_->vao);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    d_->shaders = std::make_shared<OpenGLShaders>();
    glfwMakeContextCurrent(NULL);
}


OpenGLRenderer::~OpenGLRenderer() {
    delete d_;
}


Renderer::PixelFormat OpenGLRenderer::get_pixel_format() const {
    return is_big_endian() ? Format_RGBx_8888 : Format_xBGR_8888;
}


Renderer::PushStatus OpenGLRenderer::push_frame(TreePtr tree, bool block) {
    // Data structure for processed tree
    ProcessedTree ptree;

    // Check whether we can pre-reject due to blocking
    {
        std::unique_lock<std::mutex> lock(d_->lock);
        if(d_->flush) {
            return Renderer::Push_Flush;
        }
        if(!block) {
            if(d_->task_queue_memory > (1 << 29)) {
                return Renderer::Push_Block;
            }
        }
    }

    // Calculate tree layout, store bounding box for scaling and translation
    tree->update_layout();
    ptree.bbox = tree->bounding_box();

    // Get tree's sequence index
    const auto& seq_idx = tree->seq_idx();

    // Reserve memory for vertex and index data
    ptree.vertices.reserve(2 * seq_idx.size());
    ptree.categories.reserve(seq_idx.size());
    ptree.line_indices.reserve(2 * (seq_idx.size() - 1));
    ptree.fill_indices.reserve(seq_idx.size());
    ptree.stroke_indices.reserve(seq_idx.size());

    // Iterate over all nodes
    for(auto it = seq_idx.cbegin(); it != seq_idx.cend(); ++it) {
        Node* node = it->get();
        if(!node) {
            ptree.vertices.push_back(0);
            ptree.vertices.push_back(0);
            ptree.categories.push_back(0);
        }
        else {
            ptree.vertices.push_back(GLfloat(node->x()));
            ptree.vertices.push_back(GLfloat(node->y()));
            ptree.categories.push_back(GLuint(node->category()));

            NodePtr parent = node->parent();
            if(parent) {
                ptree.line_indices.push_back(parent->seq());
                ptree.line_indices.push_back(node->seq());
            }

            if(node_style_table[node->category()].draw_filled) {
                ptree.fill_indices.push_back(node->seq());
            }
            else {
                ptree.stroke_indices.push_back(node->seq());
            }
        }
    }

    // Calculate memory footprint of processed tree
    ptree.memory = (3 + ptree.vertices.capacity()) * sizeof(GLfloat)
        + (ptree.categories.capacity() + ptree.line_indices.capacity() + ptree.fill_indices.capacity() + ptree.stroke_indices.capacity()) * sizeof(GLuint)
        + sizeof(size_t);

    {
        // Acquire a lock
        std::unique_lock<std::mutex> lock(d_->lock);

        // Reject all pushes when flushing
        if(d_->flush) {
            return Push_Flush;
        }

        // Block while memory limit (512 MiB) is exceeded
        if(block) {
            while(!d_->flush && d_->task_queue_memory > (1 << 29)) {
                d_->push_block.wait(lock);
            }
            if(d_->flush) {
                return Push_Flush;
            }
        }

        // Notify caller if tree can still not be pushed
        if(d_->task_queue_memory > (1 << 29)) {
            return Push_Block;
        }

        // Enqueue processed tree
        d_->task_queue_memory += ptree.memory;
        d_->task_queue.emplace_back(std::move(ptree));
        d_->pull_block.notify_all();

        return Push_Success;
    }
}


Renderer::PullStatus OpenGLRenderer::pull_frame(void *data, size_t size, bool block) {
    ProcessedTree ptree;

    // Verify sufficient size of buffer
    if(size < get_width() * get_height() * sizeof(uint32_t)) {
        throw std::logic_error("buffer too small");
    }

    // Fetch a processed tree from the task queue
    {
        std::unique_lock<std::mutex> lock(d_->lock);

        // Notify if queue is empty and flushing
        if(d_->flush && d_->task_queue.empty()) {
            return Pull_Flush;
        }

        // Block until queue is not empty or flush mode has been activated
        if(block) {
            while(!d_->flush && d_->task_queue.empty()) {
                d_->pull_block.wait(lock);
            }
        }

        // Notify caller if queue is still empty
        if(d_->task_queue.empty()) {
            return d_->flush ? Pull_Flush : Pull_Block;
        }

        ptree = std::move(d_->task_queue.front());
        d_->task_queue.pop_front();
        d_->task_queue_memory -= ptree.memory;
    }
    d_->push_block.notify_all();

    {
        // Acquire the render lock
        std::lock_guard<std::mutex> lock(d_->render_lock);

        // Acquire the OpenGL context associated with the renderer
        glfwMakeContextCurrent(d_->window);
        glEnable(GL_MULTISAMPLE);

        // Bind multisampling framebuffer for rendering
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, d_->fbo[0]);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        // Ensure proper viewport settings for transformation
        glViewport(0, 0, w_, h_);

        // Clear buffers
        glClearColor(background_color.r, background_color.g, background_color.b, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render tree
        draw_processed_tree(ptree, Rect { 0, 0, Scalar(w_), Scalar(h_) }, *d_->shaders);

        // Resolve multisampling by blitting to second framebuffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, d_->fbo[0]);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, d_->fbo[1]);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glBlitFramebuffer(0, 0, w_, h_, 0, 0, w_, h_, GL_COLOR_BUFFER_BIT, GL_LINEAR);

        // Read pixels from resolved framebuffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, d_->fbo[1]);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, w_, h_, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, data);

        // Relinquish control of OpenGL context
        glfwMakeContextCurrent(NULL);
    }

    // Indicate to caller if task has been completed
    return Pull_Success;
}


void OpenGLRenderer::flush(bool flush) {
    {
        std::unique_lock<std::mutex> lock(d_->lock);
        d_->flush = flush;
    }

    if(flush) {
        d_->push_block.notify_all();
        d_->pull_block.notify_all();
    }
}
