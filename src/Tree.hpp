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
typedef std::weak_ptr<Node> WeakNodePtr;
typedef std::shared_ptr<Tree> TreePtr;


class Node : public std::enable_shared_from_this<Node> {
private:
    friend class Tree;

    size_t s_;                  ///< Node sequence number
    size_t d_;                  ///< Node depth
    size_t cat_;                ///< Node category
    std::string minfo_;         ///< Principal information
    std::string ginfo_;         ///< General information

    WeakNodePtr p_;             ///< Parent node
    WeakNodePtr lsibl_;         ///< Left sibling
    NodePtr rsibl_;             ///< Right sibling
    NodePtr lchld_;             ///< First child
    WeakNodePtr rchld_;         ///< Last child

    size_t catoffs_;            ///< Offset in category
    size_t edgeoffs_;           ///< Offset of edge
    SkScalar xpre_;             ///< X coordinate within own subtree
    SkScalar xshft_;            ///< X shift of subtree
    SkScalar xacc_;             ///< Cumulative X shift
    SkScalar ext_;              ///< Extent of subtree

public:
    Node(size_t seqnum);
    Node(const Node&) = delete;
    Node(Node&&) = delete;

    size_t seq() const { return s_; }
    size_t depth() const { return d_; }
    size_t category() const { return cat_; }
    std::string main_info() const { return minfo_; }
    std::string general_info() const { return ginfo_; }
    size_t category_offset() const { return catoffs_; }

    bool has_children() const { return (bool)rsibl_; }

    void set_parent(NodePtr parent);
    void set_edge(size_t offset) { edgeoffs_ = offset; }
    void set_category(size_t category, size_t offset) { cat_ = category; catoffs_ = offset; }
    void set_info(const std::string& main, const std::string& general);
    void add_info(const std::string& main, const std::string& general);
    void strip_info(const std::string& main, const std::string& general);

    NodePtr parent() const { return p_.lock(); }
    size_t edge() const { return edgeoffs_; }
    NodePtr sibling_left() const { return lsibl_.lock(); }
    NodePtr sibling_right() const { return rsibl_; }
    NodePtr child_left() const { return lchld_; }
    NodePtr child_right() const { return rchld_.lock(); }
};


class Edge {
private:
    NodePtr p_;
    NodePtr c_;

public:
    Edge(NodePtr parent, NodePtr child) : p_(parent), c_(child) {}

    NodePtr parent() const { return p_; }
    NodePtr child() const { return c_; }
};


class Tree {
private:
    double lb_;
    double ub_;

    NodePtr root_;                          ///< Root node
    std::vector<std::vector<SkPoint>> np_;  ///< Category-based coordinate collections
    std::vector<EdgePtr> e_;                ///< Edges
    std::vector<SkPoint> ep_;               ///< Endpoints of edges
    std::vector<NodePtr> index_;            ///< Nodes by sequence number

    bool stale_;
    SkRect bbox_;

public:
    Tree();
    Tree(const Tree&) = delete;
    Tree(Tree&&) = delete;

    double lower_bound() const { return lb_; }
    double upper_bound() const { return ub_; }
    void set_lower_bound(double bound) { lb_ = bound; }
    void set_upper_bound(double bound) { ub_ = bound; }

    NodePtr node(size_t seqnum) { return index_[seqnum]; }
    void add_node(size_t node, size_t parent, size_t category);
    void remove_node(size_t node);
    void set_category(size_t node, size_t category);

    void update_layout();
    SkRect bounding_box() const { return bbox_; }
    void draw(SkCanvas* canvas) const;
};

#endif /* end of include guard: __VBC_TREE_HPP */
