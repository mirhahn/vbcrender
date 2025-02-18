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

#ifndef __VBC_VBC_READER_HPP
#define __VBC_VBC_READER_HPP

#include <atomic>
#include <condition_variable>
#include <deque>
#include <forward_list>
#include <memory>
#include <mutex>
#include <thread>

#include "Event.hpp"
#include "Tree.hpp"

class VbcReader;
typedef std::shared_ptr<VbcReader> VbcReaderPtr;

class VbcReader : public std::enable_shared_from_this<VbcReader> {
public:
    enum State {
        Empty,
        Processing,
        EndOfStream,
        Error
    };

    /// Virtual event placed into event queue to mark end of file.
    class EOSEvent : public Event {
    public:
        EOSEvent();
        virtual void apply(TreePtr tree);
        virtual void revert(TreePtr tree);
    };

    /// Virtual event placed into event queue to mark I/O error.
    class IOErrorEvent : public Event {
    private:
        std::string what_;

    public:
        IOErrorEvent(const std::string& what);
        virtual void apply(TreePtr tree);
        virtual void revert(TreePtr tree);

        const std::string& what() const { return what_; }
    };

private:
    const bool       rewind_;   ///< Indicates rewindable reader.
    const bool       strip_;    ///< Discard textual information on nodes.
    std::atomic_bool running_;  ///< Indicates running read thread.
    bool             stopreq_;  ///< User has requested read thread to stop.
    std::thread      reader_;   ///< Current reader thread.

    std::mutex              m_;     ///< Mutex used for data wait operations.
    std::condition_variable cv_;    ///< Condition variable used to wait for data.

    TreePtr tree_;                      ///< Internal tree structure.
    std::deque<EventPtr> fwd_;          ///< Forward event queue.
    std::forward_list<EventPtr> rev_;   ///< Rewind event stack.
    double timestamp_;                  ///< Timestamp of last non-virtual event applied

    EventPtr push_event(EventPtr old_head, EventPtr new_head);
    void read_file(std::string filename);

public:
    VbcReader(bool rewindable, bool strip_info);
    VbcReader(const VbcReader&) = delete;
    VbcReader(VbcReader&&) = delete;
    ~VbcReader();

    bool is_rewindable() const { return rewind_; }
    TreePtr get_tree() const { return tree_; }

    bool open(const std::string& filename);
    bool advance();
    bool rewind();
    void wait();
    void close();
    void clear();

    State get_state() const;
    bool has_prev() const;
    bool has_next() const;
    double get_timestamp() const;
    double get_next_timestamp() const;
};

#endif /* end of include guard: __VBC_VBC_READER_HPP */
