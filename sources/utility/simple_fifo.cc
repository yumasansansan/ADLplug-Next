//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: BSL-1.0 AND GPL-3.0-or-later
//
// This file comes from ADLplug and was modified for ADLplug-Next. The notice at
// the top is ADLplug's; the LICENSE it names was ADLplug's copy of the Boost
// Software License, now LICENSES/BSL-1.0.txt. The SPDX lines name the copyright
// holders and licenses in the machine-readable form of the REUSE specification:
// ADLplug's code is under the Boost Software License 1.0, and ADLplug-Next's
// changes are under the GNU General Public License, version 3 or any later
// version (LICENSES/GPL-3.0-or-later.txt).

#include "utility/simple_fifo.h"
#include <algorithm>
#include <cassert>
#include <cstring>

Simple_Fifo::Simple_Fifo(unsigned capacity)
    : fifo_(static_cast<int>(capacity)),
      buffer_(std::make_unique<std::uint8_t[]>(std::size_t{capacity} * 2)),
      capacity_(capacity)
{
}

std::uint8_t *Simple_Fifo::read(unsigned length, unsigned &offset) noexcept
{
    // A message never holds more than the FIFO, and the sum below cannot wrap.
    if (offset > capacity_ || length > capacity_ - offset)
        return nullptr;
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo_.prepareToRead(static_cast<int>(offset + length), start1, size1, start2, size2);
    if (static_cast<unsigned>(size1 + size2) != offset + length)
        return nullptr;
    std::uint8_t *data = &buffer_[static_cast<std::size_t>(start1) + offset];
    offset += length;
    return data;
}

std::uint8_t *Simple_Fifo::write(unsigned length, unsigned &offset) noexcept
{
    if (offset > capacity_ || length > capacity_ - offset)
        return nullptr;
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo_.prepareToWrite(static_cast<int>(offset + length), start1, size1, start2, size2);
    if (static_cast<unsigned>(size1 + size2) != offset + length)
        return nullptr;
    std::uint8_t *data = &buffer_[static_cast<std::size_t>(start1) + offset];
    offset += length;
    return data;
}

void Simple_Fifo::finish_write(unsigned length) noexcept
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo_.prepareToWrite(static_cast<int>(length), start1, size1, start2, size2);
    assert(static_cast<unsigned>(size1 + size2) == length);

    // write() handed out contiguous memory from start1, which may run past the
    // end of the first half. Mirror both parts into the other half.
    const std::size_t capacity = capacity_;
    const std::size_t start = static_cast<std::size_t>(start1);
    const std::size_t in_first_half = std::min<std::size_t>(length, capacity - start);
    std::uint8_t *buffer = buffer_.get();
    std::memcpy(buffer + start + capacity, buffer + start, in_first_half);
    std::memcpy(buffer, buffer + capacity, length - in_first_half);

    fifo_.finishedWrite(static_cast<int>(length));
}
