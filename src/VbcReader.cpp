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

#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <typeindex>

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/file.hpp>

#include "VbcReader.hpp"

namespace bio = boost::iostreams;


VbcReader::EOSEvent::EOSEvent()
    : Event(std::numeric_limits<size_t>::max(), -1.0)
{}


void VbcReader::EOSEvent::apply(TreePtr tree) {}


void VbcReader::EOSEvent::revert(TreePtr tree) {}


VbcReader::IOErrorEvent::IOErrorEvent(const std::string& what)
    : Event(std::numeric_limits<size_t>::max(), -1.0),
      what_(what)
{}


void VbcReader::IOErrorEvent::apply(TreePtr tree) {}


void VbcReader::IOErrorEvent::revert(TreePtr tree) {}


VbcReader::VbcReader(bool rewindable, bool strip_info)
    : rewind_(rewindable),
      strip_(strip_info),
      running_(false),
      stopreq_(false)
{}


VbcReader::~VbcReader() {
    // Wait for reader thread termination
    stopreq_ = true;
    if(reader_.joinable()) {
        reader_.join();
    }
}


EventPtr VbcReader::push_event(EventPtr old_head, EventPtr new_head) {
    {
        std::lock_guard<std::mutex> lock(m_);
        fwd_.push_back(new_head);
    }

    // Notify waiting thread that advancement is now possible
    cv_.notify_one();

    return new_head;
};


void VbcReader::read_file(std::string filename) {
    typedef typename bio::filtering_istream::traits_type traits_type;
    typedef typename traits_type::int_type int_type;

    EventPtr head;
    bio::filtering_istream in;

    // Build pipeline based on file extensions
    in.push(bio::file_source(filename));

    // Test if file was successfully opened
    bio::file_source* file = in.component<bio::file_source>(in.size() - 1);
    if(!file->is_open()) {
        head = push_event(head, std::make_shared<IOErrorEvent>("Could not open VBC file"));
    }

    // Read from file
    size_t event_seq = 0;
    char field[128];
    bool error = false;
    while(!stopreq_ && !error && in) {
        // Ignore leading whitespace
        in >> std::ws;

        // Peek at next character to determine type of line
        int_type nchar = in.get();
        if(nchar == traits_type::eof()) {
            break;
        }
        else if(nchar == '#') {
            // Read metadata line
            char* value = field + 64;
            in.get(field, 64, ':');
            in.ignore();
            in >> std::ws;
            in.get(value, 64, '\n');

            if(!strcmp(field, "TYPE")) {
                if(strcmp(value, "COMPLETE TREE")) {
                    head = push_event(head, std::make_shared<IOErrorEvent>("VbcReader encountered invalid TYPE line"));
                    error = true;
                }
            }
            else if(!strcmp(field, "TIME")) {
                if(strcmp(value, "SET")) {
                    head = push_event(head, std::make_shared<IOErrorEvent>("VbcReader only reads SET VBC files"));
                    error = true;
                }
            }
            else if(!strcmp(field, "INFORMATION")) {
                if(strcmp(value, "STANDARD")) {
                    head = push_event(head, std::make_shared<IOErrorEvent>("VbcReader cannot process EXCEPTION information"));
                    error = true;
                }
            }
            else if(!strcmp(field, "NODE_NUMBER")) {
                if(strcmp(value, "NONE")) {
                    head = push_event(head, std::make_shared<IOErrorEvent>("VbcReader cannot process node numbers"));
                    error = true;
                }
            }
        }
        else {
            // Read time stamp
            double timestamp = 0.0;
            in.putback((char)nchar);
            do {
                double component;
                in >> component;
                timestamp = 60 * timestamp + component;
            } while((nchar = in.get()) == ':');

            // Read opcode
            char opcode;
            if(!(in >> std::ws >> opcode)) {
                head = push_event(head, std::make_shared<IOErrorEvent>("incomplete operation encountered"));
                error = true;
            }

            // Read remainder of line depending on opcode
            switch(opcode) {
            case 'A':
            case 'I':
                {
                    size_t node_seq;
                    std::ostringstream main_info;
                    std::ostringstream general_info;
                    std::ostringstream* active = &general_info;
                    std::ostringstream* inactive = &main_info;
                    
                    in >> node_seq >> std::ws;
                    while((nchar = in.get()) != traits_type::eof() && nchar != '\n') {
                        if(nchar == '\\') {
                            switch(nchar = in.get()) {
                            case 't':
                                active->put('\t');
                                break;
                            case 'n':
                                active->put('\n');
                                break;
                            case 'i':
                                std::swap(active, inactive);
                                break;
                            default:
                                if(nchar != traits_type::eof()) {
                                    active->put((char)nchar);
                                }
                            }
                        }
                        else {
                            active->put((char)nchar);
                        }
                    }


                    if(!in) {
                        head = push_event(head, std::make_shared<IOErrorEvent>("error reading information modification parameters (opcode A or I)"));
                        error = true;
                    }
                    else if(strip_) {
                        break;
                    }
                    else if(opcode == 'A') {
                        head = push_event(head, std::make_shared<AppendInfoEvent>(event_seq++, timestamp, node_seq, main_info.str(), general_info.str()));
                    }
                    else {
                        head = push_event(head, std::make_shared<SetInfoEvent>(event_seq++, timestamp, node_seq, main_info.str(), general_info.str()));
                    }
                }
                break;
            case 'D':
            case 'N':
                {
                    size_t node_seq;
                    size_t parent_seq;
                    size_t category;

                    in >> parent_seq >> node_seq >> category;
                    if(opcode == 'D') {
                        size_t ignored;
                        in >> ignored;
                    }

                    if(!in) {
                        head = push_event(head, std::make_shared<IOErrorEvent>("error reading node creation parameters (opcode D or N)"));
                        error = true;
                    }
                    else {
                        head = push_event(head, std::make_shared<AddNodeEvent>(event_seq++, timestamp, node_seq, parent_seq, category));
                    }
                }
                break;
            case 'P':
                {
                    size_t node_seq;
                    size_t category;

                    in >> node_seq >> category;

                    if(!in) {
                        head = push_event(head, std::make_shared<IOErrorEvent>("error reading color change parameters (opcode P)"));
                        error = true;
                    }
                    else {
                        head = push_event(head, std::make_shared<SetCategoryEvent>(event_seq++, timestamp, node_seq, category));
                    }
                }
                break;
            case 'L':
            case 'U':
                {
                    double bound;

                    in >> bound;

                    if(!in) {
                        head = push_event(head, std::make_shared<IOErrorEvent>("error reading bound change event (opcode L or U)"));
                        error = true;
                    }
                    else {
                        head = push_event(head, std::make_shared<SetBoundEvent>(event_seq++, timestamp, (opcode == 'L') ? BoundType::Lower : BoundType::Upper, bound));
                    }
                }
                break;
            default:
                head = push_event(head, std::make_shared<IOErrorEvent>("unknown opcode"));
                error = true;
            }
        }
    }

    // Push end-of-stream event if no error has occurred
    if(!std::dynamic_pointer_cast<IOErrorEvent>(head)) {
        head = push_event(head, std::make_shared<EOSEvent>());
    }

    // Close file
    in.reset();
    running_ = false;

    // Notify all waiting threads
    cv_.notify_all();
}


