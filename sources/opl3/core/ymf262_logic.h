//
// Copyright (C) 2023 nukeykt
//
// This file is part of YMF262-LLE.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//  YMF262 emulator
//  Thanks:
//      John McMaster (siliconpr0n.org):
//          YMF262 decap and die shot
//
//
// Nuked OPL3
// Copyright (C) 2013-2020 Nuke.YKT
//
// This file is part of Nuked OPL3.
//
// Nuked OPL3 is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation, either version 2.1
// of the License, or (at your option) any later version.
//
// Nuked OPL3 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Nuked OPL3. If not, see <https://www.gnu.org/licenses/>.
//
//  Nuked OPL3 emulator.
//  Thanks:
//      MAME Development Team(Jarek Burczynski, Tatsuyuki Satoh):
//          Feedback and Rhythm part calculation information.
//      forums.submarine.org.uk(carbon14, opl3):
//          Tremolo and phase generator calculation information.
//      OPLx decapsulated(Matthew Gambrell, Olli Niemitalo):
//          OPL2 ROMs.
//      siliconpr0n.org(John McMaster, digshadow):
//          YMF262 and VRC VII decaps and die shots.
//
// version: 1.8
//
// SPDX-FileCopyrightText: 2023 nukeykt
// SPDX-FileCopyrightText: 2013-2020 Nuke.YKT
// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later AND LGPL-2.1-or-later AND GPL-3.0-or-later
//
// This file is part of ADLplug-Next, and it follows the code of two emulators
// of the YMF262 (OPL3) by nukeykt, also known as Nuke.YKT. The notices at the
// top are theirs, as the files stand in libADLMIDI: YMF262-LLE, drawn from a
// die shot of the chip (nuked_fmopl3.c, under the GNU General Public License,
// version 2 or any later version, LICENSES/GPL-2.0-or-later.txt), which the
// envelope, the phase, the operator, the modulation, the rhythm phases, the
// noise and the small tables here follow; and Nuked OPL3 (nukedopl3.c, under
// the GNU Lesser General Public License, version 2.1 or any later version,
// LICENSES/LGPL-2.1-or-later.txt), which the counters of the tremolo, the
// vibrato and the envelope's timer follow. The SPDX lines name the copyright
// holders and licenses in the machine-readable form of the REUSE
// specification, with ADLplug-Next's code, which works the same out for many
// slots at once, under the GNU General Public License, version 3 or any later
// version (LICENSES/GPL-3.0-or-later.txt). The rest of the core (core.h) is
// ADLplug-Next's alone.
//
// What a slot of the YMF262 works out for a pass over the slots (one a sample),
// one slot at a time: written for loops over slots, which the compiler
// vectorizes, with no branch that depends on a slot. The functions a loop over
// slots calls are always inlined, since a call left in the loop keeps it from
// being vectorized. Each choice in them is between names or constants, never
// nested and never with work in its arms: the compiler makes such a choice a
// select at once, where one with work in an arm can stay a branch long enough
// for the loads that arm alone needs to move into it, and the vectorized loop
// then loads them masked, or a table lane by lane behind a branch each.

#pragma once

#include "ymf262_roms.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

