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

#ifndef __VBC_EVENT_HPP
#define __VBC_EVENT_HPP

#include <memory>

#include "Tree.hpp"


class Event;
class VbcReader;
typedef std::shared_ptr<Event> EventPtr;


class Event {
private:
    const size_t seq_num;
    const double time;

public:
    Event(size_t seq, double time) : seq_num(seq), time(time) {}
    Event(const Event&) = delete;
    Event(Event&&) = delete;

    size_t get_seq_num() const { return seq_num; }
    double get_time() const { return time; }

    virtual void apply(TreePtr tree) = 0;
    virtual void revert(TreePtr tree) = 0;
};


class AddNodeEvent : public Event {
private:
    size_t node_seq;
    size_t parent_seq;
    size_t category;

public:
    AddNodeEvent(size_t seq, double time, size_t node_seq, size_t parent_seq, size_t category);

    size_t get_node_seq() const { return node_seq; }
    size_t get_parent_seq() const { return parent_seq; }
    size_t get_category() const { return category; }

    virtual void apply(TreePtr tree);
    virtual void revert(TreePtr tree);
};


class SetCategoryEvent : public Event {
private:
    size_t node_seq;
    size_t new_category;
    size_t old_category;

public:
    SetCategoryEvent(size_t seq, double time, size_t node_seq, size_t new_category);

    size_t get_node_seq() const { return node_seq; }
    size_t get_new_category() const { return new_category; }

    virtual void apply(TreePtr tree);
    virtual void revert(TreePtr tree);
};


class SetInfoEvent : public Event {
private:
    size_t node_seq;
    std::string main_info;
    std::string general_info;

    std::string old_main_info;
    std::string old_general_info;

public:
    SetInfoEvent(size_t seq, double time, size_t node_seq, const std::string& main_info, const std::string& general_info);

    size_t get_node_seq() const { return node_seq; }
    std::string get_main_info() const { return main_info; }
    std::string get_general_info() const { return general_info; }

    virtual void apply(TreePtr tree);
    virtual void revert(TreePtr tree);
};


class AppendInfoEvent : public Event {
private:
    size_t node_seq;
    std::string main_info;
    std::string general_info;

public:
    AppendInfoEvent(size_t seq, double time, size_t node_seq, const std::string& main_info, const std::string& general_info);

    size_t get_node_seq() const { return node_seq; }
    std::string get_main_info() const { return main_info; }
    std::string get_general_info() const { return general_info; }

    virtual void apply(TreePtr tree);
    virtual void revert(TreePtr tree);
};


enum class BoundType {
    Lower,
    Upper
};


class SetBoundEvent : public Event {
private:
    BoundType which;
    double new_bound;
    double old_bound;

public:
    SetBoundEvent(size_t seq, double time, BoundType which, double bound);

    BoundType get_which() const { return which; }
    double get_bound() const { return new_bound; }

    virtual void apply(TreePtr tree);
    virtual void revert(TreePtr tree);
};

#endif /* end of include guard: __VBC_EVENT_HPP */
