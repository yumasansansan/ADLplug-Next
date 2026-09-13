//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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

constexpr const char *op_prefix[4] = { "c1", "m1", "c2", "m2" };

}  // namespace

PropertySet Instrument::to_properties() const
{
    PropertySet set;

    set.setValue("four_op", four_op());
    set.setValue("pseudo_four_op", pseudo_four_op());
    set.setValue("blank", blank());
    set.setValue("con12", con12());
    set.setValue("con34", con34());
    set.setValue("note_offset1", note_offset1);
    set.setValue("note_offset2", note_offset2);
    set.setValue("fb12", fb12());
    set.setValue("fb34", fb34());
    set.setValue("midi_velocity_offset", midi_velocity_offset);
    set.setValue("second_voice_detune", second_voice_detune);
    set.setValue("percussion_key_number", percussion_key_number);

    for (unsigned opnum = 0; opnum < 4; ++opnum) {
        const String opfx = op_prefix[opnum];
        set.setValue(opfx + "attack", attack(opnum));
        set.setValue(opfx + "decay", decay(opnum));
        set.setValue(opfx + "sustain", sustain(opnum));
        set.setValue(opfx + "release", release(opnum));
        set.setValue(opfx + "level", level(opnum));
        set.setValue(opfx + "ksl", ksl(opnum));
        set.setValue(opfx + "fmul", fmul(opnum));
        set.setValue(opfx + "trem", trem(opnum));
        set.setValue(opfx + "vib", vib(opnum));
        set.setValue(opfx + "sus", sus(opnum));
        set.setValue(opfx + "env", env(opnum));
        set.setValue(opfx + "wave", wave(opnum));
    }

    set.setValue("delay_off_ms", delay_off_ms);
    set.setValue("delay_on_ms", delay_on_ms);

    return set;
}

Instrument Instrument::from_properties(const juce::PropertySet &set)
{
    Instrument ins;

    ins.four_op(set.getBoolValue("four_op"));
    ins.pseudo_four_op(set.getBoolValue("pseudo_four_op"));
    ins.blank(set.getBoolValue("blank"));
    ins.con12(set.getBoolValue("con12"));
    ins.con34(set.getBoolValue("con34"));
    ins.note_offset1 = clamp_to<std::int16_t>(set.getIntValue("note_offset1"));
    ins.note_offset2 = clamp_to<std::int16_t>(set.getIntValue("note_offset2"));
    ins.fb12(set.getIntValue("fb12"));
    ins.fb34(set.getIntValue("fb34"));
    ins.midi_velocity_offset = clamp_to<std::int8_t>(set.getIntValue("midi_velocity_offset"));
    ins.second_voice_detune = clamp_to<std::int8_t>(set.getIntValue("second_voice_detune"));
    ins.percussion_key_number = clamp_to<std::uint8_t>(set.getIntValue("percussion_key_number"));

    for (unsigned opnum = 0; opnum < 4; ++opnum) {
        const String opfx = op_prefix[opnum];
        ins.attack(opnum, set.getIntValue(opfx + "attack"));
        ins.decay(opnum, set.getIntValue(opfx + "decay"));
        ins.sustain(opnum, set.getIntValue(opfx + "sustain"));
        ins.release(opnum, set.getIntValue(opfx + "release"));
        ins.level(opnum, set.getIntValue(opfx + "level"));
        ins.ksl(opnum, set.getIntValue(opfx + "ksl"));
        ins.fmul(opnum, set.getIntValue(opfx + "fmul"));
        ins.trem(opnum, set.getBoolValue(opfx + "trem"));
        ins.vib(opnum, set.getBoolValue(opfx + "vib"));
        ins.sus(opnum, set.getBoolValue(opfx + "sus"));
        ins.env(opnum, set.getBoolValue(opfx + "env"));
        ins.wave(opnum, set.getIntValue(opfx + "wave"));
    }

    ins.delay_off_ms = clamp_to<std::uint16_t>(set.getIntValue("delay_off_ms"));
    ins.delay_on_ms = clamp_to<std::uint16_t>(set.getIntValue("delay_on_ms"));

    return ins;
}

PropertySet Instrument_Global_Parameters::to_properties() const
{
    PropertySet set;
    set.setValue("volume_model", volume_model);
    set.setValue("deep_tremolo", deep_tremolo);
    set.setValue("deep_vibrato", deep_vibrato);
    return set;
}

Instrument_Global_Parameters Instrument_Global_Parameters::from_properties(const PropertySet &set)
{
    Instrument_Global_Parameters gp;
    gp.volume_model = std::max(0, set.getIntValue("volume_model"));
    gp.deep_tremolo = set.getBoolValue("deep_tremolo");
    gp.deep_vibrato = set.getBoolValue("deep_vibrato");
    return gp;
}
