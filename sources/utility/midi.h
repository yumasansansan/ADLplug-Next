//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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
    using callback_function = Midi_Input_Message(void *);

    explicit Midi_Input_Source(callback_function *cb, void *cbdata = nullptr) noexcept
        : cb_(cb), cbdata_(cbdata) {}
    // Position within a MidiBuffer. Must outlive the source that reads it.
    struct Buffer_Cursor {
        MidiBufferIterator current;
        MidiBufferIterator end;
    };
    explicit Midi_Input_Source(Buffer_Cursor &cursor) noexcept
        : cb_(&midi_cb_for_buffer_cursor), cbdata_(&cursor) {}

    Midi_Input_Message get_next_event()
        {
            if (have_next_)
                have_next_ = false;
            else
                next_ = cb_(cbdata_);
            return next_;
        }

    Midi_Input_Message peek_next_event()
        {
            if (!have_next_) {
                next_ = cb_(cbdata_);
                have_next_ = true;
            }
            return next_;
        }

private:
    callback_function *cb_ = nullptr;
    void *cbdata_ = nullptr;
    Midi_Input_Message next_;
    bool have_next_ = false;
    static Midi_Input_Message midi_cb_for_buffer_cursor(void *cbdata);
};
