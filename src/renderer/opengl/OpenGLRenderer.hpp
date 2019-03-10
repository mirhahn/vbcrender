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

#ifndef __VBC_RENDER_OPENGL_RENDERER_HPP
#define __VBC_RENDER_OPENGL_RENDERER_HPP

#include <memory>

#include "../../Renderer.hpp"


class OpenGLRenderer : public Renderer, public std::enable_shared_from_this<OpenGLRenderer> {
private:
    class Data;                         ///< Opaque data structure for internal data
    Data* d_;                           ///< Internal data

public:
    OpenGLRenderer(size_t width, size_t height);
    ~OpenGLRenderer();

    virtual PixelFormat get_pixel_format() const;
    virtual void flush(bool flush);
    virtual PushStatus push_frame(TreePtr tree, bool block = false);
    virtual PullStatus pull_frame(void* data, size_t size, bool block = false);
};

#endif /* end of include guard: __VBC_RENDER_OPENGL_RENDERER_HPP */
