#include "Event.hpp"


AddNodeEvent::AddNodeEvent(size_t seq, double time, size_t node_seq, size_t parent_seq, size_t category)
    : Event(seq, time),
      node_seq(node_seq),
      parent_seq(parent_seq),
      category(category)
{}


void AddNodeEvent::apply(TreePtr tree) {
    tree->add_node(node_seq, parent_seq, category);
}


void AddNodeEvent::revert(TreePtr tree) {
    tree->remove_node(node_seq);
}


SetCategoryEvent::SetCategoryEvent(size_t seq, double time, size_t node_seq, size_t new_category)
    : Event(seq, time),
      node_seq(node_seq),
      new_category(new_category)
{}


void SetCategoryEvent::apply(TreePtr tree) {
    old_category = tree->node(node_seq)->category();
    tree->set_category(node_seq, new_category);
}


void SetCategoryEvent::revert(TreePtr tree) {
    tree->set_category(node_seq, old_category);
}


SetInfoEvent::SetInfoEvent(size_t seq, double time, size_t node_seq, const std::string& main_info, const std::string& general_info)
    : Event(seq, time),
      node_seq(node_seq),
      main_info(main_info),
      general_info(general_info)
{}


void SetInfoEvent::apply(TreePtr tree) {
    NodePtr node = tree->node(node_seq);
    old_main_info = node->main_info();
    old_general_info = node->general_info();
    node->set_info(main_info, general_info);
}


void SetInfoEvent::revert(TreePtr tree) {
    NodePtr node = tree->node(node_seq);
    node->set_info(old_main_info, old_general_info);
}


AppendInfoEvent::AppendInfoEvent(size_t seq, double time, size_t node_seq, const std::string& main_info, const std::string& general_info)
    : Event(seq, time),
      node_seq(node_seq),
      main_info(main_info),
      general_info(general_info)
{}


void AppendInfoEvent::apply(TreePtr tree) {
    NodePtr node = tree->node(node_seq);
    node->add_info(main_info, general_info);
}


void AppendInfoEvent::revert(TreePtr tree) {
    NodePtr node = tree->node(node_seq);
    node->strip_info(main_info, general_info);
}


SetBoundEvent::SetBoundEvent(size_t seq, double time, BoundType which, double bound)
    : Event(seq, time),
      which(which),
      new_bound(bound)
{}


void SetBoundEvent::apply(TreePtr tree) {
    if(which == BoundType::Lower) {
        old_bound = tree->lower_bound();
        tree->set_lower_bound(new_bound);
    }
    else {
        old_bound = tree->upper_bound();
        tree->set_upper_bound(new_bound);
    }
}


void SetBoundEvent::revert(TreePtr tree) {
    if(which == BoundType::Lower) {
        tree->set_lower_bound(old_bound);
    }
    else {
        tree->set_upper_bound(old_bound);
    }
}
