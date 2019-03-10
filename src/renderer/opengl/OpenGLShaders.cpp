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
#include <cmath>
#include <exception>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <cmrc/cmrc.hpp>
#include <glad/glad.h>

#include "OpenGLShaders.hpp"
#include "../../Styles.hpp"

CMRC_DECLARE(shaders);


namespace {
    GLuint compile_shader_source(GLenum type, const std::vector<std::string>& sources) {
        std::vector<const GLchar*> code;
        std::vector<GLint> codelen;
        GLint status;

        // Create new shader
        const GLuint shader = glCreateShader(type);

        // Generate code and code length arrays
        code.reserve(sources.size());
        std::transform(sources.cbegin(), sources.cend(), std::back_inserter(code), [](const std::string& code_fragment) {
            return code_fragment.c_str();
        });

        codelen.reserve(sources.size());
        std::transform(sources.cbegin(), sources.cend(), std::back_inserter(codelen), [](const std::string& code_fragment) {
            return code_fragment.size();
        });

        // Attach source and compile shader
        glShaderSource(shader, sources.size(), code.data(), codelen.data());
        glCompileShader(shader);

        // Handle compilation errors
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if(status != GL_TRUE) {
            GLint info_log_length;
            GLchar* info_log;

            // Allocate buffer for shader info log
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
            info_log = new GLchar[info_log_length];

            // Fetch info log
            glGetShaderInfoLog(shader, info_log_length, &info_log_length, info_log);

            // Compose exception text
            std::ostringstream str;
            str << "GLSL shader compilation failed (see attached log)\n\n"
                   "Compilation Log:\n"
                   "----------------\n"
                << info_log;
            delete[] info_log;

            // Mark shader for deletion
            glDeleteShader(shader);

            // Throw error
            throw std::runtime_error(str.str());
        }

        return shader;
    }


    GLuint link_program(const std::vector<GLuint>& shaders) {
        // Create new program object
        const GLuint prog = glCreateProgram();

        // Attach shaders
        for(GLuint shader : shaders) {
            glAttachShader(prog, shader);
        }

        // Link program
        glLinkProgram(prog);

        // Variables for error handling
        GLint status, info_log_len;
        GLchar *info_log;

        // Handle linker errors
        glGetProgramiv(prog, GL_LINK_STATUS, &status);
        if(status != GL_TRUE) {
            // Fetch info log
            glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &info_log_len);
            info_log = new GLchar[info_log_len];
            glGetProgramInfoLog(prog, info_log_len, &info_log_len, info_log);

            // Destroy program object
            glDeleteProgram(prog);

            // Write exception message
            std::ostringstream str;
            str << "GLSL program linking failed (see attached log)\n\n"
                   "Linker log:\n"
                   "-----------\n"
                << info_log;
            delete[] info_log;

            // Throw exception
            throw std::runtime_error(str.str());
        }

        // Validate resulting program
        glValidateProgram(prog);

        // Handle validation errors
        glGetProgramiv(prog, GL_VALIDATE_STATUS, &status);
        if(status != GL_TRUE) {
            // Fetch info log
            glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &info_log_len);
            info_log = new GLchar[info_log_len];
            glGetProgramInfoLog(prog, info_log_len, &info_log_len, info_log);

            // Destroy program object
            glDeleteProgram(prog);

            // Write exception message
            std::ostringstream str;
            str << "GLSL program validation failed (see attached log)\n\n"
                   "Validation log:\n"
                   "---------------\n"
                << info_log;
            delete[] info_log;

            // Throw exception
            throw std::runtime_error(str.str());
        }

        return prog;
    }
}

OpenGLShaders::OpenGLShaders()
    : prog_ { 0, 0, 0 },
      buf_(0)
{
    build_shaders();
    bind_buffers();
}


OpenGLShaders::~OpenGLShaders()
{
    glDeleteBuffers(1, &buf_);
    glDeleteProgram(prog_[0]);
    glDeleteProgram(prog_[1]);
    glDeleteProgram(prog_[2]);
}