namespace adlplug::opl3::ymf262 {

// The states of an envelope.
enum : std::int32_t { attack = 0, decay = 1, sustain = 2, release = 3 };

// The tables a loop over slots looks up are of 32 bits, which vector gathers
// take.

// Twice the multiple of a frequency, by the four bits of its register.
inline constexpr std::array<std::int32_t, 16> multiple = {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30};

// The key scale level: the attenuation by the top four bits of the frequency
// number, and the shift for each value of the two bits of its register.
inline constexpr std::array<std::int32_t, 16> ksl_rom = {0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64};
inline constexpr std::array<std::int32_t, 4> ksl_shift = {31, 1, 2, 0};

// Whether a rate's high step falls in a pass, by the low two bits of the key
// scaling and the low two bits of the envelope's timer. It falls for the key
// scaling's two bits from high_step_from on.
inline constexpr std::uint8_t step_high[4][4] = {{0, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 1, 0}, {1, 1, 1, 0}};
inline constexpr std::int32_t high_step_from[4] = {1, 3, 2, 4};

constexpr bool high_steps_agree()
{
    for (int k = 0; k < 4; ++k)
        for (int t = 0; t < 4; ++t)
            if ((step_high[k][t] != 0) != (k >= high_step_from[t]))
                return false;
    return true;
}
static_assert(high_steps_agree());

// The key scaling of the rates, from the block and the note select bit of the
// frequency number, whole when the register asks for it and else a quarter.
[[gnu::always_inline]] constexpr unsigned key_scale(unsigned fnum, unsigned block, bool note_select_low, bool scaled)
{
    const unsigned bit8 = (fnum >> 8) & 1, bit9 = (fnum >> 9) & 1;
    const unsigned ns = note_select_low ? bit8 : bit9;
    const unsigned whole = (block << 1) | ns, quarter = block >> 1;
    return scaled ? whole : quarter;
}

// A rate of an envelope with its key scaling, 15 at the most.
[[gnu::always_inline]] constexpr unsigned scaled_rate(unsigned rate, unsigned ksr)
{
    const unsigned r = rate + (ksr >> 2);
    return r > 15 ? 15 : r;
}

// The attenuation of the key scale level and the total level, in steps of
// the envelope.
[[gnu::always_inline]] constexpr unsigned level_offset(unsigned fnum, unsigned block, unsigned ksl, unsigned total_level)
{
    const auto rom = static_cast<unsigned>(ksl_rom[fnum >> 6]);
    const unsigned sum = (rom & 63) + (block << 3), kept = sum & 63;
    const unsigned k = (rom & 64) != 0 || (sum & 64) != 0 ? kept : 0;
    return ((k << 2) >> static_cast<unsigned>(ksl_shift[ksl & 3])) + (total_level << 2);
}

// A waveform, for the operator: how far the phase is shifted up (1 for the
// doubled ones), the bit of the shifted phase that turns it back, the bit of
// the phase that silences the output (0 for none), the bit of the shifted
// phase that makes the output negative (0 for none), and whether it is the
// square wave and whether the sawtooth.
struct Wave {
    std::int32_t phase_shift, turn_bit, mute_bit, sign_bit, square, sawtooth;
};

[[gnu::always_inline]] constexpr Wave wave(unsigned waveform)
{
    const unsigned w = waveform & 7;
    Wave v{};
    v.phase_shift = (w == 4 || w == 5) ? 1 : 0;
    v.turn_bit = w == 7 ? 512 : 256;
    const std::int32_t mute_low = w == 3 ? 256 : 0;
    v.mute_bit = (w == 1 || w == 4 || w == 5) ? 512 : mute_low;
    v.sign_bit = (w == 2 || w == 3 || w == 5) ? 0 : 512;
    v.square = w == 6 ? 1 : 0;
    v.sawtooth = w == 7 ? 1 : 0;
    return v;
}

// What the envelope goes by in a pass: whether the timer's second half of its
// count is on, the step the timer's lowest set bit gives, and the key scaling's
// two bits from which the high step falls.
struct Envelope_timing {
    std::int32_t second, add, high_from;
};

struct Envelope_step {
    std::int32_t state, level;
    bool restart;  // keyed on from release: the phase starts again
};

// A slot's envelope over a pass: its state and level for the next pass. The
// rates are a byte for each state, the rate with its key scaling and 16 when
// the register's rate is not 0; ksr is the key scaling, ksr_low its low two
// bits, and sustain_level the level of the sustain in steps of 16 (31 for the
// register's 15).
[[gnu::always_inline]] inline Envelope_step envelope(std::int32_t state, std::int32_t level, std::int32_t key, std::uint32_t rates,
                              std::int32_t ksr, std::int32_t ksr_low, std::int32_t sustain_level,
                              const Envelope_timing &t)
{
    const bool dokon = state == release && key != 0;
    const std::int32_t selected = dokon ? std::int32_t{attack} : state;
    const std::uint32_t code = (rates >> (8 * selected)) & 0xff;
    const std::int32_t rate_hi = static_cast<std::int32_t>(code & 15);
    const bool rate_nonzero = (code >> 4) != 0;
    const bool second = t.second != 0;
    const std::int32_t sum = (rate_hi + t.add) & 15;
    const bool inclow = rate_hi < 12 && rate_nonzero && second
        && (sum == 12 || (sum == 13 && (ksr & 2) != 0) || (sum == 14 && (ksr & 1) != 0));
    const bool stephi = ksr_low >= t.high_from;
    // The step of the rate in this pass, as the shift of the increment: 1 for a
    // quarter, 2 for a half, 3 for the whole, 0 for none. From rate 12 up it
    // is a step a rate, and one more when the high step falls (at 12, when the
    // timer's second half is on, too), 3 at the most; below 12 it is a quarter
    // when the low increment falls.
    const std::int32_t high = stephi || (rate_hi == 12 && second) ? 1 : 0;
    const std::int32_t step_from_12 = std::min(rate_hi - 12 + high, 3), step_below_12 = inclow ? 1 : 0;
    const std::int32_t step = rate_hi >= 12 ? step_from_12 : step_below_12;
    const bool maxrate = rate_hi == 15;
    const bool slreach = (level >> 4) == sustain_level;
    const bool zeroreach = level == 0;
    const bool silent = (level & 0x1f8) == 0x1f8;
    std::int32_t next = state;
    next = (state == decay && slreach) ? std::int32_t{sustain} : next;
    next = (state == attack && zeroreach) ? std::int32_t{decay} : next;
    next = key != 0 ? next : std::int32_t{release};
    next = dokon ? std::int32_t{attack} : next;
    const bool linear = !dokon && !silent && (state >= sustain || (state == decay && !slreach));
    const bool exponent = state == attack && key != 0 && !maxrate && !zeroreach;
    const bool instant = dokon && maxrate;
    const bool mute = state != attack && silent && !dokon;
    const std::int32_t level_at_once = instant ? 0 : level;
    const std::int32_t level2 = mute ? 0x1ff : level_at_once;
    // The increment: 4 shifted down for a linear one, the complement of the
    // level's top eight bits for the attack's, with the ninth bit and, at a
    // quarter and a half, the bits below it set.
    const std::int32_t attack_add = (level >> 1) ^ 0xff, linear_add = linear ? 4 : 0;
    const std::int32_t add = exponent ? attack_add : linear_add;
    const std::int32_t attack_bits = 0x100 | ((0x60 << step) & 0xc0);
    const std::int32_t high_bits = exponent ? attack_bits : 0;
    const std::int32_t stepped = (add >> (3 - step)) | high_bits;
    const std::int32_t addshift = step == 0 ? 0 : stepped;
    return {.state = next, .level = (level2 + addshift) & 0x1ff, .restart = dokon};
}

// The attenuation the operator takes: the level before this pass's step, with
// the key scale level, the total level and the tremolo, all ones if they come
// to more than nine bits hold.
[[gnu::always_inline]] inline std::int32_t attenuation(std::int32_t level, std::int32_t offset)
{
    const std::int32_t total = level + (offset & 0x1ff);
    return ((offset | total) & 0x200) != 0 ? 0x1ff : total;
}

// The vibrato of a pass: which bits of the frequency number go on it at the
// deep and at the shallow depth, and whether they are taken off.
struct Vibrato {
    std::uint32_t deep_shift, deep_mask, shallow_shift, shallow_mask;
    bool down;
};

constexpr Vibrato vibrato(unsigned position)
{
    const bool sel1 = (position & 3) == 2, sel2 = (position & 1) == 1;
    return {.deep_shift = sel1 ? 7u : 8u,
            .deep_mask = sel1 ? 7u : sel2 ? 3u : 0u,
            .shallow_shift = sel1 ? 8u : 9u,
            .shallow_mask = sel1 ? 3u : sel2 ? 1u : 0u,
            .down = (position & 4) != 0};
}

// A slot's phase increment in a pass: the frequency number, with the vibrato
// when the slot has it, in the block, times the
// multiple.
[[gnu::always_inline]] inline std::uint32_t phase_increment(std::uint32_t fnum, std::uint32_t block, std::uint32_t multiple_value,
                                     bool vibrato, bool deep, const Vibrato &v)
{
    const std::uint32_t shift = deep ? v.deep_shift : v.shallow_shift, bits = deep ? v.deep_mask : v.shallow_mask;
    const std::uint32_t swing = (fnum >> shift) & bits;
    const std::uint32_t add = vibrato ? swing : 0;
    const std::uint32_t lower = (fnum - add) & 1023, higher = fnum + add;
    const std::uint32_t fn = v.down ? lower : higher;
    return (((fn << block) >> 1) * multiple_value) >> 1;
}

// The phase for the next pass, from nought when the slot starts again.
[[gnu::always_inline]] inline std::uint32_t next_phase(std::uint32_t phase, std::uint32_t increment, bool restart)
{
    const std::uint32_t from = restart ? 0 : phase;
    return (from + increment) & 0x7ffff;
}

// What the operator reads of the phase: its top ten bits.
[[gnu::always_inline]] inline std::int32_t phase_out(std::uint32_t phase)
{
    return static_cast<std::int32_t>((phase >> 9) & 1023);
}

// The modulation of an operator, by what it is modulated by (0 nothing, 1 the
// stage before, 2 feedback): the low ten bits of the slot that feeds it, or
// its feedback, the sum of its last two outputs shifted down.
[[gnu::always_inline]] inline std::int32_t modulation(std::int32_t source, std::int32_t history, std::int32_t feedback_shift,
                                                     std::int32_t by)
{
    const std::int32_t stage = source & 1023, feedback = (history >> feedback_shift) & 1023;
    const std::int32_t other = by == 2 ? feedback : 0;
    return by == 1 ? stage : other;
}

// The operator's output: a 13-bit value, its negative in ones' complement,
// held signed.
[[gnu::always_inline]] inline std::int32_t operator_output(std::int32_t phase, std::int32_t phase_shift, std::int32_t turn_bit,
                                    std::int32_t mute_bit, std::int32_t sign_bit, std::int32_t square,
                                    std::int32_t sawtooth, std::int32_t eg_out)
{
    const std::int32_t shifted = (phase << phase_shift) & 1023, turned = shifted ^ 511;
    const std::int32_t phase2 = (shifted & turn_bit) != 0 ? turned : shifted;
    const bool mute = (phase & mute_bit) != 0;
    const bool negative = (phase2 & sign_bit) != 0;
    const std::int32_t low = phase2 & 255;
    const std::int32_t index = square != 0 ? 255 : low;
    const std::int32_t ramp = (phase2 & 511) << 3, sine = logsin_rom[static_cast<std::size_t>(index)];
    const std::int32_t shape = sawtooth != 0 ? ramp : sine;
    const std::int32_t sum = shape + (eg_out << 3);
    const std::int32_t att = (sum & 4096) != 0 ? 4095 : sum;
    const std::int32_t magnitude = ((exp_rom[static_cast<std::size_t>(att & 255)] | 0x400) << 1) >> (att >> 8);
    const std::int32_t inverted = magnitude ^ 8191;
    const std::int32_t signed_value = negative ? inverted : magnitude;
    const std::int32_t value = mute ? 0 : signed_value;
    const std::int32_t wrapped = value - 8192;
    return (value & 0x1000) != 0 ? wrapped : value;
}

// The noise: a 23-bit shift register fed with the sum of its bits 22 and 8,
// and with a one when it is empty.
constexpr std::uint32_t noise_step(std::uint32_t s)
{
    std::uint32_t bit = ((s >> 22) ^ (s >> 8)) & 1;
    if ((s & 0x7fffff) == 0)
        bit = 1;
    return ((s << 1) | bit) & 0x7fffff;
}

// The 18 steps of a pass at once, for a register that is not empty: the bits
// fed in go in below the old ones, the first nine of them the sums of bits the
// register holds now, the last nine taking the first nine for bit 8.
constexpr std::uint32_t noise_pass(std::uint32_t s)
{
    const std::uint32_t high = ((s >> 5) ^ (s << 9)) & 0x3fe00;
    const std::uint32_t low = ((s >> 5) ^ (high >> 9)) & 0x1ff;
    return ((s << 18) | high | low) & 0x7fffff;
}

constexpr bool noise_pass_steps(std::uint32_t s)
{
    std::uint32_t t = s;
    for (int i = 0; i < 18; ++i)
        t = noise_step(t);
    return noise_pass(s) == t;
}
static_assert(noise_pass_steps(0x287844) && noise_pass_steps(0x000001) && noise_pass_steps(0x7fffff));

// The hi-hat reads the noise's top bit 6 steps after the pass's first step,
// which is bit 16 as that step leaves it, and the snare 8, bit 14.
constexpr unsigned hi_hat_noise(std::uint32_t s) { return (s >> 16) & 1; }
constexpr unsigned snare_noise(std::uint32_t s) { return (s >> 14) & 1; }

// The bits of the hi-hat's and the top cymbal's phases that the rhythm
// phases take.
struct Rhythm_bits {
    unsigned hh2 = 0, hh3 = 0, hh7 = 0, hh8 = 0, tc3 = 0, tc5 = 0;