bool VbcReader::open(const std::string& filename) {
    using std::placeholders::_1;

    // Do not reopen if already running
    if(running_.exchange(true)) {
        return false;
    }

    // Clear internal state
    fwd_.clear();
    rev_.clear();
    tree_ = std::make_shared<Tree>();
    timestamp_ = 0.0;

    // Launch a new thread
    stopreq_ = false;
    reader_ = std::thread(std::bind(&VbcReader::read_file, this, _1), filename);

    return true;
}


bool VbcReader::advance() {
    std::unique_lock<std::mutex> lock(m_);

    // Terminate if there is no next event
    if(fwd_.empty()) {
        return false;
    }

    // Advance to next event
    EventPtr current = fwd_.front();
    Event* ptr = current.get();

    const std::type_index event_type = typeid(*ptr);
    std::shared_ptr<IOErrorEvent> error = std::dynamic_pointer_cast<IOErrorEvent>(current);
    if(error) {
        std::cerr << "IO ERROR: " << error->what() << std::endl;
    }
    else if(event_type != std::type_index(typeid(EOSEvent))) {
        current->apply(tree_);
        if(current->get_time() > timestamp_) {
            timestamp_ = current->get_time();
        }
        fwd_.pop_front();

        if(rewind_) {
            rev_.push_front(current);
        }
    }
    
    return true;
}


bool VbcReader::rewind() {
    std::unique_lock<std::mutex> lock(m_);

    // Stop if we are at the beginning
    if(rev_.empty()) {
        return false;
    }

    // Revert current event
    EventPtr current = rev_.front();
    current->revert(tree_);
    timestamp_ = rev_.empty() ? rev_.front()->get_time() : 0.0;

    return true;
}


void VbcReader::wait() {
    std::unique_lock<std::mutex> lock(m_);
    if(fwd_.empty()) {
        cv_.wait(lock);
    }
}


void VbcReader::close() {
    if(running_) {
        stopreq_ = true;
        if(reader_.joinable()) {
            reader_.join();
        }
    }
}


void VbcReader::clear() {
    close();
    tree_.reset();
    fwd_.clear();
    rev_.clear();
}


VbcReader::State VbcReader::get_state() const {
    std::unique_lock<std::mutex> lock(const_cast<VbcReader*>(this)->m_);
    Event* next_event = fwd_.empty() ? nullptr : fwd_.front().get();

    if(next_event && std::type_index(typeid(*next_event)) == std::type_index(typeid(IOErrorEvent))) {
        return VbcReader::Error;
    }
    else if(next_event && std::type_index(typeid(*next_event)) == std::type_index(typeid(EOSEvent))) {
        return VbcReader::EndOfStream;
    }
    else if(!tree_) {
        return VbcReader::Empty;
    }
    else {
        return VbcReader::Processing;
    }
}


bool VbcReader::has_prev() const {
    std::unique_lock<std::mutex> lock(const_cast<VbcReader*>(this)->m_);
    return !rev_.empty();
}


bool VbcReader::has_next() const {
    std::unique_lock<std::mutex> lock(const_cast<VbcReader*>(this)->m_);
    return !fwd_.empty();
}


double VbcReader::get_timestamp() const {
    return timestamp_;
}

double VbcReader::get_next_timestamp() const {
    std::unique_lock<std::mutex> lock(const_cast<VbcReader*>(this)->m_);
    return fwd_.empty() ? -1.0 : std::max(fwd_.front()->get_time(), timestamp_);
}

