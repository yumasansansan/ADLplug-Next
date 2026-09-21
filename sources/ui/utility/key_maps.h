//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018, 2021 Jean Pierre Cimalando
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
Key_Layout load_key_configuration(Midi_Keyboard_Ex &kb, const Configuration &conf);
void build_key_layout_menu(PopupMenu &menu, Key_Layout current);

const char *name_of_key_layout(Key_Layout layout);
Key_Layout key_layout_of_name(const char *name);
