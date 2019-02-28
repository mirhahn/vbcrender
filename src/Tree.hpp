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

#ifndef __VBC_TREE_HPP
#define __VBC_TREE_HPP

#include <cstddef>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "Types.hpp"


class Edge;
class NodeBase;
class Node;
class Tree;
typedef std::shared_ptr<Edge> EdgePtr;
typedef std::shared_ptr<NodeBase> NodeBasePtr;
typedef std::shared_ptr<Node> NodePtr;
typedef std::weak_ptr<Node> WeakNodePtr;
typedef std::shared_ptr<Tree> TreePtr;


class NodeBase {
public:
    typedef std::list<NodePtr> ChildrenList;
    typedef typename ChildrenList::iterator ChildrenIterator;
    typedef typename ChildrenList::const_iterator ConstChildrenIterator;
    typedef typename ChildrenList::reverse_iterator ReverseChildrenIterator;
    typedef typename ChildrenList::const_reverse_iterator ConstReverseChildrenIterator;

protected:
    ChildrenList children_;     ///< List of children

public:
    virtual ~NodeBase() {}

    ChildrenList& children() { return children_; }
    const ChildrenList& children() const { return children_; }
};


class Node : public NodeBase, public std::enable_shared_from_this<Node> {
private:
    friend class Tree;

    NodeBase* parent_;          ///< Parent node
    ChildrenIterator childpos_; ///< Corresponding iterator in parent's children list.

    size_t s_;                  ///< Node sequence number
    size_t d_;                  ///< Node depth
    size_t cat_;                ///< Node category
    std::string minfo_;         ///< Principal information
    std::string ginfo_;         ///< General information

    Scalar xpre_;               ///< X coordinate within own subtree
    Scalar xshft_;              ///< X shift of subtree
    Scalar xacc_;               ///< Cumulative X shift
    Scalar ext_;                ///< Extent of subtree

public:
    Node(size_t seqnum);
    Node(const Node&) = delete;
    Node(Node&&) = delete;

    size_t seq() const { return s_; }
    size_t depth() const { return d_; }
    size_t category() const { return cat_; }
    std::string main_info() const { return minfo_; }
    std::string general_info() const { return ginfo_; }

    void set_parent(NodeBase* parent);
    void set_category(size_t category) { cat_ = category; }
    void set_info(const std::string& main, const std::string& general);
    void add_info(const std::string& main, const std::string& general);
    void strip_info(const std::string& main, const std::string& general);

    NodePtr parent() const { return dynamic_cast<Node*>(parent_) ? reinterpret_cast<Node*>(parent_)->shared_from_this() : nullptr; }
    ChildrenIterator iterator() { return childpos_; }
    ConstChildrenIterator iterator() const { return childpos_; }
};


class Tree : public NodeBase, public std::enable_shared_from_this<Tree> {
public:
    class PostOrderIterator {
    public:
        typedef ptrdiff_t                       difference_type;
        typedef NodePtr                         value_type;
        typedef NodePtr*                        pointer;
        typedef NodePtr&                        reference;
        typedef std::bidirectional_iterator_tag iterator_category;

    private:
        ChildrenIterator current;
        ChildrenIterator end;

    public:
        PostOrderIterator() : current(), end() {}
        PostOrderIterator(ChildrenIterator begin, ChildrenIterator end) : current(begin), end(end) {}
        explicit PostOrderIterator(Tree& tree);

        NodePtr& operator*() const { return *current; }
        NodePtr* operator->() const { return &*current; }

        bool operator==(const PostOrderIterator& it) const { return current == it.current; }
        bool operator!=(const PostOrderIterator& it) const { return current != it.current; }

        PostOrderIterator& operator++();
        PostOrderIterator& operator--();
        PostOrderIterator operator++(int) { PostOrderIterator it = *this; ++*this; return it; }
        PostOrderIterator operator--(int) { PostOrderIterator it = *this; --*this; return it; }
    };

    class PreOrderIterator {
    public:
        typedef ptrdiff_t                       difference_type;
        typedef NodePtr                         value_type;
        typedef NodePtr*                        pointer;
        typedef NodePtr&                        reference;
        typedef std::bidirectional_iterator_tag iterator_category;

    private:
        ChildrenIterator current;
        ChildrenIterator end;

    public:
        PreOrderIterator() : current(), end() {}
        PreOrderIterator(ChildrenIterator begin, ChildrenIterator end) : current(begin), end(end) {}
        explicit PreOrderIterator(Tree& tree);

        NodePtr& operator*() const { return *current; }
        NodePtr* operator->() const { return &*current; }

        bool operator==(const PreOrderIterator& it) const { return current == it.current; }
        bool operator!=(const PreOrderIterator& it) const { return current != it.current; }

        PreOrderIterator& operator++();
        PreOrderIterator& operator--();
        PreOrderIterator operator++(int) { PreOrderIterator it = *this; ++*this; return it; }
        PreOrderIterator operator--(int) { PreOrderIterator it = *this; --*this; return it; }
    };

private:
    double lb_;                             ///< Global lower bound for objective function value
    double ub_;                             ///< Global upper bound for objective function value
    bool stale_;                            ///< Indicates that the layout needs to be updated
    Rect bbox_;                             ///< Bounding box determined by last layout
    std::vector<NodePtr> index_;            ///< Nodes by sequence number

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
    Rect bounding_box() const { return bbox_; }
    void draw(Canvas* canvas, bool raster_protect = false);
};

#endif /* end of include guard: __VBC_TREE_HPP */
