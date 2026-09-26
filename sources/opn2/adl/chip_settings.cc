//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018-2019 Jean Pierre Cimalando
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

#include "chip_settings.h"
#include "player.h"
#include "resources.h"
#include "ui/utility/image.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

namespace {

Emulator_Defaults make_emulator_defaults()
{
    Emulator_Defaults defaults;

    const std::vector<std::string> choices = Player::enumerate_emulators();
    defaults.choices.ensureStorageAllocated(static_cast<int>(choices.size()));
    for (const std::string &choice : choices)
        defaults.choices.add(choice);

    // Always built: the measurer runs on it (see cmake/ADLMIDI.cmake).
    defaults.default_index = static_cast<unsigned>(OPNMIDI_EMU_MAME);
    assert(defaults.is_built(defaults.default_index));

    return defaults;
}

unsigned non_negative(int value) noexcept
{
    return static_cast<unsigned>(std::max(0, value));
}

}  // namespace

const Emulator_Defaults &get_emulator_defaults()
{
    static const Emulator_Defaults defaults = make_emulator_defaults();
    return defaults;
}

unsigned available_emulator(unsigned index)
{
    const Emulator_Defaults &defaults = get_emulator_defaults();
    if (defaults.is_built(index))
        return index;

    // An OPNA core is replaced by MAME's YM2608 core, which emulates the same
    // chip; any other core by the default, an OPN2 one.
    switch (static_cast<int>(index)) {
    case OPNMIDI_EMU_NP2:
    case OPNMIDI_EMU_YMFM_OPNA:
    case OPNMIDI_EMU_NUKED_YM2608_LLE:
        if (defaults.is_built(static_cast<unsigned>(OPNMIDI_EMU_MAME_2608)))
            return static_cast<unsigned>(OPNMIDI_EMU_MAME_2608);
        break;
    default:
        break;
    }
    return defaults.default_index;
}

Emulator_Icons::Emulator_Icons()
{
    const Emulator_Defaults &defaults = get_emulator_defaults();
    const auto load = [](const Res::Data &res) { return ImageFileFormat::loadFrom(res.data, res.size); };

    const Image icon_nuked = load(Res::emu_nuked);

    // The labels are gathered and drawn together, into one image, afterwards
    // (Image_Utils::make_text_icons).
    StringArray labels;
    std::vector<std::size_t> labelled;

    images.resize(static_cast<std::size_t>(defaults.choices.size()));
    for (std::size_t i = 0; i < images.size(); ++i) {
        const String &name = defaults.choices[static_cast<int>(i)];
        if (name.isEmpty())
            continue;
        switch (static_cast<int>(i)) {
        // Nuke.YKT's cores, including the low-level ones.
        case OPNMIDI_EMU_NUKED_YM3438:
        case OPNMIDI_EMU_NUKED_YM2612:
        case OPNMIDI_EMU_NUKED_YM2612_LLE:
        case OPNMIDI_EMU_NUKED_YM2608_LLE:
        case OPNMIDI_EMU_NUKED_YM3438_LLE:
        case OPNMIDI_EMU_NUKED_YMF276_LLE:
            images[i] = icon_nuked;
            break;
        default:
            // The other cores show the first word of their name. The resources
            // hold only the logos whose origin and license are known
            // (REUSE.toml): ymfm has none, and the ones that ADLplug had for
            // MAME, GENS and Neko Project II could not be traced.
            labels.add(name.upToFirstOccurrenceOf(" ", false, false));
            labelled.push_back(i);
            break;
        }
    }

    const std::vector<Image> drawn = Image_Utils::make_text_icons(labels);
    for (std::size_t k = 0; k < labelled.size() && k < drawn.size(); ++k)
        images[labelled[k]] = drawn[k];
}

PropertySet Chip_Settings::to_properties() const
{
    PropertySet set;
    set.setValue("emulator", static_cast<int>(emulator));
    set.setValue("chip_count", static_cast<int>(chip_count));
    set.setValue("chip_type", static_cast<int>(chip_type));
    set.setValue("chan_alloc", chan_alloc);
    return set;
}

Chip_Settings Chip_Settings::from_properties(const PropertySet &set)
{
    Chip_Settings cs;
    cs.emulator = non_negative(set.getIntValue("emulator"));
    cs.chip_count = non_negative(set.getIntValue("chip_count"));
    cs.chip_type = non_negative(set.getIntValue("chip_type"));
    // A project that has none of this is one from before the mode was a setting,
    // and the library's own choice is what it meant. A number that is no mode is
    // held to the ones there are where the player is given it
    // (playable_chip_settings).
    cs.chan_alloc = set.getIntValue("chan_alloc", -1);
    return cs;
}
