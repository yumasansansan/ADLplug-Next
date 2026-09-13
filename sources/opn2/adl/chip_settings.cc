//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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

    const Image icon_mame = load(Res::emu_mame);
    const Image icon_nuked = load(Res::emu_nuked);
    const Image icon_gens = load(Res::emu_gens);
    const Image icon_neko = load(Res::emu_neko);

    images.resize(static_cast<std::size_t>(defaults.choices.size()));
    for (std::size_t i = 0; i < images.size(); ++i) {
        const String &name = defaults.choices[static_cast<int>(i)];
        if (name.isEmpty())
            continue;
        switch (static_cast<int>(i)) {
        case OPNMIDI_EMU_MAME:
        case OPNMIDI_EMU_MAME_2608:
            images[i] = icon_mame;
            break;
        // Nuke.YKT's cores, including the low-level ones.
        case OPNMIDI_EMU_NUKED_YM3438:
        case OPNMIDI_EMU_NUKED_YM2612:
        case OPNMIDI_EMU_NUKED_YM2612_LLE:
        case OPNMIDI_EMU_NUKED_YM2608_LLE:
        case OPNMIDI_EMU_NUKED_YM3438_LLE:
        case OPNMIDI_EMU_NUKED_YMF276_LLE:
            images[i] = icon_nuked;
            break;
        case OPNMIDI_EMU_GENS:
            images[i] = icon_gens;
            break;
        case OPNMIDI_EMU_NP2:
            images[i] = icon_neko;
            break;
        default:
            // Cores without a logo among the resources -- ymfm has none in its
            // repository -- show the first word of their name.
            images[i] = Image_Utils::make_text_icon(name.upToFirstOccurrenceOf(" ", false, false));
            break;
        }
    }
}

PropertySet Chip_Settings::to_properties() const
{
    PropertySet set;
    set.setValue("emulator", static_cast<int>(emulator));
    set.setValue("chip_count", static_cast<int>(chip_count));
    set.setValue("chip_type", static_cast<int>(chip_type));
    return set;
}

Chip_Settings Chip_Settings::from_properties(const PropertySet &set)
{
    Chip_Settings cs;
    cs.emulator = non_negative(set.getIntValue("emulator"));
    cs.chip_count = non_negative(set.getIntValue("chip_count"));
    cs.chip_type = non_negative(set.getIntValue("chip_type"));
    return cs;
}