void OpenGLShaders::build_shaders() {
    GLint returncode;

    // Short-circuit if all shader programs already exist
    if(prog_[0] && prog_[1] && prog_[2]) {
        return;
    }

    // Write headers for shaders
    std::ostringstream header;
    header << "#version 330\n"
              "#define NUM_SHAPES 2u\n"
              "#define NUM_VERT 64u\n"
              "#define NUM_VERT_LOOP 65u\n"
              "#define NUM_STYLES " << node_style_table.size() << "u\n";
    std::string geom_hdr_str = header.str();

    // Load shader code from resource files
    auto rc_fs = cmrc::shaders::get_filesystem();
    auto transform_source   = rc_fs.open("transform.vert"   );
    auto marker_source      = rc_fs.open("marker.geom"      );
    auto color_source       = rc_fs.open("color.frag"       );

    const std::vector<std::string> marker_transform_shader_code {
        "#version 330\n"
        "#define SKIP_GEOM 0\n",
        std::string(transform_source.begin(), transform_source.end())
    };
    const std::vector<std::string> line_transform_shader_code {
        "#version 330\n"
        "#define SKIP_GEOM 1\n",
        std::string(transform_source.begin(), transform_source.end())
    };
    const std::vector<std::string> stroke_shader_code {
        geom_hdr_str,
        "#define PRIM_TYPE line_strip\n"
        "#define CLOSE_LOOP 1\n",
        std::string(marker_source.begin(), marker_source.end())
    };
    const std::vector<std::string> fill_shader_code {
        geom_hdr_str,
        "#define PRIM_TYPE triangle_strip\n"
        "#define CLOSE_LOOP 0\n",
        std::string(marker_source.begin(), marker_source.end())
    };
    const std::vector<std::string> color_shader_code {
        std::string(color_source.begin(), color_source.end())
    };

    // Initialize shader object names to 0
    GLuint marker_transform_shader = 0;
    GLuint line_transform_shader   = 0;
    GLuint stroke_shader           = 0;
    GLuint fill_shader             = 0;
    GLuint color_shader            = 0;

    std::exception_ptr eptr;
    try {
        // Build individual shaders
        marker_transform_shader = compile_shader_source(GL_VERTEX_SHADER,   marker_transform_shader_code);
        line_transform_shader   = compile_shader_source(GL_VERTEX_SHADER,   line_transform_shader_code  );
        stroke_shader           = compile_shader_source(GL_GEOMETRY_SHADER, stroke_shader_code          );
        fill_shader             = compile_shader_source(GL_GEOMETRY_SHADER, fill_shader_code            );
        color_shader            = compile_shader_source(GL_FRAGMENT_SHADER, color_shader_code           );

        // Link stroke program
        if(!prog_[Program_Stroke]) {
            prog_[Program_Stroke] = link_program({ marker_transform_shader, stroke_shader, color_shader });
        }

        // Link fill program
        if(!prog_[Program_Fill]) {
            prog_[Program_Fill] = link_program({ marker_transform_shader, fill_shader  , color_shader });
        }

        // Link line program
        if(!prog_[Program_Line]) {
            prog_[Program_Line] = link_program({ line_transform_shader  ,                color_shader });
        }
    } catch(...) {
        eptr = std::current_exception();
    }

    // Delete shaders
    if(marker_transform_shader) glDeleteShader(marker_transform_shader);
    if(line_transform_shader  ) glDeleteShader(line_transform_shader  );
    if(stroke_shader          ) glDeleteShader(stroke_shader          );
    if(fill_shader            ) glDeleteShader(fill_shader            );
    if(color_shader           ) glDeleteShader(color_shader           );

    // Rethrow exception if any was caught
    if(eptr) {
        std::rethrow_exception(eptr);
    }
}


