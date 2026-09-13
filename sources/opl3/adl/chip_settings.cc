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
    defaults.default_index = static_cast<unsigned>(ADLMIDI_EMU_DOSBOX);
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

    // An OPL2 core is replaced by DOSBox in OPL2 mode, which emulates the same
    // chip; any other core by the default, an OPL3 one.
    switch (static_cast<int>(index)) {
    case ADLMIDI_EMU_MAME_OPL2:
    case ADLMIDI_EMU_YMFM_OPL2:
    case ADLMIDI_EMU_NUKED_OPL2_LLE:
    case ADLMIDI_EMU_NUKED_OPL2_LITE:
        if (defaults.is_built(static_cast<unsigned>(ADLMIDI_EMU_DOSBOX_OPL2)))
            return static_cast<unsigned>(ADLMIDI_EMU_DOSBOX_OPL2);
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

    const Image icon_dosbox = load(Res::emu_dosbox);
    const Image icon_nuked = load(Res::emu_nuked);
    const Image icon_nuked2 = load(Res::emu_nuked2);
    const Image icon_opal = load(Res::emu_opal);
    const Image icon_java = load(Res::emu_java);
    const Image icon_esfmu = load(Res::emu_esfmu);
    const Image icon_mame = load(Res::emu_mame);

    images.resize(static_cast<std::size_t>(defaults.choices.size()));
    for (std::size_t i = 0; i < images.size(); ++i) {
        const String &name = defaults.choices[static_cast<int>(i)];
        if (name.isEmpty())
            continue;
        switch (static_cast<int>(i)) {
        case ADLMIDI_EMU_DOSBOX:
        case ADLMIDI_EMU_DOSBOX_OPL2:
            images[i] = icon_dosbox;
            break;
        // Nuke.YKT's cores, including the low-level ones.
        case ADLMIDI_EMU_NUKED:
        case ADLMIDI_EMU_NUKED_OPL2_LITE:
        case ADLMIDI_EMU_NUKED_CQM:
        case ADLMIDI_EMU_NUKED_OPL2_LLE:
        case ADLMIDI_EMU_NUKED_OPL3_LLE:
            images[i] = icon_nuked;
            break;
        // tgies' fork of Nuked OPL3 took the place of Nuked OPL3 1.7.4 in
        // libADLMIDI, and takes the second icon that one had.
        case ADLMIDI_EMU_NUKED_FAST:
            images[i] = icon_nuked2;
            break;
        case ADLMIDI_EMU_OPAL:
            images[i] = icon_opal;
            break;
        case ADLMIDI_EMU_JAVA:
            images[i] = icon_java;
            break;
        case ADLMIDI_EMU_ESFMu:
            images[i] = icon_esfmu;
            break;
        case ADLMIDI_EMU_MAME_OPL2:
            images[i] = icon_mame;
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
    set.setValue("4op_count", static_cast<int>(fourop_count));
    return set;
}

Chip_Settings Chip_Settings::from_properties(const PropertySet &set)
{
    Chip_Settings cs;
    cs.emulator = non_negative(set.getIntValue("emulator"));
    cs.chip_count = non_negative(set.getIntValue("chip_count"));
    cs.fourop_count = non_negative(set.getIntValue("4op_count"));
    return cs;
}
