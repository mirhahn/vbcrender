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

#include <cmath>
#include <limits>
#include <stack>
#include <tuple>

#include <cairo.h>

#include "Styles.hpp"
#include "Tree.hpp"
#include "Types.hpp"


void NodeBase::mark_stale() {
    NodeBase* current = this;
    Node* self;

    // Recursively mark ancestors as stale
    do {
        current->stale_ = true;
    } while((self = dynamic_cast<Node*>(current)) && (current = self->parent_) && !current->stale_);
}


void NodeBase::local_layout() {
    // Don't update subtrees that are not stale
    if(!stale_) {
        return;
    }

    // Calculate separation distances
    const Scalar actual_sibling_sep = tree_sibling_sep + 2 * tree_node_radius;
    const Scalar actual_subtree_sep = tree_subtree_sep + 2 * tree_node_radius;

    // Stack structure for pseudo-recursion
    std::stack<std::tuple<NodeBase*, ChildrenIterator, bool>> stack;

    // Initialize stack by pushing NodeBase on which function was called
    stack.emplace(this, children_.begin(), true);

    // Begin iteration
    while(!stack.empty()) {
        // Fetch references to active position
        auto& tuple = stack.top();
        NodeBase* const   parent    = std::get<0>(tuple);
        ChildrenIterator& it        = std::get<1>(tuple);
        bool&             skippable = std::get<2>(tuple);

        // Skip nodes until we reach a stale child
        if(skippable) {
            while(it != parent->children_.end() && !it->get()->stale_) {
                ++it;
            }
            skippable = false;
        }

        // Iterate over remaining children updating shifts
        while(it != parent->children_.end()) {
            // If child is stale, update subtree first
            if(it->get()->stale_) {
                stack.emplace(it->get(), it->get()->children_.begin(), true);
                break;
            }

            // Set child's preliminary X coordinate based on grandchildren
            if(it->get()->children_.empty()) {
                it->get()->xpre_ = 0;
            }
            else {
                const NodePtr& front = it->get()->children_.front();
                const NodePtr& back = it->get()->children_.back();
                it->get()->xpre_ = (front->xpre_ + front->xshft_ + back->xpre_ + back->xshft_) / 2;
            }

            // Initialize node shift based on left sibling
            if(it == parent->children_.begin()) {
                it->get()->xshft_ = 0;
            }
            else {
                // Find immediate left sibling
                ChildrenIterator sibling = std::prev(it);

                // Set initial X shift based on immediate left sibling
                it->get()->xshft_ = sibling->get()->xpre_ + sibling->get()->xshft_ + actual_sibling_sep - it->get()->xpre_;

                // Skip further adjustments if current child is a leaf
                if(!it->get()->children_.empty()) {
                    // Declare variables for contour tracing
                    Node *left, *right;
                    Scalar lacc, racc;

                    // Initialize right contour pointer and shift accumulator
                    right = it->get()->children_.front().get();
                    racc  = it->get()->xshft_;

                    // Identify first left sibling that has children
                    while(sibling->get()->children_.empty() && sibling != parent->children_.begin()) {
                        --sibling;
                    }

                    // No further adjustment if necessary if no left siblings have children
                    if(!sibling->get()->children_.empty()) {
                        // Initialize left contour pointer and shift accumulator
                        left = sibling->get()->children_.back().get();
                        lacc = sibling->get()->xshft_;

                        do {
                            // Calculate adjustment
                            Scalar adj = left->xpre_ + left->xshft_ + lacc + actual_subtree_sep - (right->xpre_ + right->xshft_ + racc);

                            // Apply adjustment if it is positive
                            if(adj > 0) {
                                it->get()->xshft_ += adj;
                                racc += adj;
                            }

                            // Determine next level for contour tracing
                            size_t target_depth = right->depth() + 1;

                            // Advance right contour node by one level
                            do {
                                ChildrenIterator rsibl;

                                if(!right->children_.empty()) {
                                    racc += right->xshft_;
                                    right = right->children_.front().get();
                                }
                                else if((rsibl = std::next(right->childpos_)) != right->parent_->children_.end()) {
                                    right = rsibl->get();
                                }
                                else {
                                    do {
                                        right = reinterpret_cast<Node*>(right->parent_);
                                        racc -= right->xshft_;
                                    } while(right != it->get() && (rsibl = std::next(right->childpos_)) == right->parent_->children_.end());

                                    if(right == it->get()) {
                                        break;
                                    }
                                    else {
                                        right = rsibl->get();
                                    }
                                }
                            } while(right->depth() < target_depth);

                            // Break if right contour has been fully traced
                            if(right == it->get()) {
                                break;
                            }

                            // Advance left contour node by one level
                            do {
                                if(!left->children_.empty()) {
                                    lacc += left->xshft_;
                                    left = left->children_.back().get();
                                }
                                else if(left->childpos_ != left->parent_->children_.begin()) {
                                    left = std::prev(left->childpos_)->get();
                                }
                                else {
                                    do {
                                        left = reinterpret_cast<Node*>(left->parent_);
                                        lacc -= left->xshft_;
                                    } while(left != sibling->get() && left->childpos_ == left->parent_->children_.begin());

                                    if(left == sibling->get()) {
                                        left = nullptr;

                                        if(sibling == parent->children_.begin()) {
                                            break;
                                        }

                                        do {
                                            --sibling;
                                        } while(sibling->get()->children_.empty() && sibling != parent->children_.begin());

                                        if(sibling->get()->children_.empty()) {
                                            break;
                                        }
                                        else {
                                            left = sibling->get()->children_.back().get();
                                            lacc = sibling->get()->xshft_;
                                        }
                                    }
                                    else {
                                        left = std::prev(left->childpos_)->get();
                                    }
                                }
                            } while(left->depth() < target_depth);
                        } while(left != nullptr);
                    }
                }
            }

            // Advance to next child
            ++it;
        }

        // If we do not need to descend, the local layout of all descendants is finished
        if(it == parent->children_.end()) {
            parent->stale_ = false;
            stack.pop();
        }
    }
}


