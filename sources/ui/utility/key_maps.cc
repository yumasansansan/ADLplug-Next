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

#include "key_maps.h"
#include "configuration.h"
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

const std::array<const char *, 3> key_layout_names {
    "qwerty",
    "qwertz",
    "azerty",
};

namespace {

constexpr std::array<std::u32string_view, 3> key_layout_maps {
    U"zsxdcvgbhnjmq2w3er5t6y7ui9o0p",
    U"ysxdcvgbhnjmq2w3er5t6z7ui9o0p",
    U"wsxdcvgbhnj,aéz\"er(t-yèuiçoàp",
};

std::size_t index_of_key_layout(Key_Layout layout) noexcept
{
    const auto index = static_cast<std::size_t>(layout);
    return (index < key_layout_names.size()) ? index : static_cast<std::size_t>(Key_Layout::Default);
}

}  // namespace

String default_key_map(Key_Layout layout)
{
    String keys;
    for (const char32_t key : key_layout_maps[index_of_key_layout(layout)])
        keys += static_cast<juce_wchar>(key);
    return keys;
}

Key_Layout set_key_layout(Midi_Keyboard_Ex &kb, Key_Layout layout, Configuration &conf)
{
    kb.clearKeyMappings();

    conf.set_string("piano", "layout", name_of_key_layout(layout));
    layout = load_key_configuration(kb, conf);
    conf.save_default();
    return layout;
}

Key_Layout load_key_configuration(Midi_Keyboard_Ex &kb, Configuration &conf)
{
    const Key_Layout layout = key_layout_of_name(conf.get_string("piano", "layout", key_layout_names[0]));
    const std::string keymap_key = std::string("keymap:") + name_of_key_layout(layout);

    String keymap;
    if (const char *value = conf.get_string("piano", keymap_key.c_str(), nullptr))
        keymap = String::fromUTF8(value);
    else
        keymap = default_key_map(layout);

    kb.clearKeyMappings();
    int note = 0;
    for (auto p = keymap.getCharPointer(); !p.isEmpty();)
        kb.setKeyPressForNote(KeyPress(static_cast<int>(p.getAndAdvance()), 0, 0), note++);

    return layout;
}

void build_key_layout_menu(PopupMenu &menu, Key_Layout current)
{
    for (std::size_t i = 0; i < key_layout_names.size(); ++i)
        menu.addItem(
            static_cast<int>(i + 1),
            "Use " + String(key_layout_names[i]).toUpperCase() + " keys",
            true, static_cast<Key_Layout>(i) == current);
}

const char *name_of_key_layout(Key_Layout layout)
{
    return key_layout_names[index_of_key_layout(layout)];
}

Key_Layout key_layout_of_name(const char *name)
{
    for (std::size_t i = 0; name && i < key_layout_names.size(); ++i) {
        if (std::strcmp(name, key_layout_names[i]) == 0)
            return static_cast<Key_Layout>(i);
    }
    return Key_Layout::Default;
}