void OpenGLShaders::create_interface_block_bindings(GLuint program, const std::vector<std::pair<const GLchar*, Block>>& blockNames, const std::vector<std::pair<const GLchar*, Uniform>>& varNames, GLint& size_acc) {
    // Determine offset alignment for buffer extensions
    GLint offs_align;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &offs_align);

    // Find named uniform blocks in given program
    for(const auto& blockDesc : blockNames) {
        const GLchar* name;
        Block blk_id;
        std::tie(name, blk_id) = blockDesc;

        // Find block index
        GLuint blk_idx = glGetUniformBlockIndex(program, name);
        if(blk_idx == GL_INVALID_INDEX) {
            continue;
        }

        // Bind block to designated binding point
        glUniformBlockBinding(program, blk_idx, blk_id + 1);

        // Skip blocks that have already been found
        if(boff_[blk_id] >= 0) {
            continue;
        }

        // Detect block size
        glGetActiveUniformBlockiv(program, blk_idx, GL_UNIFORM_BLOCK_DATA_SIZE, bsize_ + blk_id);

        // Append new interface block to end of uniform storage block
        boff_[blk_id] = size_acc;
        if(boff_[blk_id] % offs_align) {
            boff_[blk_id] = ((boff_[blk_id] + offs_align) / offs_align) * offs_align;
        }
        size_acc = boff_[blk_id] + bsize_[blk_id];
    }

    // Find named uniforms in given program
    for(const auto& varDesc : varNames) {
        const GLchar* name;
        Uniform var_id;
        std::tie(name, var_id) = varDesc;

        // Skip variables that have already been found
        if(voff_[var_id] >= 0) {
            continue;
        }

        // Find active uniform index
        GLuint var_idx;
        glGetUniformIndices(program, 1, &name, &var_idx);
        if(var_idx == GL_INVALID_INDEX) {
            continue;
        }

        // Fetch uniform parameters
        glGetActiveUniformsiv(program, 1, &var_idx, GL_UNIFORM_OFFSET, voff_ + var_id);
        glGetActiveUniformsiv(program, 1, &var_idx, GL_UNIFORM_ARRAY_STRIDE, vstr_ + var_id);
    }
}


void OpenGLShaders::bind_buffers() {
    // Reset buffer and index arrays
    glDeleteBuffers(1, &buf_); buf_ = 0;
    std::fill_n(boff_, Block_Count, -1);
    std::fill_n(bsize_, Block_Count, -1);
    std::fill_n(voff_, Uniform_Count, -1);
    std::fill_n(vstr_, Uniform_Count, -1);

    // Accumulate buffer size while calculating block offsets, block layouts, and assigning binding points
    GLint buffer_size = 0;
    create_interface_block_bindings(prog_[Program_Stroke], {
            { "TransformBlock", Block_Transform   },
            { "NodeBlock"     , Block_NodeStyle   },
            { "ShapeBlock"    , Block_StrokeShape },
        }, {
            { "scale"              , Uniform_Transform_Scale       },
            { "translate"          , Uniform_Transform_Translate   },
            { "shape_table"        , Uniform_NodeStyle_ShapeTable  },
            { "color_table"        , Uniform_NodeStyle_ColorTable  },
            { "ShapeBlock.num_vert", Uniform_StrokeShape_NumVertex },
            { "ShapeBlock.rel_pos" , Uniform_StrokeShape_RelPos    },
        },
        buffer_size
    );
    create_interface_block_bindings(prog_[Program_Fill], {
            { "TransformBlock", Block_Transform },
            { "NodeBlock"     , Block_NodeStyle },
            { "ShapeBlock"    , Block_FillShape },
        }, {
            { "scale"              , Uniform_Transform_Scale      },
            { "translate"          , Uniform_Transform_Translate  },
            { "shape_table"        , Uniform_NodeStyle_ShapeTable },
            { "color_table"        , Uniform_NodeStyle_ColorTable },
            { "ShapeBlock.num_vert", Uniform_FillShape_NumVertex  },
            { "ShapeBlock.rel_pos" , Uniform_FillShape_RelPos     },
        },
        buffer_size
    );
    create_interface_block_bindings(prog_[Program_Line], {
            { "TransformBlock", Block_Transform       },
            { "EdgeBlock"     , Block_EdgeStyle       },
        }, {
            { "scale"     , Uniform_Transform_Scale     },
            { "translate" , Uniform_Transform_Translate },
            { "edge_color", Uniform_EdgeStyle_Color     },
        },
        buffer_size
    );

    // Create new uniform buffer
    glGenBuffers(1, &buf_);
    glBindBuffer(GL_UNIFORM_BUFFER, buf_);

    // Allocate memory for initial buffer contents
    GLbyte* data = new GLbyte[buffer_size];
    std::fill_n(data, buffer_size, 0);

    // Generate constant data
    if(boff_[Block_EdgeStyle] >= 0 && voff_[Uniform_EdgeStyle_Color]) {
        const EdgeStyle& style = edge_style_table.back();
        GLfloat* ecolor = reinterpret_cast<GLfloat*>(data + boff_[Block_EdgeStyle] + voff_[Uniform_EdgeStyle_Color]);
        ecolor[0] = style.edge_color.r;
        ecolor[1] = style.edge_color.g;
        ecolor[2] = style.edge_color.b;
    }

    if(boff_[Block_NodeStyle]) {
        if(voff_[Uniform_NodeStyle_ShapeTable]) {
            for(size_t i = 0; i < node_style_table.size(); ++i) {
                GLuint* shape = reinterpret_cast<GLuint*>(data + boff_[Block_NodeStyle] + voff_[Uniform_NodeStyle_ShapeTable] + i * vstr_[Uniform_NodeStyle_ShapeTable]);
                *shape = node_style_table[i].draw_circle ? 0 : 1;
            }
        }
        if(voff_[Uniform_NodeStyle_ColorTable]) {
            for(size_t i = 0; i < node_style_table.size(); ++i) {
                Color node_color = node_style_table[i].node_color;
                GLfloat* color = reinterpret_cast<GLfloat*>(data + boff_[Block_NodeStyle] + voff_[Uniform_NodeStyle_ColorTable] + i * vstr_[Uniform_NodeStyle_ColorTable]);
                color[0] = node_color.r;
                color[1] = node_color.g;
                color[2] = node_color.b;
            }
        }
    }

    // Initialize storage for uniform object buffer
    glBufferData(GL_UNIFORM_BUFFER, buffer_size, data, GL_DYNAMIC_DRAW);
    delete[] data;

    // Bind buffer sections to binding points
    for(Block block : { Block_Transform, Block_EdgeStyle, Block_NodeStyle, Block_StrokeShape, Block_FillShape }) {
        if(boff_[block] >= 0) {
            glBindBufferRange(GL_UNIFORM_BUFFER, block + 1, buf_, boff_[block], bsize_[block]);
        }
    }
}


