//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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
        Instrument instrument() const;
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
