//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#include "messages.h"

namespace Messages {

Buffered_Message read(Simple_Fifo &fifo) noexcept
{
    Buffered_Message msg;
    std::uint8_t *header = fifo.read(sizeof(Message_Header), msg.offset);
    if (!header || !fifo.read_padding(msg.offset))
        return {};
    msg.header = static_cast<Message_Header *>(static_cast<void *>(header));
    msg.data = fifo.read(msg.header->size, msg.offset);
    if (!msg.data || !fifo.read_padding(msg.offset))
        return {};
    return msg;
}

void finish_read(Simple_Fifo &fifo, const Buffered_Message &msg) noexcept
{
    fifo.finish_read(msg.offset);
}

Buffered_Message write(Simple_Fifo &fifo, unsigned tag, unsigned size) noexcept
{
    Buffered_Message msg;
    std::uint8_t *header = fifo.write(sizeof(Message_Header), msg.offset);
    if (!header || !fifo.write_padding(msg.offset))
        return {};
    msg.header = static_cast<Message_Header *>(static_cast<void *>(header));
    *msg.header = Message_Header{tag, size};
    msg.data = fifo.write(size, msg.offset);
    if (!msg.data || !fifo.write_padding(msg.offset))
        return {};
    return msg;
}

void finish_write(Simple_Fifo &fifo, const Buffered_Message &msg) noexcept
{
    fifo.finish_write(msg.offset);
}

}  // namespace Messages
