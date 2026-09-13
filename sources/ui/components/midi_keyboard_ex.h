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
#include <array>
#include <cstdint>

class Midi_Keyboard_Ex : public MidiKeyboardComponent {
public:
    Midi_Keyboard_Ex(MidiKeyboardState &state, Orientation orientation);
    void highlight_note(unsigned note, unsigned velocity);
    void designate_note(int note);

protected:
    void colourChanged() override;
    void drawWhiteNote(int note, Graphics &g, Rectangle<float> area, bool is_down, bool is_over, Colour line_colour, Colour text_colour) override;
    void drawBlackNote(int note, Graphics &g, Rectangle<float> area, bool is_down, bool is_over, Colour note_fill_colour) override;

private:
    std::array<std::uint8_t, 128> highlight_value_ {};
    int designated_note_ = -1;
    Colour designated_note_color_;
    bool block_colour_changed_callback_ = false; // stops excessive repaints

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Midi_Keyboard_Ex)
};
