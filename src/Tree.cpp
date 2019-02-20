#include <SkPaint.h>

#include "Styles.hpp"
#include "Tree.hpp"

Node::Node(size_t seqnum, NodePtr  parent, size_t category)
    : seqnum(seqnum),
      category(category),
      parent(parent),
      main_info(),
      general_info()
{
    // Calculate depth of node
    depth = parent ? parent->get_depth() + 1 : 0;
}


void Node::set_info(const std::string& main, const std::string& general) {
    main_info = main;
    general_info = general;
}


void Node::add_info(const std::string& main, const std::string& general) {
    main_info += main;
    general_info += general;
}


void Node::strip_info(const std::string& main, const std::string& general) {
    main_info = main_info.substr(0, main_info.size() - main.size());
    general_info = general_info.substr(0, general_info.size() - general.size());
}


Edge::Edge(NodePtr parent, NodePtr child)
    : parent(parent),
      child(child)
{}


Tree::Tree()
    : upper_bound(std::numeric_limits<double>::infinity()),
      lower_bound(-std::numeric_limits<double>::infinity()),
      category_index(node_style_table.size()),
      node_points(node_style_table.size()),
      layout_stale(true),
      bounding_box()
{}


void Tree::add_node(size_t seqnum, size_t parent_seqnum, size_t category) {
    // Validate data
    if(!seqnum) {
        throw std::out_of_range("invalid node sequence number");
    }
    else if(category >= category_index.size()) {
        throw std::out_of_range("invalid node cateogry");
    }

    // Find parent node
    NodePtr parent = parent_seqnum ? nodes.at(offset_table.at(parent_seqnum)) : nullptr;

    // Create new node object and insert it
    NodePtr node = std::make_shared<Node>(seqnum, parent, category);
    size_t offset = nodes.size();
    nodes.push_back(node);

    // Insert node into offset table
    if(offset_table.size() <= seqnum) {
        offset_table.resize(seqnum + 1, std::numeric_limits<size_t>::max());
    }
    offset_table[seqnum] = offset;

    // Insert node into depth index
    if(depth_index.size() <= node->get_depth()) {
        depth_index.resize(node->get_depth() + 1);
    }
    node->depth_offset = depth_index[node->get_depth()].size();
    depth_index[node->get_depth()].push_back(offset);

    // Insert node into category index
    node->category_offset = category_index[category].size();
    category_index[category].push_back(offset);

    // Add coordinate structure
    node_points[category].emplace_back();

    // Create edge if necessary
    if(parent) {
        edges.push_back(std::make_shared<Edge>(parent, node));
        edge_points.emplace_back();
        edge_points.emplace_back();
    }

    // Mark layout as stale
    layout_stale = true;
}


void Tree::remove_node(size_t seqnum) {
    // Check whether node exists
    if(seqnum >= offset_table.size() || offset_table[seqnum] >= nodes.size()) {
        throw std::out_of_range("invalid node sequence number");
    }

    // Determine offset
    const size_t offset = offset_table[seqnum];
    NodePtr node = nodes[offset];

    // Remove node from list and offset table
    for(auto cit = nodes.erase(std::next(nodes.cbegin(), offset)); cit != nodes.cend(); ++cit) {
        NodePtr node = *cit;
        --offset_table[node->seqnum];
        --depth_index[node->depth][node->depth_offset];
        --category_index[node->category][node->category_offset];
    }
    offset_table[seqnum] = std::numeric_limits<size_t>::max();

    // Remove node from depth index, category index, and node coordinates
    auto& dindex = depth_index[node->depth];
    for(auto cit = dindex.erase(std::next(dindex.begin(), node->depth_offset)); cit != dindex.cend(); ++cit) {
        --nodes[*cit]->depth_offset;
    }

    auto& cindex = category_index[node->category];
    for(auto cit = cindex.erase(std::next(cindex.begin(), node->category_offset)); cit != cindex.cend(); ++cit) {
        --nodes[*cit]->category_offset;
    }

    node_points[node->category].pop_back();

    // Remove all edges attached to the node
    for(auto cit = edges.cbegin(); cit != edges.cend();) {
        if((*cit)->child == node) {
            cit = edges.erase(cit);
        }
        else if((*cit)->parent == node) {
            throw std::logic_error("cannot erase entire subtree");
        }
        else {
            ++cit;
        }
    }
    edge_points.resize(edges.size());

    // Mark layout as stale
    layout_stale = true;
}