void OpenGLShaders::use_line_program() {
    glUseProgram(prog_[Program_Line]);
}


void OpenGLShaders::use_fill_program() {
    glUseProgram(prog_[Program_Fill]);
}


void OpenGLShaders::use_stroke_program() {
    glUseProgram(prog_[Program_Stroke]);
}


void OpenGLShaders::set_transform(GLfloat scale, GLfloat xtrans, GLfloat ytrans) {
    GLfloat xscale, yscale;
    GLint viewport[4];

    if(boff_[Block_Transform] < 0) {
        return;
    }

    // Get viewport information
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Apply new transform to uniform storage buffer
    GLbyte* transform_buffer = (GLbyte*)glMapBufferRange(GL_UNIFORM_BUFFER, boff_[Block_Transform], bsize_[Block_Transform], GL_MAP_WRITE_BIT);
    if(voff_[Uniform_Transform_Scale] >= 0) {
        GLfloat* var_scale = reinterpret_cast<GLfloat*>(transform_buffer + voff_[Uniform_Transform_Scale]);
        var_scale[0] = 2 * scale / viewport[2];
        var_scale[1] = 2 * scale / viewport[3];
    }
    if(voff_[Uniform_Transform_Translate] >= 0) {
        GLfloat* var_transl = reinterpret_cast<GLfloat*>(transform_buffer + voff_[Uniform_Transform_Translate]);
        var_transl[0] = 2 * (xtrans + viewport[0]) / viewport[2] - 1;
        var_transl[1] = 2 * (ytrans + viewport[1]) / viewport[3] - 1;
    }
    glUnmapBuffer(GL_UNIFORM_BUFFER);
}


