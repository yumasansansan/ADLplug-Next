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
#include <adlmidi.h>
#include <bit>
#include <cstring>

#define EACH_INS_FIELD(F)                                               \
    F(note_offset1) F(note_offset2)                                     \
    F(midi_velocity_offset) F(second_voice_detune) F(percussion_key_number) \
    F(inst_flags)                                                       \
    F(fb_conn1_C0) F(fb_conn2_C0)                                       \
    F(delay_on_ms) F(delay_off_ms)

#define EACH_OP_FIELD(F)                        \
    F(avekf_20) F(ksl_l_40) F(atdec_60) F(susrel_80) F(waveform_E0)

Instrument Instrument::from_adlmidi(const ADL_Instrument &o) noexcept
{
    Instrument ins;
    static_cast<ADL_Instrument &>(ins) = o;
    return ins;
}

Instrument Instrument::from_wopl(const WOPLInstrument &o) noexcept
{
    Instrument ins;
    ins.version = ADLMIDI_InstrumentVersion;

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

WOPLInstrument Instrument::to_wopl() const noexcept
{
    WOPLInstrument ins = {};

    #define F(x) ins.x = this->x;
    EACH_INS_FIELD(F)
    #undef F

    for (unsigned op = 0; op < 4; ++op) {
        #define F(x) ins.operators[op].x = this->operators[op].x;
        EACH_OP_FIELD(F)
        #undef F
    }

    // WOPL keeps 34 characters for the name of an instrument and this keeps
    // 32, so only 32 are there to copy; the two that are left stay the zeros
    // that ins was initialised with. Copying the length of the destination
    // read past the end of this instrument (plan D47).
    static_assert(sizeof name <= sizeof ins.inst_name,
        "the name of an instrument does not fit in WOPL's field");
    std::memcpy(ins.inst_name, name, sizeof name);

    return ins;
}

Instrument Instrument::from_sbi(const std::uint8_t *data, std::size_t length) noexcept
{
    Instrument ins;
    ins.version = ADLMIDI_InstrumentVersion;
    ins.blank(true);

    if (length < 4 + 32)
        return ins;

    const std::uint8_t *magic = data;
    data += 4;
    length -= 4;

    enum class Kind { Dos, Unix2op, Unix4op, Other };

    Kind kind;
    std::size_t minsize;
    if (std::memcmp(magic, "SBI\x1a", 4) == 0) {
        kind = Kind::Dos;
        minsize = 11;
    }
    else if (std::memcmp(magic, "2OP\x1a", 4) == 0) {
        kind = Kind::Unix2op;
        minsize = 11;
    }
    else if (std::memcmp(magic, "4OP\x1a", 4) == 0) {
        kind = Kind::Unix4op;
        minsize = 22;
    }
    else if (std::memcmp(magic, "SBI", 3) == 0) {
        kind = Kind::Other;
        minsize = 11;
    }
    else
        return ins;

    const bool unix_format = kind == Kind::Unix2op || kind == Kind::Unix4op;
    // The length check above leaves the 32 bytes of the name.
    const std::uint8_t *name_field = data;
    static_assert(sizeof ins.name == 32);
    std::memcpy(ins.name, name_field, unix_format ? 30 : 32);
    data += 32;
    length -= 32;

    if (length < minsize) {
        ins.blank(true);
        return ins;
    }

    // Each operator pair is stored carrier first.
    const auto load_operator_pair = [&ins](const std::uint8_t *pair, unsigned first) {
        for (unsigned i = 0; i < 2; ++i) {
            const std::size_t j = (i == 0) ? 1 : 0;
            ADL_Operator &op = ins.operators[first + i];
            op.avekf_20 = pair[0 + j];
            op.ksl_l_40 = pair[2 + j];
            op.atdec_60 = pair[4 + j];
            op.susrel_80 = pair[6 + j];
            op.waveform_E0 = pair[8 + j];
        }
    };

    load_operator_pair(data, 0);
    ins.fb_conn1_C0 = data[10];
    data += 11;
    length -= 11;

    switch (kind) {
    case Kind::Dos:
        if (length > 1)
            ins.note_offset1 = std::bit_cast<std::int8_t>(data[1]);
        if (length > 2)
            ins.percussion_key_number = data[2];
        break;

    case Kind::Unix4op:
        ins.four_op(true);
        load_operator_pair(data, 2);
        ins.fb_conn2_C0 = data[10];
        [[fallthrough]];
    case Kind::Unix2op:
        ins.percussion_key_number = name_field[31];
        break;

    case Kind::Other:
        break;
    }

    ins.blank(false);
    return ins;
}

void Instrument::describe(std::FILE *out) const noexcept
{
    std::fprintf(out,
                 "Instrument\n"
                 " - 4Op %d Ps4Op %d Blank %d\n"
                 " - 1-2 Feedback %d Conn %d Tune %d\n"
                 " - 3-4 Feedback %d Conn %d Tune %d\n"
                 " - Velocity offset %d\n"
                 " - Second voice fine tune %d\n"
                 " - Percussion note %d\n",
                 four_op(), pseudo_four_op(), blank(),
                 fb12(), con12(), note_offset1,
                 fb34(), con34(), note_offset2,
                 midi_velocity_offset, second_voice_detune, percussion_key_number);
    for (unsigned op = 0; op < 4; ++op)
        describe_operator(op, out, "    ");
}

void Instrument::describe_operator(unsigned op, std::FILE *out, const char *indent) const noexcept
{
    const char *text = "?";
    switch (op) {
    case WOPL_OP_MODULATOR1: text = "Modulator 1"; break;
    case WOPL_OP_CARRIER1: text = "Carrier 1"; break;
    case WOPL_OP_MODULATOR2: text = "Modulator 2"; break;
    case WOPL_OP_CARRIER2: text = "Carrier 2"; break;
    default: break;
    }
    std::fprintf(out,
                 "%sOperator %u: %s\n"
                 "%s - ADSR %d %d %d %d\n"
                 "%s - Level %d FMul %d KSL %d\n"
                 "%s - Trem %d Vib %d Sus %d Env %d\n"
                 "%s - Wave %d\n",
                 indent, op, text,
                 indent, attack(op), decay(op), sustain(op), release(op),
                 indent, level(op), fmul(op), ksl(op),
                 indent, trem(op), vib(op), sus(op), env(op),
                 indent, wave(op));
}

// Compares field by field. The structs have padding, so memcmp could tell
// apart two instruments that are the same.
bool Instrument::equal_instrument(const ADL_Instrument &o) const noexcept
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

bool Instrument::equal_instrument_except_delays(const ADL_Instrument &o) const noexcept
{
    ADL_Instrument same_delays = o;
    same_delays.delay_on_ms = delay_on_ms;
    same_delays.delay_off_ms = delay_off_ms;
    return equal_instrument(same_delays);
}

void Midi_Bank::from_wopl(const WOPLFile &wopl, std::vector<Midi_Bank> &banks, Instrument_Global_Parameters &igp)
{
    const unsigned nm = wopl.banks_count_melodic;
    const unsigned np = wopl.banks_count_percussion;
    banks.clear();
    banks.resize(nm + np);

    for (unsigned b = 0; b < nm + np; ++b) {
        Midi_Bank &bank = banks[b];
        const bool percussive = b >= nm;
        const WOPLBank &src = percussive ? wopl.banks_percussive[b - nm] : wopl.banks_melodic[b];
        bank.id = Bank_Id(src.bank_midi_msb, src.bank_midi_lsb, percussive);
        for (std::size_t p = 0; p < bank.ins.size(); ++p)
            bank.ins[p] = Instrument::from_wopl(src.ins[p]);
        static_assert(sizeof bank.name <= sizeof src.bank_name);
        std::memcpy(bank.name, src.bank_name, sizeof bank.name);
    }

    igp.volume_model = wopl.volume_model;
    igp.deep_tremolo = (wopl.opl_flags & WOPL_FLAG_DEEP_TREMOLO) != 0;
    igp.deep_vibrato = (wopl.opl_flags & WOPL_FLAG_DEEP_VIBRATO) != 0;
    igp.mt32_defaults = (wopl.opl_flags & WOPL_FLAG_MT32) != 0;
}
