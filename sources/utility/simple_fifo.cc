//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#include "utility/simple_fifo.h"
#include <algorithm>
#include <cassert>
#include <cstring>

Simple_Fifo::Simple_Fifo(unsigned capacity)
    : fifo_(static_cast<int>(capacity)),
      buffer_(std::make_unique<std::uint8_t[]>(std::size_t{capacity} * 2))
{
    assert(capacity % alignment == 0);
    assert(reinterpret_cast<std::uintptr_t>(buffer_.get()) % alignment == 0);
}

std::uint8_t *Simple_Fifo::read(unsigned length, unsigned &offset) noexcept
{
    int start1, size1, start2, size2;
    fifo_.prepareToRead(static_cast<int>(offset + length), start1, size1, start2, size2);
    if (static_cast<unsigned>(size1 + size2) != offset + length)
        return nullptr;
    std::uint8_t *data = &buffer_[static_cast<std::size_t>(start1) + offset];
    offset += length;
    return data;
}

bool Simple_Fifo::read_padding(unsigned &offset) const noexcept
{
    const unsigned padded = pad_offset(offset);
    if (padded > get_num_ready())
        return false;
    offset = padded;
    return true;
}

std::uint8_t *Simple_Fifo::write(unsigned length, unsigned &offset) noexcept
{
    int start1, size1, start2, size2;
    fifo_.prepareToWrite(static_cast<int>(offset + length), start1, size1, start2, size2);
    if (static_cast<unsigned>(size1 + size2) != offset + length)
        return nullptr;
    std::uint8_t *data = &buffer_[static_cast<std::size_t>(start1) + offset];
    offset += length;
    return data;
}

bool Simple_Fifo::write_padding(unsigned &offset) const noexcept
{
    const unsigned padded = pad_offset(offset);
    if (padded > get_free_space())
        return false;
    offset = padded;
    return true;
}

void Simple_Fifo::finish_write(unsigned length) noexcept
{
    int start1, size1, start2, size2;
    fifo_.prepareToWrite(static_cast<int>(length), start1, size1, start2, size2);
    assert(static_cast<unsigned>(size1 + size2) == length);

    // write() handed out contiguous memory from start1, which may run past the
    // end of the first half. Mirror both parts into the other half.
    const std::size_t capacity = static_cast<std::size_t>(fifo_.getTotalSize());
    const std::size_t start = static_cast<std::size_t>(start1);
    const std::size_t in_first_half = std::min<std::size_t>(length, capacity - start);
    std::uint8_t *buffer = buffer_.get();
    std::memcpy(buffer + start + capacity, buffer + start, in_first_half);
    std::memcpy(buffer, buffer + capacity, length - in_first_half);

    fifo_.finishedWrite(static_cast<int>(length));
}
