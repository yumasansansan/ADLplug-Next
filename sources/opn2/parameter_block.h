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

#pragma once
#include "JuceHeader.h"
#include "../parameter_block.h"
struct Instrument;
struct Chip_Settings;
struct Instrument_Global_Parameters;

struct Parameter_Block : Basic_Parameter_Block {
    Chip_Settings chip_settings() const;
    Instrument_Global_Parameters global_parameters() const;
    void set_chip_settings(const Chip_Settings &cs);
    void set_global_parameters(const Instrument_Global_Parameters &gp);

    AudioParameterFloat *p_mastervol = nullptr;

    AudioParameterChoice *p_emulator = nullptr;
    AudioParameterInt *p_nchip = nullptr;
    AudioParameterChoice *p_chiptype = nullptr;
    AudioParameterChoice *p_chan_alloc = nullptr;

    struct Operator {
        AudioParameterInt *p_detune = nullptr;
        AudioParameterInt *p_fmul = nullptr;
        AudioParameterInt *p_level = nullptr;
        AudioParameterInt *p_ratescale = nullptr;
        AudioParameterInt *p_attack = nullptr;
        AudioParameterBool *p_am = nullptr;
        AudioParameterInt *p_decay1 = nullptr;
        AudioParameterInt *p_decay2 = nullptr;
        AudioParameterInt *p_sustain = nullptr;
        AudioParameterInt *p_release = nullptr;
        AudioParameterBool *p_ssgenable = nullptr;
        AudioParameterChoice *p_ssgwave = nullptr;
    };

    struct Part {
        // The instrument the parameters describe, over base: what no parameter
        // holds, such as the pseudo eight-operator flag, keeps base's value.
        Instrument instrument(const Instrument &base) const;
        void set_instrument(const Instrument &ins);

        // AudioParameterBool *p_ps8op = nullptr;
        AudioParameterBool *p_blank = nullptr;
        AudioParameterInt *p_tune = nullptr;
        // AudioParameterInt *p_tune34 = nullptr;
        AudioParameterInt *p_feedback = nullptr;
        AudioParameterInt *p_algorithm = nullptr;
        AudioParameterInt *p_ams = nullptr;
        AudioParameterInt *p_fms = nullptr;
        AudioParameterInt *p_veloffset = nullptr;
        // AudioParameterInt *p_voice2ft = nullptr;
        AudioParameterInt *p_drumnote = nullptr;

        Operator op1, op3, op2, op4;

        // Operators in the order the instrument numbers them.
        Operator &nth_operator(unsigned i) noexcept
            { return this->*operator_member(i); }
        const Operator &nth_operator(unsigned i) const noexcept
            { return this->*operator_member(i); }

    private:
        static constexpr Operator Part::*operator_member(unsigned i) noexcept
        {
            static constexpr Operator Part::*members[4] {&Part::op1, &Part::op3, &Part::op2, &Part::op4};
            return members[i];
        }
    };

    Part part[16];

    AudioParameterChoice *p_volmodel = nullptr;
    AudioParameterBool *p_lfoenable = nullptr;
    AudioParameterChoice *p_lfofreq = nullptr;

    void setup_parameters(AudioProcessorEx &p);
};