    void take_hi_hat(std::int32_t phase)
    {
        const unsigned p = static_cast<unsigned>(phase);
        hh2 = (p >> 2) & 1;
        hh3 = (p >> 3) & 1;
        hh7 = (p >> 7) & 1;
        hh8 = (p >> 8) & 1;
    }
    void take_top_cymbal(std::int32_t phase)
    {
        const unsigned p = static_cast<unsigned>(phase);
        tc3 = (p >> 3) & 1;
        tc5 = (p >> 5) & 1;
    }
    [[nodiscard]] unsigned ring() const { return (hh2 ^ hh7) | (tc5 ^ hh3) | (tc5 ^ tc3); }
    [[nodiscard]] std::int32_t hi_hat(unsigned noise) const
    {
        const unsigned rm = ring();
        return static_cast<std::int32_t>((rm << 9) | ((noise ^ rm) != 0 ? 0xd0u : 0x34u));
    }
    [[nodiscard]] std::int32_t snare(unsigned noise) const { return static_cast<std::int32_t>((hh8 << 9) | ((noise ^ hh8) << 8)); }
    [[nodiscard]] std::int32_t top_cymbal() const { return static_cast<std::int32_t>((ring() << 9) | 0x80); }
};

// The counters of the chip: the one of the tremolo and the vibrato, and the
// envelope's timer. The timer here is one ahead of the chip's, as it is
// counted up after it is read.
struct Counters {
    std::uint32_t lfo = 0;
    std::uint16_t tremolo_position = 0;  // 0 to 209, up and back down in the level
    std::uint8_t tremolo_step = 0, vibrato_position = 0;
    std::uint64_t eg_timer = 1;
    std::uint8_t eg_second = 0, eg_add = 0, eg_timer_low = 0, eg_carry = 0;

