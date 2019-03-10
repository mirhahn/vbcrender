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

#ifndef __VBC_RENDER_RENDERER_HPP
#define __VBC_RENDER_RENDERER_HPP

#include <functional>
#include <memory>
#include <string>

#include "Tree.hpp"


class                                               Renderer;
typedef std::shared_ptr<Renderer>                   RendererPtr;
typedef std::shared_ptr<const Renderer>             ConstRendererPtr;

typedef std::function<RendererPtr (size_t, size_t)> RendererFactory;


void register_renderer_factory(const std::string& name, RendererFactory factory);
void unregister_renderer_factory(const std::string& name);
RendererPtr create_renderer(size_t width, size_t height, const std::string& name = std::string());


class Renderer {
public:
    enum PixelFormat {
        Format_RGBx_8888,   ///< 32 bit RGBx with 8 bits each (big endian)
        Format_xBGR_8888,   ///< 32 bit RGBx with 8 bits each (little endian)
        Format_xRGB_8888,   ///< 32 bit xRGB with 8 bits each (big endian)
        Format_BGRx_8888,   ///< 32 bit xRGB with 8 bits each (little endian)
        Format_RGBA_8888,   ///< 32 bit RGBA with 8 bits each (big endian)
        Format_ABGR_8888,   ///< 32 bit RGBA with 8 bits each (little endian)
        Format_ARGB_8888,   ///< 32 bit ARGB with 8 bits each (big endian)
        Format_BGRA_8888,   ///< 32 bit ARGB with 8 bits each (little endian)
    };

    enum PushStatus {
        Push_Success,       ///< Tree has been successfully pushed
        Push_Block,         ///< Push would have blocked but was not allowed to
        Push_Flush,         ///< Renderer is in flush mode
    };

    enum PullStatus {
        Pull_Success,       ///< Rendered frame has been successfully pulled
        Pull_Block,         ///< Pull would have blocked but was not allowed to
        Pull_Flush,         ///< Renderer is in flush mode and queue is empty
    };

protected:
    size_t w_;                                                                  ///< Width of output images
    size_t h_;                                                                  ///< Height of output images

public:
    Renderer(size_t width, size_t height) : w_(width), h_(height) {}
    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;

    Renderer& operator=(const Renderer&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    size_t get_width() const { return w_; }                                         ///< Returns width of rendered images.
    size_t get_height() const { return h_; }                                        ///< Returns height of rendered images.
    virtual PixelFormat get_pixel_format() const = 0;                               ///< Returns pixel format returned by this renderer.
    virtual void flush(bool flush) = 0;                                             ///< Switches flush mode on or off.
    virtual PushStatus push_frame(TreePtr tree, bool block = false) = 0;            ///< Adds a render task to the queue (may block).
    virtual PullStatus pull_frame(void* data, size_t size, bool block = false) = 0; ///< Pulls rendered image from output (may block).
};

#endif /* end of include guard: __VBC_RENDER_RENDERER_HPP */
