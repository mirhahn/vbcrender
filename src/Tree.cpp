#include <stack>

#include <SkPaint.h>

#include "Styles.hpp"
#include "Tree.hpp"

Node::Node(size_t seqnum)
    : s_(seqnum),
      d_(0),
      cat_(0),
      catoffs_(std::numeric_limits<size_t>::max()),
      edgeoffs_(std::numeric_limits<size_t>::max())
{}


void Node::set_parent(NodePtr parent) {
    // Must be leaf node
    if(lchld_) {
        throw std::logic_error("cannot adopt inner node");
    }

    // Remove from previous parent
    NodePtr p = p_.lock();
    if(p) {
        // Find parent and immediate siblings
        NodePtr sl = sibling_left();
        NodePtr sr = sibling_right();
        
        // Remove node from child list
        if(sl) {
            sl->rsibl_ = sr;
        }
        else {
            p->lchld_ = sr;
        }

        if(sr) {
            sr->lsibl_ = sl;
        }
        else {
            p->rchld_ = sl;
        }

        lsibl_.reset();
        rsibl_.reset();
    }

    // Append to children of new parent
    if(parent) {
        // Make right sibling of rightmost child
        NodePtr lsibl = parent->rchld_.lock();
        lsibl_ = lsibl;
        if(lsibl) {
            lsibl->rsibl_ = shared_from_this();
        }

        // Make tail and (possibly) head of parent's child list
        parent->rchld_ = shared_from_this();
        if(!parent->lchld_) {
            parent->lchld_ = shared_from_this();
        }

        p_ = parent;
        d_ = parent->d_ + 1;
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


Tree::Tree()
    : lb_(-std::numeric_limits<double>::infinity()),
      ub_(std::numeric_limits<double>::infinity()),
      stale_(true),
      bbox_(),
      np_(node_style_table.size())
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
    if(category >= np_.size()) {
        throw std::invalid_argument("unknown category code");
    }
    if(!parent && root_) {
        throw std::invalid_argument("cannot create multiple roots");
    }

    // Create new node and assign parent
    NodePtr node = std::make_shared<Node>(seqnum);
    node->set_parent(parent);
    if(!parent) {
        root_ = node;
    }

    // Enter node into sequence index
    if(index_.size() <= seqnum) {
        index_.resize(seqnum + 1);
    }
    index_[seqnum] = node;

    // Allocate new coordinate pair and set category
    size_t offset = np_[category].size();
    np_[category].emplace_back();
    node->set_category(category, offset);

    // Allocate new edge
    if(parent) {
        offset = e_.size();
        e_.emplace_back(std::make_shared<Edge>(parent, node));
        ep_.emplace_back();
        ep_.emplace_back();
        node->set_edge(offset);
    }

    // Mark layout as stale
    stale_ = true;
}


void Tree::remove_node(size_t seqnum) {
    // Validate data
    NodePtr node;
    if(seqnum >= index_.size() || !(node = index_[seqnum])) {
        throw std::out_of_range("unknown sequence number");
    }

    // Remove node from sequence index
    index_[seqnum].reset();

    // Attempt to orphan node (will throw exception if impossible)
    node->set_parent(nullptr);

    // Remove root if necessary
    if(root_ == node) {
        root_.reset();
    }

    // Remove coordinate pair and edge
    size_t category = node->category();
    size_t coord_offs = node->category_offset();
    size_t edge_offs = node->edge();

    np_[category].pop_back();
    if(coord_offs == np_[category].size()) {
        size_t offs;
        for(NodePtr n : index_) {
            if(n && n->category() == category && (offs = n->category_offset()) > coord_offs) {
                n->set_category(category, offs - 1);
            }
        }
    }

    if(edge_offs < e_.size()) {
        ep_.pop_back();
        auto it = e_.erase(std::next(e_.begin(), edge_offs));
        while(it != e_.end()) {
            (*(it++))->child()->set_edge(edge_offs++);
        }
    }

    // Mark layout as stale
    stale_ = true;
}


void Tree::set_category(size_t seqnum, size_t category) {
    // Validate category code
    NodePtr node;
    if(category >= np_.size()) {
        throw std::out_of_range("invalid node category code");
    }
    if(seqnum >= index_.size() || !(node = index_[seqnum])) {
        throw std::out_of_range("unknown sequence number");
    }

    // Stop if category is actually unchanged
    size_t old_cat = node->category();
    if(category == old_cat) {
        return;
    }

    // Remove node from category index
    size_t offset = node->category_offset();
    size_t n_offs;
    np_[old_cat].pop_back();
    for(NodePtr n : index_) {
        if(n && n->category() == old_cat && (n_offs = n->category_offset()) > offset) {
            n->set_category(old_cat, n_offs - 1);
        }
    }

    // Insert node into new location in category index
    offset = np_[category].size();
    np_[category].emplace_back();
    node->set_category(category, offset);

    // Mark layout as stale
    stale_ = true;
}


void Tree::update_layout() {
    // Short-circuit if there are no nodes or the layout is up to date
    if(!root_ || !stale_) {
        return;
    }

    // Calculate node and subtree separation
    const SkScalar actual_sibling_sep = SkScalar(2 * tree_node_radius + tree_sibling_sep);
    const SkScalar actual_subtree_sep = SkScalar(2 * tree_node_radius + tree_subtree_sep);
    const SkScalar actual_level_sep = SkScalar(2 * tree_node_radius + tree_level_sep);

    // Traverse tree
    NodePtr node = root_;
    NodePtr other;
    while(node) {
        // Dive into leftmost leaf
        while((other = node->child_left())) {
            node = other;
        }

        // Set preliminary coordinate and shift
        node->xpre_ = 0.0;
        node->xshft_ = (other = node->sibling_left()) ? other->xpre_ + other->xshft_ + actual_sibling_sep : 0.0;

        // Attempt to shift into right sibling
        if((other = node->sibling_right())) {
            node = other;
        }
        // Start ascending into parents
        else {
            while((node = node->parent())) {
                // Calculate preliminary position of node based on children
                NodePtr left = node->child_left();
                NodePtr right = node->child_right();
                node->xpre_ = (left->xpre_ + left->xshft_ + right->xpre_ + right->xshft_) / 2;

                // Determine shift based on sibling contours
                right = node->sibling_left();
                if(!right) {
                    node->xshft_ = 0.0;
                }
                else {
                    node->xshft_ = right->xpre_ + right->xshft_ + actual_sibling_sep;

                    // Trace contours
                    double acc_left = node->xshft_;
                    double acc_right = 0.0;
                    while(right && left) {
                        // Trace right contour of left subtree until depth is reached
                        while(right && right->depth() < left->depth()) {
                            if((other = right->child_right())) {
                                acc_right += right->xshft_;
                                right = std::move(other);
                            }
                            else if((other = right->sibling_left())) {
                                right = std::move(other);
                            }
                            else {
                                do {
                                    right = right->parent();
                                    acc_right -= right->xshft_;
                                } while(right && right->depth() >= node->depth() && !(other = right->sibling_left()));
                                
                                if(right && right->depth() < node->depth()) {
                                    right.reset();
                                }
                                else {
                                    right = std::move(other);
                                }
                            }
                        }

                        if(!right) {
                            break;
                        }

                        // Adjust node shift based on contour proximity
                        double adj = acc_right + right->xshft_ + right->xpre_ + actual_subtree_sep - (acc_left + left->xshft_ - left->xpre_);
                        if(adj > 0.0) {
                            node->xshft_ += adj;
                            acc_left += adj;
                        }

                        // Trace left contour of right subtree until depth is increased by one
                        while(left && left->depth() <= right->depth()) {
                            if((other = left->child_left())) {
                                acc_left += left->xshft_;
                                left = std::move(other);
                            }
                            else if((other = left->sibling_right())) {
                                left = std::move(other);
                            }
                            else {
                                do {
                                    left = left->parent();
                                    acc_left -= left->xshft_;
                                } while(left && left->depth() > node->depth() && !(other = left->sibling_right()));

                                if(left && left->depth() <= node->depth()) {
                                    left.reset();
                                    break;
                                }
                                else {
                                    left = std::move(other);
                                }
                            }
                        }
                    }
                }

                // Attempt to move into right sibling
                if((other = node->sibling_right())) {
                    node = other;
                    break;
                }
            }
        }
    }

    // Accumulate node shift and calculate final positions
    bbox_.setEmpty();
    node = root_;
    while(node) {
        node->xacc_ = node->xshft_;
        if((other = node->parent())) {
            node->xacc_ += other->xacc_;
        }

        SkPoint& coord = np_[node->category()][node->category_offset()];
        coord.fX = node->xacc_ + node->xpre_;
        coord.fY = node->depth() * actual_level_sep;
        bbox_.fRight = std::max(bbox_.fRight, coord.fX);
        bbox_.fBottom = std::max(bbox_.fBottom, coord.fY);

        size_t edge = node->edge();
        if(edge < e_.size()) {
            NodePtr parent = node->parent();
            ep_[2 * edge] = np_[parent->category()][parent->category_offset()];
            ep_[2 * edge + 1] = np_[node->category()][node->category_offset()];
        }

        if((other = node->child_left())) {
            node = std::move(other);
        }
        else if((other = node->sibling_right())) {
            node = std::move(other);
        }
        else {
            do {
                node = node->parent();
            } while(node && !(other = node->sibling_right()));

            if(node) {
                node = std::move(other);
            }
            else {
                break;
            }
        }
    }
    bbox_.inset(-SkScalar(tree_node_radius), -SkScalar(tree_node_radius));

    // Mark layout as not stale
    stale_ = false;
}


void Tree::draw(SkCanvas* canvas) const {
    // Draw edges
    SkPaint edge_paint;
    edge_paint.setColor(edge_style_table.front().edge_color);
    canvas->drawPoints(SkCanvas::kLines_PointMode, ep_.size(), ep_.data(), edge_paint);

    // Draw nodes
    for(size_t category = 0; category < np_.size(); ++category) {
        const auto& points = np_[category];
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
