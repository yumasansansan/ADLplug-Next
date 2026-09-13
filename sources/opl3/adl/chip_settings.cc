//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "chip_settings.h"
#include "player.h"
#include "resources.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

RESOURCE(Res, emu_dosbox);
RESOURCE(Res, emu_nuked);
RESOURCE(Res, emu_nuked2);
RESOURCE(Res, emu_opal);
RESOURCE(Res, emu_java);

static Emulator_Defaults make_emulator_defaults()
{
    Emulator_Defaults defaults;

    //
    std::vector<std::string> choices = Player::enumerate_emulators();
    unsigned count = (unsigned)choices.size();
    defaults.choices.ensureStorageAllocated(count);
    for (const std::string &choice : choices)
        defaults.choices.add(choice);

    //
    unsigned default_index = ~0u;
    for (unsigned i = 0; i < count && default_index == ~0u; ++i) {
        std::string name = choices[i];
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) -> unsigned char
                           { return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c; });
        if (name.size() >= 6 && !memcmp(name.data(), "dosbox", 6))
            default_index = i;
    }
    defaults.default_index = (default_index != ~0u) ? default_index : 0;

    return defaults;
}

const Emulator_Defaults &get_emulator_defaults()
{
    static const Emulator_Defaults defaults = make_emulator_defaults();
    return defaults;
}

Emulator_Icons::Emulator_Icons()
{
    const Emulator_Defaults &defaults = get_emulator_defaults();
    unsigned count = (unsigned)defaults.choices.size();

    images.resize(count);
    Image icon_dosbox = ImageFileFormat::loadFrom(Res::emu_dosbox.data, Res::emu_dosbox.size);
    Image icon_nuked = ImageFileFormat::loadFrom(Res::emu_nuked.data, Res::emu_nuked.size);
    Image icon_nuked2 = ImageFileFormat::loadFrom(Res::emu_nuked2.data, Res::emu_nuked2.size);
    Image icon_opal = ImageFileFormat::loadFrom(Res::emu_opal.data, Res::emu_opal.size);
    Image icon_java = ImageFileFormat::loadFrom(Res::emu_java.data, Res::emu_java.size);
    unsigned nth_icon_nuked = 0;
    for (unsigned i = 0; i < count; ++i) {
        const String &name = defaults.choices[i];
        String lowerName = name.toLowerCase();
        if (lowerName.startsWith("dosbox"))
            images[i] = icon_dosbox;
        else if (lowerName.startsWith("nuked"))
            images[i] = (nth_icon_nuked++ == 0) ? icon_nuked : icon_nuked2;
        else if (lowerName.startsWith("opal"))
            images[i] = icon_opal;
        else if (lowerName.startsWith("java"))
            images[i] = icon_java;
    }
}

PropertySet Chip_Settings::to_properties() const
{
    PropertySet set;
    set.setValue("emulator", (int)emulator);
    set.setValue("chip_count", (int)chip_count);
    set.setValue("4op_count", (int)fourop_count);
    return set;
}

Chip_Settings Chip_Settings::from_properties(const PropertySet &set)
{
    Chip_Settings cs;
    cs.emulator = set.getIntValue("emulator");
    cs.chip_count = set.getIntValue("chip_count");
    cs.fourop_count = set.getIntValue("4op_count");
    return cs;
}
