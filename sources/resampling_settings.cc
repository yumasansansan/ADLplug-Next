// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#include "resampling_settings.h"
#include <array>
#include <charconv>
#include <cstdint>
#include <string>
#include <system_error>

namespace {

// A number written so that reading it gives the same number back, bit for bit,
// whatever the locale: the state of a project has to come back as it was saved,
// and a decimal comma would not.
String exact_text(double value)
{
    std::array<char, 32> text {};
    const auto [end, error] = std::to_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc())
        return {};
    return String(text.data(), static_cast<std::size_t>(end - text.data()));
}

// Reads what exact_text() wrote, or anything else that is all a number; a value
// that is not one leaves `value` as it was, which is the default.
void read_number(const PropertySet &set, const char *key, double &value)
{
    if (!set.containsKey(key))
        return;
    const std::string text = set.getValue(key).toStdString();
    double read = 0.0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), read);
    if (error == std::errc() && end == text.data() + text.size())
        value = read;
}

void read_number(const PropertySet &set, const char *key, std::uint32_t &value)
{
    if (!set.containsKey(key))
        return;
    const std::string text = set.getValue(key).toStdString();
    std::uint32_t read = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), read);
    if (error == std::errc() && end == text.data() + text.size())
        value = read;
}

}  // namespace

PropertySet Resampling_Settings::to_properties() const
{
    // Every field, so that a project says what it was made with rather than what
    // the defaults happened to be at the time: a default that changes later is not
    // a reason for a saved project to sound different.
    PropertySet set;
    set.setValue("own_filter", own_filter);
    set.setValue("method", mp::resample::method_name(design.method));
    set.setValue("window", mp::resample::window_name(design.window));
    set.setValue("phase", mp::resample::phase_name(design.phase));
    set.setValue("attenuation_db", exact_text(design.attenuation_db));
    set.setValue("passband_ripple_db", exact_text(design.passband_ripple_db));
    set.setValue("bandwidth", exact_text(design.bandwidth));
    set.setValue("taps", String(design.taps));
    set.setValue("max_taps", String(design.max_taps));
    set.setValue("cepstrum", String(design.cepstrum));
    set.setValue("phase_floor_db", exact_text(design.phase_floor_db));
    set.setValue("remez_max_taps", String(design.remez_max_taps));
    set.setValue("refine_rounds", String(design.refine_rounds));
    set.setValue("refine_patience", String(design.refine_patience));
    set.setValue("measure_points", String(design.measure_points));
    set.setValue("stages", String(design.stages));
    set.setValue("verify", design.verify);
    return set;
}

Resampling_Settings Resampling_Settings::from_properties(const PropertySet &set)
{
    // A field that is missing -- a project from before there was a choice, or one
    // from before the field existed -- takes the default, and so does one that is
    // no value of its kind. What the design then makes of the values is the
    // design's to say, where the player is made.
    Resampling_Settings rs;
    rs.own_filter = set.getBoolValue("own_filter", rs.own_filter);

    mp::resample::Design &d = rs.design;
    if (set.containsKey("method"))
        static_cast<void>(mp::resample::method_from_name(set.getValue("method").toStdString(), d.method));
    if (set.containsKey("window"))
        static_cast<void>(mp::resample::window_from_name(set.getValue("window").toStdString(), d.window));
    if (set.containsKey("phase"))
        static_cast<void>(mp::resample::phase_from_name(set.getValue("phase").toStdString(), d.phase));
    read_number(set, "attenuation_db", d.attenuation_db);
    read_number(set, "passband_ripple_db", d.passband_ripple_db);
    read_number(set, "bandwidth", d.bandwidth);
    read_number(set, "taps", d.taps);
    read_number(set, "max_taps", d.max_taps);
    read_number(set, "cepstrum", d.cepstrum);
    read_number(set, "phase_floor_db", d.phase_floor_db);
    read_number(set, "remez_max_taps", d.remez_max_taps);
    read_number(set, "refine_rounds", d.refine_rounds);
    read_number(set, "refine_patience", d.refine_patience);
    read_number(set, "measure_points", d.measure_points);
    read_number(set, "stages", d.stages);
    d.verify = set.getBoolValue("verify", d.verify);
    return rs;
}

std::string resampling_preset_of(const mp::resample::Design &design)
{
    for (const char *name : resampling_preset_names) {
        mp::resample::Design preset;
        if (mp::resample::design_from_name(name, preset) && preset == design)
            return name;
    }
    return {};
}

bool resampling_preset(const std::string &name, mp::resample::Design &design)
{
    mp::resample::Design preset;
    if (!mp::resample::design_from_name(name, preset))
        return false;
    design = preset;
    return true;
}
