//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018-2019, 2021 Jean Pierre Cimalando
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
