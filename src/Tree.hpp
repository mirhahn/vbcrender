#ifndef __VBC_TREE_HPP
#define __VBC_TREE_HPP

#include <memory>
#include <string>
#include <vector>

#include <SkCanvas.h>
#include <SkPoint.h>


class Edge;
class Node;
class Tree;
typedef std::shared_ptr<Edge> EdgePtr;
typedef std::shared_ptr<Node> NodePtr;
typedef std::shared_ptr<Tree> TreePtr;


class Node {
private:
    friend class Tree;

    size_t seqnum;
    size_t category;
    NodePtr parent;
    std::string main_info;
    std::string general_info;

    size_t depth;
    size_t depth_offset;
    size_t category_offset;

    size_t child_count;
    double subtree_width;
    double allocated_width;

public:
    Node(size_t seqnum, NodePtr parent, size_t category);

    size_t get_seq_num() const { return seqnum; }
    size_t get_category() const { return category; }
    NodePtr get_parent() const { return parent; }
    std::string get_main_info() const { return main_info; }
    std::string get_general_info() const { return general_info; }
    size_t get_depth() const { return depth; }

    void set_info(const std::string& main, const std::string& general);
    void add_info(const std::string& main, const std::string& general);
    void strip_info(const std::string& main, const std::string& general);
};


class Edge {
private:
    friend class Tree;

    NodePtr parent;
    NodePtr child;

public:
    Edge(NodePtr parent, NodePtr child);

    NodePtr get_parent() const { return parent; }
    NodePtr get_child() const { return child; }
};


class Tree {
private:
    friend class Node;
    friend class Edge;

    double lower_bound;
    double upper_bound;

    std::vector<NodePtr> nodes;
    std::vector<EdgePtr> edges;

    std::vector<size_t>               offset_table;
    std::vector<std::vector<size_t>>  depth_index;
    std::vector<std::vector<size_t>>  category_index;
    std::vector<SkPoint>              edge_points;
    std::vector<std::vector<SkPoint>> node_points;

    bool layout_stale;
    SkRect bounding_box;

public:
    Tree();
    Tree(const Tree&) = delete;
    Tree(Tree&&) = delete;

    double get_lower_bound() const { return lower_bound; }
    double get_upper_bound() const { return upper_bound; }
    void set_lower_bound(double bound) { lower_bound = bound; }
    void set_upper_bound(double bound) { upper_bound = bound; }

    size_t node_count() const { return nodes.size(); }
    size_t edge_count() const { return edges.size(); }
    NodePtr node(size_t seqnum) const { return nodes[offset_table[seqnum]]; }
    EdgePtr edge(size_t idx) const { return edges[idx]; }

    void add_node(size_t node, size_t parent, size_t category);
    void remove_node(size_t node);
    void set_category(size_t node, size_t category);

    void update_layout();
    SkRect get_bounding_box() const { return bounding_box; }
    void draw(SkCanvas* canvas) const;
};

#endif /* end of include guard: __VBC_TREE_HPP */