void NodeBase::aggregate_layout(Rect &bbox) {
    // Determine actual level separation
    const Scalar actual_level_sep = tree_level_sep + 2 * tree_node_radius;

    // Initialize bounding box
    bbox.x0 = std::numeric_limits<Scalar>::infinity();
    bbox.y0 = 0;
    bbox.x1 = -std::numeric_limits<Scalar>::infinity();
    bbox.y1 = 0;

    for(NodePtr& child : children_) {
        // Initialize shift accumulation on first level by copying local shift
        child->xshftacc_ = child->xshft_;
        child->x_        = child->xpre_ + child->xshftacc_;
        child->y_        = 0;

        // Update bounding box
        bbox.x0 = std::min(bbox.x0, child->x_);
        bbox.x1 = std::max(bbox.x1, child->x_);

        // Move on if there are no children
        if(child->children_.empty()) {
            continue;
        }

        // Initialize subtree traversal with leftmost child
        Node* desc = child->children_.front().get();

        // Traverse subtree
        ChildrenIterator rsibl;
        while(desc != child.get()) {
            Node* parent = reinterpret_cast<Node*>(desc->parent_);

            // Determine cumulative shift and node coordinates
            desc->xshftacc_ = desc->xshft_ + parent->xshftacc_;
            desc->x_ = desc->xpre_ + desc->xshftacc_;
            desc->y_ = parent->y_ + actual_level_sep;

            // Update bounding box
            bbox.x0 = std::min(bbox.x0, desc->x_);
            bbox.x1 = std::max(bbox.x1, desc->x_);
            bbox.y1 = std::max(bbox.y1, desc->y_);

            // Advance to next child in pre-order
            if(!desc->children_.empty()) {
                desc = desc->children_.front().get();
            }
            else if((rsibl = std::next(desc->childpos_)) != desc->parent_->children_.end()) {
                desc = rsibl->get();
            }
            else {
                do {
                    desc = reinterpret_cast<Node*>(desc->parent_);
                } while(desc != child.get() && (rsibl = std::next(desc->childpos_)) == desc->parent_->children_.end());

                if(desc != child.get()) {
                    desc = rsibl->get();
                }
            }
        }
    }
}


