#ifndef __VBC_VBC_READER_HPP
#define __VBC_VBC_READER_HPP

#include <memory>

#include "Event.hpp"
#include "Tree.hpp"

class VbcReader;
typedef std::shared_ptr<VbcReader> VbcReaderPtr;

class VbcReader {
public:
    /// Virtual event placed into event queue to mark end of file.
    class EOSEvent : public Event {
    public:
        EOSEvent();
        virtual void apply(TreePtr tree);
        virtual void revert(TreePtr tree);
    };

    /// Virtual event placed into event queue to mark I/O error.
    class IOErrorEvent : public Event {
    public:
        IOErrorEvent(const std::string& what);

private:
    EventPtr start_;    ///< First event in queue.
    EventPtr current_;  ///< Last event applied.

#endif /* end of include guard: __VBC_VBC_READER_HPP */