void Tree::set_category(size_t seqnum, size_t category) {
    // Validate category code
    if(category >= category_index.size()) {
        throw std::out_of_range("invalid node category code");
    }

    // Find node structure
    size_t offset = offset_table.at(seqnum);
    NodePtr node = nodes.at(offset);
    if(category == node->category) {
        return;
    }

    // Remove node from category index
    auto& old_index = category_index[node->category];
    for(auto cit = old_index.erase(std::next(old_index.cbegin(), node->category_offset)); cit != old_index.cend(); ++cit) {
        --nodes[*cit]->category_offset;
    }
    node_points[node->category].pop_back();

    // Insert node into new location in category index
    auto& new_index = category_index[category];
    node->category = category;
    node->category_offset = new_index.size();
    new_index.push_back(offset);
    node_points[category].emplace_back();
}


void Tree::update_layout() {
    // Short-circuit if there are no levels
    if(depth_index.empty() || !layout_stale) {
        return;
    }

    // Reset subtree widths on all nodes
    for(NodePtr node : nodes) {
        node->child_count = 0;
        node->subtree_width = 0.0;
        node->allocated_width = 0.0;
    }

    // Propagate widths upwards
    for(auto lit = depth_index.crbegin(); lit != std::prev(depth_index.crend()); ++lit) {
        for(size_t offset : *lit) {
            NodePtr node = nodes[offset];
            if(node->child_count) {
                node->subtree_width += (node->child_count - 1) * tree_subtree_sep;
            }
            node->subtree_width = std::max(2 * tree_node_radius, node->subtree_width);

            ++node->parent->child_count;
            node->parent->subtree_width += node->subtree_width;
        }
    }

    // Compute widths on the root level
    double root_width = 0.0;
    for(size_t offset : depth_index.front()) {
        NodePtr node = nodes[offset];
        if(node->child_count) {
            node->subtree_width += (node->child_count - 1) * tree_subtree_sep;
        }
        node->subtree_width = std::max(2 * tree_node_radius, node->subtree_width);
        root_width += node->subtree_width;
    }
    if(!depth_index.front().empty()) {
        root_width += (depth_index.front().size() - 1) * tree_subtree_sep;
    }

    // Reset bounding box
    bounding_box.fTop = std::numeric_limits<SkScalar>::infinity();
    bounding_box.fBottom = -std::numeric_limits<SkScalar>::infinity();
    bounding_box.fRight = -std::numeric_limits<SkScalar>::infinity();
    bounding_box.fLeft = std::numeric_limits<SkScalar>::infinity();

    // Compute positions on the root level
    double x = -0.5 * root_width;
    for(size_t offset : depth_index.front()) {
        NodePtr node = nodes[offset];
        x += 0.5 * node->subtree_width;
        node_points[node->category][node->category_offset] = { (float)x, 0.0 };
        bounding_box.join(
            float(x - tree_node_radius),
            float(-tree_node_radius),
            float(x + tree_node_radius),
            float(tree_node_radius)
        );
        x += 0.5 * node->subtree_width + tree_subtree_sep;
    }

    // Propagate positions downwards
    for(size_t level = 1; level < depth_index.size(); ++level) {
        const double y = (2 * tree_node_radius + tree_level_sep) * level;
        for(size_t offset : depth_index[level]) {
            NodePtr node = nodes[offset];
            NodePtr parent = node->parent;

            double x = node_points[parent->category][parent->category_offset].fX - 0.5 * parent->subtree_width + parent->allocated_width + 0.5 * node->subtree_width;
            node_points[node->category][node->category_offset] = { (float)x, (float)y };
            bounding_box.join(
                float(x - tree_node_radius),
                float(y - tree_node_radius),
                float(x + tree_node_radius),
                float(y + tree_node_radius)
            );
            parent->allocated_width += node->subtree_width + tree_subtree_sep;
        }
    }

    // Copy node positions to edge coordinates
    auto coord_it = edge_points.begin();
    for(EdgePtr edge : edges) {
        *coord_it++ = node_points[edge->parent->category][edge->parent->category_offset];
        *coord_it++ = node_points[edge->child->category][edge->child->category_offset];
    }

    // Mark layout as fresh
    layout_stale = false;
}


void Tree::draw(SkCanvas* canvas) const {
    // Draw edges
    SkPaint edge_paint;
    edge_paint.setColor(edge_style_table.front().edge_color);
    canvas->drawPoints(SkCanvas::kLines_PointMode, edge_points.size(), edge_points.data(), edge_paint);

    // Draw nodes
    for(size_t category = 0; category < node_points.size(); ++category) {
        const auto& points = node_points[category];
        const auto& style = node_style_table[category];

        if(!points.empty()) {
            SkPaint paint;
            paint.setColor(style.node_color);
            paint.setStrokeWidth(tree_node_radius * 2);
            paint.setStrokeCap(style.draw_circle ? SkPaint::kRound_Cap : SkPaint::kSquare_Cap);

            canvas->drawPoints(SkCanvas::kPoints_PointMode, points.size(), points.data(), paint);
        }
    }
}