Node::Node(size_t seqnum)
    : parent_(nullptr),
      s_(seqnum),
      d_(0),
      cat_(0)
{}


void Node::set_parent(NodeBase* parent) {
    // Must be leaf node
    if(!children_.empty()) {
        throw std::logic_error("cannot adopt inner node");
    }

    // Reference self to avoid deletion
    NodePtr self = shared_from_this();

    // Remove from previous parent
    if(parent_) {
        parent_->children().erase(childpos_);
        parent_->mark_stale();
    }

    // Append to children of new parent
    if((parent_ = parent)) {
        childpos_ = parent_->children().insert(parent->children().end(), std::move(self));
        
        if(dynamic_cast<Node*>(parent_)) {
            d_ = reinterpret_cast<Node*>(parent_)->d_ + 1;
        }
        else {
            d_ = 0;
        }

        mark_stale();
    }
}


void Node::set_info(const std::string& main, const std::string& general) {
    minfo_ = main;
    ginfo_ = general;
}


void Node::add_info(const std::string& main, const std::string& general) {
    minfo_ += main;
    ginfo_ += general;
}


void Node::strip_info(const std::string& main, const std::string& general) {
    minfo_ = minfo_.substr(0, minfo_.size() - main.size());
    ginfo_ = ginfo_.substr(0, ginfo_.size() - general.size());
}


Tree::PostOrderIterator::PostOrderIterator(Tree& tree)
    : current(tree.children().begin()),
      end(tree.children().end())
{
    if(current != end) {
        while(!(*current)->children().empty()) {
            end = (*current)->children().end();
            current = (*current)->children().begin();
        }
    }
}


Tree::PostOrderIterator& Tree::PostOrderIterator::operator++() {
    if(++current == end) {
        // Attempt to move to parent instead
        Node* parent = dynamic_cast<Node*>(std::prev(current)->get()->parent_);
        if(parent) {
            NodeBase* grand_parent = parent->parent_;
            current = parent->childpos_;
            end = grand_parent->children().end();
        }
    }
    else {
        // Descend to left
        while(!current->get()->children().empty()) {
            current = current->get()->children().begin();
        }
        end = current->get()->parent_->children().end();
    }
    return *this;
}


Tree::PostOrderIterator& Tree::PostOrderIterator::operator--() {
    if(current != end && !current->get()->children().empty()) {
        end = current->get()->children().end();
        current = std::prev(end);
    }
    else if(current == current->get()->parent_->children().begin()) {
        ChildrenIterator it = current;
        do {
            Node* parent = dynamic_cast<Node*>(it->get()->parent_);
            if(!parent) {
                throw std::logic_error("attempted to decrement post-order iterator beyond first node");
            }
            it = parent->childpos_;
        } while(it == it->get()->parent_->children().begin());
    }
    else {
        current = std::prev(current);
    }
    return *this;
}


Tree::PreOrderIterator::PreOrderIterator(Tree& tree)
    : current(tree.children().begin()),
      end(tree.children().end())
{}


Tree::PreOrderIterator& Tree::PreOrderIterator::operator++() {
    // Attempt to move to child
    if(!current->get()->children().empty()) {
        end = current->get()->children().end();
        current = current->get()->children().begin();
    }
    // Otherwise, ascend until move to right sibling is possible
    else {
        while(++current == end) {
            Node* parent = dynamic_cast<Node*>(std::prev(current)->get()->parent_);
            if(!parent) {
                break;
            }
            end = parent->parent_->children().end();
            current = parent->childpos_;
        }
    }
    return *this;
}


Tree::PreOrderIterator& Tree::PreOrderIterator::operator--() {
    // Attempt to move left and descend
    if(current == end || current != current->get()->parent_->children().begin()) {
        current = std::prev(current);
        while(!current->get()->children().empty()) {
            end = current->get()->children().end();
            current = std::prev(end);
        }
    }
    // Otherwise, ascend by one level
    else {
        Node* parent = dynamic_cast<Node*>(current->get()->parent_);
        if(!parent) {
            throw std::logic_error("attempted to decrement pre-order iterator beyond first node");
        }
        current = parent->childpos_;
        end = parent->parent_->children().end();
    }
    return *this;
}


