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

#include "instrument.h"
#include "JuceHeader.h"
#include <algorithm>
#include <cstdint>
#include <limits>

namespace {

// Saved states come from project files, so saturate values into their fields.
template <class T>
T clamp_to(int value) noexcept
{
    return static_cast<T>(std::clamp<int>(value, std::numeric_limits<T>::min(), std::numeric_limits<T>::max()));
}

constexpr const char *op_prefix[4] = { "op1", "op3", "op2", "op4" };

}  // namespace

PropertySet Instrument::to_properties() const
{
    PropertySet set;

    set.setValue("pseudo_eight_op", pseudo_eight_op());
    set.setValue("blank", blank());
    set.setValue("note_offset", note_offset);
    set.setValue("feedback", feedback());
    set.setValue("algorithm", algorithm());
    set.setValue("ams", ams());
    set.setValue("fms", fms());
    set.setValue("midi_velocity_offset", midi_velocity_offset);
    set.setValue("percussion_key_number", percussion_key_number);

    for (unsigned opnum = 0; opnum < 4; ++opnum) {
        const String opfx = op_prefix[opnum];
        set.setValue(opfx + "detune", detune(opnum));
        set.setValue(opfx + "fmul", fmul(opnum));
        set.setValue(opfx + "level", level(opnum));
        set.setValue(opfx + "ratescale", ratescale(opnum));
        set.setValue(opfx + "attack", attack(opnum));
        set.setValue(opfx + "am", am(opnum));
        set.setValue(opfx + "decay1", decay1(opnum));
        set.setValue(opfx + "decay2", decay2(opnum));
        set.setValue(opfx + "sustain", sustain(opnum));
        set.setValue(opfx + "release", release(opnum));
        set.setValue(opfx + "ssgenable", ssgenable(opnum));
        set.setValue(opfx + "ssgwave", ssgwave(opnum));
    }

    set.setValue("delay_off_ms", delay_off_ms);
    set.setValue("delay_on_ms", delay_on_ms);

    return set;
}

Instrument Instrument::from_properties(const juce::PropertySet &set)
{
    Instrument ins;

    ins.pseudo_eight_op(set.getBoolValue("pseudo_eight_op"));
    ins.blank(set.getBoolValue("blank"));
    ins.note_offset = clamp_to<std::int16_t>(set.getIntValue("note_offset"));
    ins.feedback(set.getIntValue("feedback"));
    ins.algorithm(set.getIntValue("algorithm"));
    ins.ams(set.getIntValue("ams"));
    ins.fms(set.getIntValue("fms"));
    ins.midi_velocity_offset = clamp_to<std::int8_t>(set.getIntValue("midi_velocity_offset"));
    ins.percussion_key_number = clamp_to<std::uint8_t>(set.getIntValue("percussion_key_number"));

    for (unsigned opnum = 0; opnum < 4; ++opnum) {
        const String opfx = op_prefix[opnum];
        ins.detune(opnum, set.getIntValue(opfx + "detune"));
        ins.fmul(opnum, set.getIntValue(opfx + "fmul"));
        ins.level(opnum, set.getIntValue(opfx + "level"));
        ins.ratescale(opnum, set.getIntValue(opfx + "ratescale"));
        ins.attack(opnum, set.getIntValue(opfx + "attack"));
        ins.am(opnum, set.getBoolValue(opfx + "am"));
        ins.decay1(opnum, set.getIntValue(opfx + "decay1"));
        ins.decay2(opnum, set.getIntValue(opfx + "decay2"));
        ins.sustain(opnum, set.getIntValue(opfx + "sustain"));
        ins.release(opnum, set.getIntValue(opfx + "release"));
        ins.ssgenable(opnum, set.getBoolValue(opfx + "ssgenable"));
        ins.ssgwave(opnum, set.getIntValue(opfx + "ssgwave"));
    }

    ins.delay_off_ms = clamp_to<std::uint16_t>(set.getIntValue("delay_off_ms"));
    ins.delay_on_ms = clamp_to<std::uint16_t>(set.getIntValue("delay_on_ms"));

    return ins;
}

PropertySet Instrument_Global_Parameters::to_properties() const
{
    PropertySet set;
    set.setValue("volume_model", volume_model);
    set.setValue("lfo_enable", lfo_enable);
    set.setValue("lfo_frequency", lfo_frequency);
    return set;
}

Instrument_Global_Parameters Instrument_Global_Parameters::from_properties(const PropertySet &set)
{
    Instrument_Global_Parameters gp;
    gp.volume_model = std::max(0, set.getIntValue("volume_model"));
    gp.lfo_enable = set.getBoolValue("lfo_enable");
    gp.lfo_frequency = std::max(0, set.getIntValue("lfo_frequency"));
    return gp;
}
