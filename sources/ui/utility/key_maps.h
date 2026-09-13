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
#include "../components/midi_keyboard_ex.h"
#include <array>
class Configuration;

enum class Key_Layout {
    Qwerty, Qwertz, Azerty,
    Default = Qwerty,
};

extern const std::array<const char *, 3> key_layout_names;

// The keys which play the notes upwards from C in a layout, unless the
// configuration maps other ones.
String default_key_map(Key_Layout layout);

Key_Layout set_key_layout(Midi_Keyboard_Ex &kb, Key_Layout layout, Configuration &conf);
Key_Layout load_key_configuration(Midi_Keyboard_Ex &kb, Configuration &conf);
void build_key_layout_menu(PopupMenu &menu, Key_Layout current);

const char *name_of_key_layout(Key_Layout layout);
Key_Layout key_layout_of_name(const char *name);
