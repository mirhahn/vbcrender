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

#include "Types.hpp"

struct NodeStyle {
    Color       node_color;     ///< Color of node marker
    Color       font_color;     ///< Color of node text
    bool        draw_number;    ///< Indicates whether the sequence number should be drawn
    bool        draw_filled;    ///< Indicates whether the node marker should be filled
    bool        draw_circle;    ///< Indicates whether the node marker is a circle or square
    std::string name;           ///< Name of the style
};

struct EdgeStyle {
    Color edge_color;           ///< Color of the edge
};

extern Scalar tree_level_sep;   ///< Vertical separation between nodes on subsequent levels of the tree
extern Scalar tree_subtree_sep; ///< Horizontal separation between contour nodes in adjacent subtrees
extern Scalar tree_sibling_sep; ///< Horizontal separation between adjacent siblings
extern Scalar tree_node_radius; ///< Radius (or half of side length) of node markers
extern Color background_color;  ///< Background color

extern std::vector<NodeStyle> node_style_table; ///< Table of node styles
extern std::vector<EdgeStyle> edge_style_table; ///< Table of edge styles

#endif /* end of include guard: __VBC_STYLES_HPP */
