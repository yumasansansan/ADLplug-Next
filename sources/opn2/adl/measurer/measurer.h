/*
 * OPN2 Bank Editor by Wohlstand, a free tool for music bank editing
 * Copyright (c) 2017-2018 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Modified for ADLplug-Next. The modifications are distributed under the
 * GNU GPL v3 or later, as above.
 */

#pragma once
#include <cstdint>
struct Instrument;

namespace Measurer
{
    struct DurationInfo
    {
        std::uint64_t peak_amplitude_time = 0;
        double peak_amplitude_value = 0;
        double quarter_amplitude_time = 0;
        double begin_amplitude = 0;
        double interval = 0;
        double keyoff_out_time = 0;
        std::uint64_t ms_sound_kon = 0;
        std::uint64_t ms_sound_koff = 0;
        bool nosound = false;
    };

    void ComputeDurations(const Instrument &in, DurationInfo &result);
}
