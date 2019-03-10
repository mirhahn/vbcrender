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

#ifndef __VBC_RENDER_OPENGL_SHADERS_HPP
#define __VBC_RENDER_OPENGL_SHADERS_HPP

#include <vector>

#include <glad/glad.h>

/**
 * \brief Convenience object managing OpenGL shaders.
 *
 * This object compiles, links, and subsequently manages the three main shader
 * programs used by VbcRender's OpenGL renderer:
 *
 * * `stroke` renders unfilled node markers (both circles and squares),
 * * `fill` renders filled node markers (both circles and squares),
 * * `line` renders edges.
 *
 * All three programs expect the following bound vertex arrays:
 *
 * * untransformed vertex position (`vec2` at 0),
 * * vertex color (`vec3` at 1),
 * * shape index (`uint` at 2).
 *
 * The output is a single `vec4` color in draw buffer 0. Drawing is defined by
 * several uniform parameters which can be set using this class.
 *
 * All calls to an instance of this class must be made with the same active
 * OpenGL context.
 */
class OpenGLShaders {
public:
    enum Program {
        Program_Stroke = 0,
        Program_Fill   = 1,
        Program_Line   = 2,

        Program_Count  = 3
    };

    enum Block {
        Block_Transform   = 0,                  ///< TransformBlock
        Block_EdgeStyle   = 1,                  ///< EdgeBlock
        Block_NodeStyle   = 2,                  ///< NodeBlock
        Block_FillShape   = 3,                  ///< ShapeBlock[0] in fill program
        Block_StrokeShape = 4,                  ///< ShapeBlock[1] in fill program

        Block_Count       = 5                   ///< Number of uniform blocks
    };

    enum Uniform {
        Uniform_Transform_Scale,                ///< TransformBlock.scale     [vec2]
        Uniform_Transform_Translate,            ///< TransformBlock.translate [vec2]

        Uniform_EdgeStyle_Color,                ///< EdgeBlock.color          [vec3]

        Uniform_NodeStyle_ShapeTable,           ///< NodeBlock.shape_table    [uint[NUM_STYLES]]
        Uniform_NodeStyle_ColorTable,           ///< NodeBlock.color_table    [vec3[NUM_STYLES]]

        Uniform_FillShape_NumVertex,            ///< ShapeBlock.num_vert      [uint]
        Uniform_FillShape_RelPos,               ///< ShapeBlock.rel_pos       [vec2[NUM_VERT]]
        Uniform_StrokeShape_NumVertex,          ///< ShapeBlock.num_vert      [uint]
        Uniform_StrokeShape_RelPos,             ///< ShapeBlock.rel_pos       [vec2[NUM_VERT]]

        Uniform_Count                           ///< Number of uniforms
    };

private:
    GLuint prog_[3];                            ///< Shader programs
    GLuint buf_;                                ///< Uniform block buffer

    GLint boff_[Block_Count];                   ///< Buffer offsets for uniform blocks
    GLint bsize_[Block_Count];                  ///< Sizes of uniform blocks
    GLint voff_[Uniform_Count];                 ///< Buffer offsets for uniform variables
    GLint vstr_[Uniform_Count];                 ///< Strides for uniform arrays

    void build_shaders();
    void bind_buffers();
    void create_interface_block_bindings(GLuint program, const std::vector<std::pair<const GLchar*, Block>>& blockNames, const std::vector<std::pair<const GLchar*, Uniform>>& varNames, GLint& size_acc);

public:
    OpenGLShaders();
    OpenGLShaders(const OpenGLShaders&) = delete;
    OpenGLShaders(OpenGLShaders&&) = delete;
    ~OpenGLShaders();

    OpenGLShaders& operator=(const OpenGLShaders&) = delete;
    OpenGLShaders& operator=(OpenGLShaders&&) = delete;

    void use_stroke_program();
    void use_fill_program();
    void use_line_program();

    void set_transform(GLfloat scale, GLfloat xtrans, GLfloat ytrans);
    void update_shapes(GLfloat radius, GLuint segments);
};

#endif /* end of include guard: __VBC_RENDER_OPENGL_SHADERS_HPP */
