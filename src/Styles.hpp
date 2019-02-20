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

extern double tree_level_sep;
extern double tree_subtree_sep;
extern double tree_sibling_sep;
extern double tree_node_radius;
extern SkColor background_color;

extern std::vector<NodeStyle> node_style_table;
extern std::vector<EdgeStyle> edge_style_table;

#endif /* end of include guard: __VBC_STYLES_HPP */
