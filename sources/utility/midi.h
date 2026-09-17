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
#include <cstdint>

struct Midi_Input_Message
{
    const std::uint8_t *data = nullptr;
    unsigned size = 0;
    int time = 0;

    explicit operator bool() const noexcept
        { return data != nullptr; }
};

class Midi_Input_Source {
public:
    // Position within a MidiBuffer. Must outlive the source that reads it.
    struct Buffer_Cursor {
        MidiBufferIterator current;
        MidiBufferIterator end;
    };
    explicit Midi_Input_Source(Buffer_Cursor &cursor) noexcept
        : cursor_(cursor) {}

    Midi_Input_Message get_next_event()
        {
            if (have_next_)
                have_next_ = false;
            else
                next_ = read_event();
            return next_;
        }

    Midi_Input_Message peek_next_event()
        {
            if (!have_next_) {
                next_ = read_event();
                have_next_ = true;
            }
            return next_;
        }

private:
    Buffer_Cursor &cursor_;
    Midi_Input_Message next_;
    bool have_next_ = false;
    Midi_Input_Message read_event();
};
