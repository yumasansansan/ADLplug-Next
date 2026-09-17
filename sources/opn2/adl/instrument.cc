//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
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
#include <opnmidi.h>
#include <cstring>

#define EACH_INS_FIELD(F)                               \
    F(note_offset)                                      \
    F(midi_velocity_offset) F(percussion_key_number)    \
    F(inst_flags)                                       \
    F(fbalg) F(lfosens)                                 \
    F(delay_on_ms) F(delay_off_ms)

#define EACH_OP_FIELD(F)                                                \
    F(dtfm_30) F(level_40) F(rsatk_50) F(amdecay1_60) F(decay2_70) F(susrel_80) F(ssgeg_90)

Instrument Instrument::from_adlmidi(const OPN2_Instrument &o) noexcept
{
    Instrument ins;
    static_cast<OPN2_Instrument &>(ins) = o;
    return ins;
}

Instrument Instrument::from_wopl(const WOPNInstrument &o) noexcept
{
    Instrument ins;
    ins.version = OPNMIDI_InstrumentVersion;

    #define F(x) ins.x = o.x;
    EACH_INS_FIELD(F)
    #undef F

    for (unsigned op = 0; op < 4; ++op) {
        #define F(x) ins.operators[op].x = o.operators[op].x;
        EACH_OP_FIELD(F)
        #undef F
    }

    static_assert(sizeof ins.name <= sizeof o.inst_name);
    std::memcpy(ins.name, o.inst_name, sizeof ins.name);

    return ins;
}

WOPNInstrument Instrument::to_wopl() const noexcept
{
    WOPNInstrument ins = {};

    #define F(x) ins.x = this->x;
    EACH_INS_FIELD(F)
    #undef F

    for (unsigned op = 0; op < 4; ++op) {
        #define F(x) ins.operators[op].x = this->operators[op].x;
        EACH_OP_FIELD(F)
        #undef F
    }

    static_assert(sizeof ins.inst_name <= sizeof name);
    std::memcpy(ins.inst_name, name, sizeof ins.inst_name);

    return ins;
}

void Instrument::describe(std::FILE *out) const noexcept
{
    std::fprintf(out,
                 "Instrument\n"
                 " - Blank %d\n"
                 " - Feedback %d Algorithm %d Tune %d\n"
                 " - AM sensitivity %d FM sensitivity %d\n"
                 " - Velocity offset %d\n"
                 " - Percussion note %d\n",
                 blank(),
                 feedback(), algorithm(), note_offset,
                 ams(), fms(),
                 midi_velocity_offset, percussion_key_number);
    for (unsigned op = 0; op < 4; ++op)
        describe_operator(op, out, "    ");
}

void Instrument::describe_operator(unsigned op, std::FILE *out, const char *indent) const noexcept
{
    std::fprintf(out,
                 "%sOperator %u\n"
                 "%s - ADSR %d %d,%d %d %d\n"
                 "%s - AM %d Level %d Rate scale %d Detune %d FMul %d\n"
                 "%s - SSG-EG Enable %d Wave %d\n",
                 indent, op,
                 indent, attack(op), decay2(op), decay1(op), sustain(op), release(op),
                 indent, am(op), level(op), ratescale(op), detune(op), fmul(op),
                 indent, ssgenable(op), ssgwave(op));
}

// Compares field by field. The structs have padding, so memcmp could tell
// apart two instruments that are the same.
bool Instrument::equal_instrument(const OPN2_Instrument &o) const noexcept
{
    if (version != o.version)
        return false;

    #define F(x) if (x != o.x) return false;
    EACH_INS_FIELD(F)
    #undef F

    for (unsigned op = 0; op < 4; ++op) {
        #define F(x) if (operators[op].x != o.operators[op].x) return false;
        EACH_OP_FIELD(F)
        #undef F
    }

    return true;
}

bool Instrument::equal_instrument_except_delays(const OPN2_Instrument &o) const noexcept
{
    OPN2_Instrument same_delays = o;
    same_delays.delay_on_ms = delay_on_ms;
    same_delays.delay_off_ms = delay_off_ms;
    return equal_instrument(same_delays);
}

void Midi_Bank::from_wopl(const WOPNFile &wopl, std::vector<Midi_Bank> &banks, Instrument_Global_Parameters &igp)
{
    const unsigned nm = wopl.banks_count_melodic;
    const unsigned np = wopl.banks_count_percussion;
    banks.clear();
    banks.resize(nm + np);

    for (unsigned b = 0; b < nm + np; ++b) {
        Midi_Bank &bank = banks[b];
        const bool percussive = b >= nm;
        const WOPNBank &src = percussive ? wopl.banks_percussive[b - nm] : wopl.banks_melodic[b];
        bank.id = Bank_Id(src.bank_midi_msb, src.bank_midi_lsb, percussive);
        for (std::size_t p = 0; p < bank.ins.size(); ++p)
            bank.ins[p] = Instrument::from_wopl(src.ins[p]);
        static_assert(sizeof bank.name <= sizeof src.bank_name);
        std::memcpy(bank.name, src.bank_name, sizeof bank.name);
    }

    igp.volume_model = wopl.volume_model;
    igp.lfo_enable = (wopl.lfo_freq & 8) != 0;
    igp.lfo_frequency = wopl.lfo_freq & 7;
}
