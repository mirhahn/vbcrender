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

#include <unordered_map>

#include "Renderer.hpp"
#include "renderer/opengl/OpenGLRenderer.hpp"


static std::unordered_map<std::string, RendererFactory> factory_map {
    { "opengl", [](size_t w, size_t h) { return std::make_shared<OpenGLRenderer>(w, h); } }
};
static std::string default_factory = "opengl";


void register_renderer_factory(const std::string& name, RendererFactory factory) {
    if(!name.empty() && factory) {
        factory_map[name] = factory;
        if(default_factory.empty()) {
            default_factory = name;
        }
    }
}


void unregister_renderer_factory(const std::string& name) {
    auto it = factory_map.find(name);
    if(it != factory_map.end()) {
        factory_map.erase(it);
        if(default_factory == name) {
            if(factory_map.empty()) {
                default_factory.clear();
            }
            else {
                default_factory = factory_map.cbegin()->first;
            }
        }
    }
}


RendererPtr create_renderer(size_t width, size_t height, const std::string& name) {
    const RendererFactory& factory = factory_map.at(name.empty() ? default_factory : name);
    return factory(width, height);
}
