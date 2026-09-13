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
#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool starts_with_ignoring_ascii_case(std::string_view text, std::string_view lowercase_prefix) noexcept
{
    const auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
    return text.size() >= lowercase_prefix.size() &&
           std::ranges::equal(text.substr(0, lowercase_prefix.size()), lowercase_prefix,
                              [&lower](char a, char b) { return lower(a) == b; });
}

Emulator_Defaults make_emulator_defaults()
{
    Emulator_Defaults defaults;

    const std::vector<std::string> choices = Player::enumerate_emulators();
    defaults.choices.ensureStorageAllocated(static_cast<int>(choices.size()));
    for (const std::string &choice : choices)
        defaults.choices.add(choice);

    const auto it = std::ranges::find_if(choices, [](const std::string &name) {
        return starts_with_ignoring_ascii_case(name, "dosbox");
    });
    defaults.default_index = (it != choices.end()) ? static_cast<unsigned>(it - choices.begin()) : 0;

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

Emulator_Icons::Emulator_Icons()
{
    const Emulator_Defaults &defaults = get_emulator_defaults();
    const auto load = [](const Res::Data &res) { return ImageFileFormat::loadFrom(res.data, res.size); };

    const Image icon_dosbox = load(Res::emu_dosbox);
    const Image icon_nuked = load(Res::emu_nuked);
    const Image icon_nuked2 = load(Res::emu_nuked2);
    const Image icon_opal = load(Res::emu_opal);
    const Image icon_java = load(Res::emu_java);

    images.resize(static_cast<std::size_t>(defaults.choices.size()));
    unsigned nth_icon_nuked = 0;
    for (std::size_t i = 0; i < images.size(); ++i) {
        const String name = defaults.choices[static_cast<int>(i)].toLowerCase();
        if (name.startsWith("dosbox"))
            images[i] = icon_dosbox;
        else if (name.startsWith("nuked"))
            images[i] = (nth_icon_nuked++ == 0) ? icon_nuked : icon_nuked2;
        else if (name.startsWith("opal"))
            images[i] = icon_opal;
        else if (name.startsWith("java"))
            images[i] = icon_java;
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
