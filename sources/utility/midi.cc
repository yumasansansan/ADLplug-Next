//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#include "utility/midi.h"

Midi_Input_Message Midi_Input_Source::midi_cb_for_buffer_cursor(void *cbdata)
{
    Buffer_Cursor &cursor = *static_cast<Buffer_Cursor *>(cbdata);
    if (cursor.current == cursor.end)
        return {};
    const MidiMessageMetadata event = *cursor.current;
    ++cursor.current;
    return {event.data, static_cast<unsigned>(event.numBytes), event.samplePosition};
}
