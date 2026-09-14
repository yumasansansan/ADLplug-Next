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

#pragma once
#include "JuceHeader.h"
#include <cstddef>
#include <cstdint>
#include <memory>

// A single-producer, single-consumer byte FIFO whose reads and writes are
// always contiguous: the buffer is allocated twice over and every write is
// mirrored into the other half.
//
// Offsets given to read() and write() count from the start of the pending
// message, and the *_padding() calls round them up to `alignment`. Every
// message is padded that way and the capacity is a multiple of it, so the
// pointers handed out are aligned for any object.
class Simple_Fifo
{
public:
    static constexpr unsigned alignment = static_cast<unsigned>(alignof(std::max_align_t));

    explicit Simple_Fifo(unsigned capacity);

    std::uint8_t *read(unsigned length, unsigned &offset) noexcept;
    bool read_padding(unsigned &offset) const noexcept;
    void finish_read(unsigned length) noexcept
        { fifo_.finishedRead(static_cast<int>(length)); }

    std::uint8_t *write(unsigned length, unsigned &offset) noexcept;
    bool write_padding(unsigned &offset) const noexcept;
    void finish_write(unsigned length) noexcept;

    unsigned get_free_space() const noexcept
        { return static_cast<unsigned>(fifo_.getFreeSpace()); }
    unsigned get_num_ready() const noexcept
        { return static_cast<unsigned>(fifo_.getNumReady()); }

private:
    AbstractFifo fifo_;
    std::unique_ptr<std::uint8_t[]> buffer_;

    static constexpr unsigned pad_offset(unsigned offset) noexcept
        { return (offset + alignment - 1) / alignment * alignment; }
};
