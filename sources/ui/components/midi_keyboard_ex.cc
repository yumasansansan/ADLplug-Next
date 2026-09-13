//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#include "midi_keyboard_ex.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

constexpr int gray_min = 0xa0;
constexpr int gray_max = 0xa0;

// The key-down overlay of a note highlighted with a velocity from 1 to 127.
Colour highlight_colour(std::uint8_t velocity)
{
    const double amount = (velocity - 1) / 126.0;
    const auto gray = static_cast<std::uint8_t>(std::lround(gray_min + amount * (gray_max - gray_min)));
    return Colour(gray, gray, gray);
}

}  // namespace

Midi_Keyboard_Ex::Midi_Keyboard_Ex(MidiKeyboardState &state, Orientation orientation)
    : MidiKeyboardComponent(state, orientation),
      designated_note_color_(static_cast<std::uint8_t>(245), 0, 41, 0.5f)
{
}

void Midi_Keyboard_Ex::highlight_note(unsigned note, unsigned velocity)
{
    if (note >= highlight_value_.size())
        return;

    const auto value = static_cast<std::uint8_t>(std::min(velocity, 127u));
    if (highlight_value_[note] == value)
        return;

    highlight_value_[note] = value;
    repaint(getRectangleForKey(static_cast<int>(note)).toNearestInt());
}

void Midi_Keyboard_Ex::designate_note(int note)
{
    if (note == designated_note_)
        return;

    designated_note_ = note;
    repaint();
}

void Midi_Keyboard_Ex::colourChanged()
{
    if (!block_colour_changed_callback_)
        MidiKeyboardComponent::colourChanged();
}

void Midi_Keyboard_Ex::drawWhiteNote(int note, Graphics &g, Rectangle<float> area, bool is_down, bool is_over, Colour line_colour, Colour text_colour)
{
    jassert(note >= 0 && note < 128);

    const std::uint8_t hl = highlight_value_[static_cast<std::size_t>(note)];

    Colour orig_colour;
    if (hl > 0) {
        orig_colour = findColour(keyDownOverlayColourId);
        block_colour_changed_callback_ = true;
        setColour(keyDownOverlayColourId, highlight_colour(hl));
        block_colour_changed_callback_ = false;
    }

    MidiKeyboardComponent::drawWhiteNote(note, g, area, is_down || hl > 0, is_over, line_colour, text_colour);

    if (hl > 0) {
        block_colour_changed_callback_ = true;
        setColour(keyDownOverlayColourId, orig_colour);
        block_colour_changed_callback_ = false;
    }

    if (note == designated_note_) {
        const float w = area.getWidth();
        const float r = w * 0.7f * getBlackNoteWidthProportion();
        g.setColour(designated_note_color_);
        g.fillEllipse(area.getX() + 0.5f * (w - r), area.getBottom() - 1.5f * r, r, r);
    }
}

void Midi_Keyboard_Ex::drawBlackNote(int note, Graphics &g, Rectangle<float> area, bool is_down, bool is_over, Colour note_fill_colour)
{
    jassert(note >= 0 && note < 128);

    const std::uint8_t hl = highlight_value_[static_cast<std::size_t>(note)];

    Colour orig_colour;
    if (hl > 0) {
        orig_colour = findColour(keyDownOverlayColourId);
        block_colour_changed_callback_ = true;
        setColour(keyDownOverlayColourId, highlight_colour(hl));
        block_colour_changed_callback_ = false;
    }

    MidiKeyboardComponent::drawBlackNote(note, g, area, is_down || hl > 0, is_over, note_fill_colour);

    if (hl > 0) {
        block_colour_changed_callback_ = true;
        setColour(keyDownOverlayColourId, orig_colour);
        block_colour_changed_callback_ = false;
    }

    if (note == designated_note_) {
        const float w = area.getWidth();
        const float r = w * 0.7f;
        g.setColour(designated_note_color_);
        g.fillEllipse(area.getX() + 0.5f * (w - r), area.getBottom() - 1.5f * r, r, r);
    }
}
