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
// message. The FIFO holds bytes only: what goes through it is copied in and
// out of them (messages.h), and nothing is used in place as an object.
class Simple_Fifo
{
public:
    explicit Simple_Fifo(unsigned capacity);

    // `length` bytes at `offset`, or null if fewer are ready.
    std::uint8_t *read(unsigned length, unsigned &offset) noexcept;
    void finish_read(unsigned length) noexcept
        { fifo_.finishedRead(static_cast<int>(length)); }

    // Room for `length` bytes at `offset`, or null if there is less.
    std::uint8_t *write(unsigned length, unsigned &offset) noexcept;
    void finish_write(unsigned length) noexcept;

    [[nodiscard]] unsigned get_free_space() const noexcept
        { return static_cast<unsigned>(fifo_.getFreeSpace()); }
    [[nodiscard]] unsigned get_num_ready() const noexcept
        { return static_cast<unsigned>(fifo_.getNumReady()); }

private:
    AbstractFifo fifo_;
    std::unique_ptr<std::uint8_t[]> buffer_;
    unsigned capacity_ = 0;
};