    [[nodiscard]] std::int32_t tremolo_level() const { return tremolo_position < 105 ? tremolo_position : 210 - tremolo_position; }

    [[nodiscard]] Envelope_timing envelope_timing() const
    {
        return {.second = eg_second, .add = eg_add, .high_from = high_step_from[eg_timer_low]};
    }

    // At the end of a pass. The tremolo takes a step two passes after the one
    // whose count calls for it, the vibrato one pass after.
    constexpr void advance()
    {
        if (tremolo_step != 0)
            tremolo_position = static_cast<std::uint16_t>((tremolo_position + 1) % 210);
        tremolo_step = (lfo & 0x3f) == 0x3f ? 1 : 0;
        if ((lfo & 0x3ff) == 0x3ff)
            vibrato_position = static_cast<std::uint8_t>((vibrato_position + 1) & 7);
        lfo = (lfo + 1) & 0x3ff;
        if (eg_second != 0) {
            // The timer's lowest set bit among its low 13 (13 for none).
            const int shift = std::countr_zero(static_cast<std::uint32_t>(eg_timer & 0x1fff) | 0x2000u);
            eg_add = static_cast<std::uint8_t>(shift > 12 ? 0 : shift + 1);
            eg_timer_low = static_cast<std::uint8_t>(eg_timer & 3);
        }
        if (eg_carry != 0 || eg_second != 0) {
            if (eg_timer == 0xfffffffffULL) {
                eg_timer = 0;
                eg_carry = 1;
            }
            else {
                ++eg_timer;
                eg_carry = 0;
            }
        }
        eg_second ^= 1;
    }
};

}  // namespace adlplug::opl3::ymf262