Tree::Tree()
    : NodeBase(),
      lb_(-std::numeric_limits<double>::infinity()),
      ub_(std::numeric_limits<double>::infinity()),
      nvert_(0),
      stale_(true)
{}


void Tree::add_node(size_t seqnum, size_t parent_seqnum, size_t category) {
    NodePtr parent;

    // Validate data
    if(seqnum < index_.size() && index_[seqnum]) {
        throw std::invalid_argument("assigned or reserved sequence number");
    }
    if(parent_seqnum > 0 && (parent_seqnum >= index_.size() || !(parent = index_[parent_seqnum]))) {
        throw std::invalid_argument("unknown parent sequence number");
    }
    if(category >= node_style_table.size()) {
        throw std::invalid_argument("unknown category code");
    }

    // Create new node and assign parent
    NodePtr node = std::make_shared<Node>(seqnum);
    node->set_parent(parent_seqnum ? static_cast<NodeBase*>(parent.get()) : static_cast<NodeBase*>(this));

    // Enter node into sequence index
    if(index_.size() <= seqnum) {
        index_.resize(seqnum + 1);
    }
    index_[seqnum] = node;

    // Set node category
    node->set_category(category);

    // Increase node count
    ++nvert_;
    stale_ = true;
}


void Tree::remove_node(size_t seqnum) {
    // Validate data
    NodePtr node;
    if(seqnum >= index_.size() || !(node = index_[seqnum])) {
        throw std::out_of_range("unknown sequence number");
    }

    // Attempt to orphan node (will throw exception if impossible)
    node->set_parent(nullptr);

    // Remove node from sequence index
    index_[seqnum].reset();

    // Decrease node count and mark layout as stale
    --nvert_;
    stale_ = true;
}


void Tree::set_category(size_t seqnum, size_t category) {
    // Validate category code
    NodePtr node;
    if(category >= node_style_table.size()) {
        throw std::out_of_range("invalid node category code");
    }
    if(seqnum >= index_.size() || !(node = index_[seqnum])) {
        throw std::out_of_range("unknown sequence number");
    }

    // Set new category
    node->set_category(category);
}