void OpenGLShaders::update_shapes(GLfloat radius, GLuint segments) {
    GLbyte *map;
    double ang_step;

    // Impose cap on number of segments
    if(segments > 64) {
        segments = 64;
    }

    // Calculate angular step per segment
    ang_step = 2 * M_PI / segments;

    // Map uniform shape block for filled markers
    if(boff_[Block_FillShape] >= 0) {
        map = (GLbyte*)glMapBufferRange(GL_UNIFORM_BUFFER, boff_[Block_FillShape], bsize_[Block_FillShape], GL_MAP_WRITE_BIT);
        if(voff_[Uniform_FillShape_NumVertex] >= 0) {
            GLbyte* map_array = map + voff_[Uniform_FillShape_NumVertex];
            GLint map_stride = vstr_[Uniform_FillShape_NumVertex];
            *reinterpret_cast<GLuint*>(map_array) = segments;
            *reinterpret_cast<GLuint*>(map_array + map_stride) = segments;
        }
        if(voff_[Uniform_FillShape_RelPos] >= 0) {
            GLbyte* map_array = map + voff_[Uniform_FillShape_RelPos];
            GLint map_stride = vstr_[Uniform_FillShape_RelPos];
            for(GLuint i = 0; i < segments / 2; ++i) {
                GLfloat* map_vec2 = reinterpret_cast<GLfloat*>(map_array + 2 * i * map_stride);
                map_vec2[0] = GLfloat(radius * std::cos(ang_step * i));
                map_vec2[1] = GLfloat(-radius * std::sin(ang_step * i));

                map_vec2 = reinterpret_cast<GLfloat*>(map_array + (2 * i + 1) * map_stride);
                map_vec2[0] = GLfloat(radius * std::cos(ang_step * (i + 1)));
                map_vec2[1] = GLfloat(radius * std::sin(ang_step * (i + 1)));
            }
            if(segments % 2) {
                GLfloat* map_vec2 = reinterpret_cast<GLfloat*>(map_array + (segments - 1) * map_stride);
                map_vec2[0] = GLfloat(radius * std::cos(ang_step * (segments - 1)));
                map_vec2[1] = GLfloat(-radius * std::sin(ang_step * (segments - 1)));
            }

            map_array += map_stride * 64;
            GLfloat* map_vec2 = reinterpret_cast<GLfloat*>(map_array);
            map_vec2[0] = radius;
            map_vec2[1] = radius;

            map_vec2 = reinterpret_cast<GLfloat*>(map_array + map_stride);
            map_vec2[0] = radius;
            map_vec2[1] = -radius;

            map_vec2 = reinterpret_cast<GLfloat*>(map_array + 2 * map_stride);
            map_vec2[0] = -radius;
            map_vec2[1] = radius;

            map_vec2 = reinterpret_cast<GLfloat*>(map_array + 3 * map_stride);
            map_vec2[0] = -radius;
            map_vec2[1] = -radius;
        }
        glUnmapBuffer(GL_UNIFORM_BUFFER);
    }

    // Map uniform shape block for filled markers
    if(boff_[Block_StrokeShape] >= 0) {
        map = (GLbyte*)glMapBufferRange(GL_UNIFORM_BUFFER, boff_[Block_StrokeShape], bsize_[Block_StrokeShape], GL_MAP_WRITE_BIT);
        if(voff_[Uniform_StrokeShape_NumVertex] >= 0) {
            GLbyte* map_array = map + voff_[Uniform_StrokeShape_NumVertex];
            GLint map_stride = vstr_[Uniform_StrokeShape_NumVertex];
            *reinterpret_cast<GLuint*>(map_array) = segments;
            *reinterpret_cast<GLuint*>(map_array + map_stride) = segments;
        }
        if(voff_[Uniform_StrokeShape_RelPos] >= 0) {
            GLbyte* map_array = map + voff_[Uniform_StrokeShape_RelPos];
            GLint map_stride = vstr_[Uniform_StrokeShape_RelPos];
            for(GLuint i = 0; i < segments; ++i) {
                GLfloat* map_vec2 = reinterpret_cast<GLfloat*>(map_array + i * map_stride);
                map_vec2[0] = GLfloat(radius * std::cos(ang_step * i));
                map_vec2[1] = GLfloat(radius * std::sin(ang_step * i));
            }

            map_array += map_stride * 64;
            GLfloat* map_vec2 = reinterpret_cast<GLfloat*>(map_array);
            map_vec2[0] = radius;
            map_vec2[1] = radius;

            map_vec2 = reinterpret_cast<GLfloat*>(map_array + map_stride);
            map_vec2[0] = radius;
            map_vec2[1] = -radius;

            map_vec2 = reinterpret_cast<GLfloat*>(map_array + 2 * map_stride);
            map_vec2[0] = -radius;
            map_vec2[1] = -radius;

            map_vec2 = reinterpret_cast<GLfloat*>(map_array + 3 * map_stride);
            map_vec2[0] = -radius;
            map_vec2[1] = radius;
        }
        glUnmapBuffer(GL_UNIFORM_BUFFER);
    }
}
