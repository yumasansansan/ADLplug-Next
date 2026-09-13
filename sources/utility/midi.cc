//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "utility/midi.h"

Midi_Input_Message Midi_Input_Source::midi_cb_for_buffer_cursor(void *cbdata)
{
    Buffer_Cursor &cursor = *(Buffer_Cursor *)cbdata;
    if (cursor.current == cursor.end)
        return Midi_Input_Message();
    const MidiMessageMetadata event = *cursor.current;
    ++cursor.current;
    return Midi_Input_Message(event.data, (unsigned)event.numBytes, event.samplePosition);
}