void Tree::update_layout() {
    // Short-circuit if the layout is up to date
    if(!stale_) {
        return;
    }

    local_layout();
    aggregate_layout(bbox_);

    stale_ = false;
}
//
//    // Calculate node and subtree separation
//    const Scalar actual_sibling_sep = 2 * tree_node_radius + tree_sibling_sep;
//    const Scalar actual_subtree_sep = 2 * tree_node_radius + tree_subtree_sep;
//    const Scalar actual_level_sep = 2 * tree_node_radius + tree_level_sep;
//
//    // Traverse tree
//    PostOrderIterator it(*this);
//    const PostOrderIterator end(children().end(), children().end());
//    while(it != end) {
//        // Obtain pointer to the current node
//        Node* node = it->get();
//
//        // Get begin and end iterator of children list
//        ChildrenIterator begin = node->parent_->children().begin();
//        ChildrenIterator end = node->parent_->children().end();
//
//        // Set preliminary coordinate based on children
//        if(node->children().empty()) {
//            node->x_ = 0.0;
//        }
//        else {
//            Node* left = node->children().front().get();
//            Node* right = node->children().back().get();
//            node->x_ = (left->x_ + left->xshft_ + right->x_ + right->xshft_) / 2;
//        }
//
//        // Set shift based on left siblings
//        if(node->childpos_ == begin) {
//            node->xshft_ = 0.0;
//        }
//        else {
//            // Set initial shift based on immediate sibling
//            ChildrenIterator rcont_it = std::prev(node->childpos_);
//            Node* rcont_node = rcont_it->get();
//            node->xshft_ = rcont_node->xshft_ + rcont_node->x_ + actual_sibling_sep - node->x_;
//
//            // Begin descent along contour
//            if(!node->children().empty()) {
//                ChildrenIterator lcont_it = node->children().begin();
//                const ChildrenIterator lcont_end = node->children().end();
//                Node* lcont_node = lcont_it->get();
//
//                Scalar lcont_shft = node->xshft_ + lcont_node->xshft_;
//                Scalar rcont_shft = rcont_node->xshft_;
//                while(lcont_it != lcont_end) {
//                    // Descend one level along right contour of left subtrees
//                    while(rcont_node->depth() < lcont_node->depth()) {
//                        if(!rcont_node->children().empty()) {
//                            rcont_it = std::prev(rcont_node->children().end());
//                            rcont_node = rcont_it->get();
//                            rcont_shft += rcont_node->xshft_;
//                        }
//                        else {
//                            while(rcont_it != begin && rcont_it == rcont_node->parent_->children().begin()) {
//                                rcont_it = reinterpret_cast<Node*>(rcont_node->parent_)->childpos_;
//                                rcont_shft -= rcont_node->xshft_;
//                                rcont_node = rcont_it->get();
//                            }
//                            if(rcont_it != begin) {
//                                rcont_it = std::prev(rcont_it);
//                                rcont_shft -= rcont_node->xshft_;
//                                rcont_node = rcont_it->get();
//                                rcont_shft += rcont_node->xshft_;
//                            }
//                            else {
//                                break;
//                            }
//                        }
//                    }
//                    if(rcont_it == begin) {
//                        break;
//                    }
//
//                    // Calculate required adjustment
//                    Scalar adjust = rcont_shft + rcont_node->x_ + actual_subtree_sep - (lcont_shft + lcont_node->x_);
//                    if(adjust > 0.0) {
//                        node->xshft_ += adjust;
//                        lcont_shft += adjust;
//                    }
//
//                    // Descend one level along left contour of right subtree
//                    while(lcont_it != lcont_end && lcont_node->depth() <= rcont_node->depth()) {
//                        if(!lcont_node->children().empty()) {
//                            lcont_it = lcont_node->children().begin();
//                            lcont_node = lcont_it->get();
//                            lcont_shft += lcont_node->xshft_;
//                        }
//                        else {
//                            while(++lcont_it != lcont_end && lcont_it == lcont_node->parent_->children().end()) {
//                                lcont_it = reinterpret_cast<Node*>(lcont_node->parent_)->childpos_;
//                                lcont_shft -= lcont_node->xshft_;
//                                lcont_node = lcont_it->get();
//                            }
//                            if(lcont_it != lcont_end) {
//                                lcont_shft -= lcont_node->xshft_;
//                                lcont_node = lcont_it->get();
//                                lcont_shft += lcont_node->xshft_;
//                            }
//                            else {
//                                break;
//                            }
//                        }
//                    }
//                }
//            }
//        }
//
//        // Advance to next node
//        ++it;
//    }
//
//    // Accumulate node shift and calculate final positions
//    bbox_.x0 = Scalar(0.0);
//    bbox_.x1 = Scalar(0.0);
//    bbox_.y0 = Scalar(0.0);
//    bbox_.y1 = Scalar(0.0);
//    PreOrderIterator pre_it(*this);
//    const PreOrderIterator pre_end(children().end(), children().end());
//    
//    while(pre_it != pre_end) {
//        Node* node = pre_it->get();
//
//        if(node->parent_ != this) {
//            Node* parent = reinterpret_cast<Node*>(node->parent_);
//            node->xshft_ += parent->xshft_;
//        }
//
//        node->x_ += node->xshft_;
//        node->y_ = node->depth() * actual_level_sep;
//        bbox_.x1 = std::max(bbox_.x1, node->x_);
//        bbox_.x0 = std::min(bbox_.x0, node->x_);
//        bbox_.y1 = std::max(bbox_.y1, node->y_);
//        bbox_.y0 = std::min(bbox_.y0, node->y_);
//
//        ++pre_it;
//    }
//    bbox_.x0 -= tree_node_radius;
//    bbox_.x1 += tree_node_radius;
//    bbox_.y0 -= tree_node_radius;
//    bbox_.y1 += tree_node_radius;
//
//    // Mark layout as not stale
//    stale_ = false;
//}
