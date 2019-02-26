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

#ifndef __VBC_STYLES_HPP
#define __VBC_STYLES_HPP

#include <string>
#include <vector>

#include <SkColor.h>

struct NodeStyle {
    SkColor node_color;
    SkColor font_color;
    bool draw_number;
    bool draw_filled;
    bool draw_circle;
    std::string name;
};

struct EdgeStyle {
    SkColor edge_color;
};

extern SkScalar tree_level_sep;
extern SkScalar tree_subtree_sep;
extern SkScalar tree_sibling_sep;
extern SkScalar tree_node_radius;
extern SkColor background_color;

extern std::vector<NodeStyle> node_style_table;
extern std::vector<EdgeStyle> edge_style_table;

#endif /* end of include guard: __VBC_STYLES_HPP */
