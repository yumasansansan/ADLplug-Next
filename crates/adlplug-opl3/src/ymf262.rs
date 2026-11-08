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
// version (LICENSES/GPL-3.0-or-later.txt). It is the C++ version's
// ymf262_logic.h in Rust, and the rest of the core (core.rs, pass.rs, roms.rs)
// is ADLplug-Next's alone.

//! What a slot of the YMF262 (OPL3) works out for a pass over the slots (one a
//! sample), one slot at a time: written for loops over slots, for the compiler
//! to vectorize, with no branch that depends on a slot. The functions a loop
//! over slots calls are small and inlined into it, since a call left in the
//! loop keeps it from being vectorized. Each choice in them is between
//! names or constants, never nested and never with work in its arms: the
//! compiler makes such a choice a select at once, where one with work in an arm
//! can stay a branch long enough for the loads that arm alone needs to move into
//! it, and the vectorized loop then loads them masked, or a table lane by lane
//! behind a branch each.
//!
//! Where the chip's arithmetic is in unsigned numbers that could wrap, it wraps
//! here too, by name (`wrapping_sub`); everywhere else the numbers stay within
//! their types, as the tests find in the dev profile, where an overflow panics.
#![forbid(unsafe_code)]

use crate::roms::{EXP_ROM, LOGSIN_ROM};

/// The state of an envelope: attack.
pub(crate) const ATTACK: i32 = 0;
/// The state of an envelope: decay.
pub(crate) const DECAY: i32 = 1;
/// The state of an envelope: sustain.
pub(crate) const SUSTAIN: i32 = 2;
/// The state of an envelope: release.
pub(crate) const RELEASE: i32 = 3;

// The tables a loop over slots looks up are of 32 bits, which vector gathers
// take.

/// Twice the multiple of a frequency, by the four bits of its register.
pub(crate) static MULTIPLE: [u32; 16] = [1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30];

/// The key scale level: the attenuation by the top four bits of the frequency
/// number.
static KSL_ROM: [u32; 16] = [0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64];
/// The shift of the key scale level for each value of the two bits of its
/// register.
static KSL_SHIFT: [u32; 4] = [31, 1, 2, 0];

/// Whether a rate's high step falls in a pass, by the low two bits of the key
/// scaling and the low two bits of the envelope's timer. It falls for the key
/// scaling's two bits from [`HIGH_STEP_FROM`] on.
const STEP_HIGH: [[u8; 4]; 4] = [[0, 0, 0, 0], [1, 0, 0, 0], [1, 0, 1, 0], [1, 1, 1, 0]];
/// The key scaling's two bits from which the high step falls, by the low two
/// bits of the envelope's timer.
const HIGH_STEP_FROM: [i32; 4] = [1, 3, 2, 4];

/// Whether [`STEP_HIGH`] and [`HIGH_STEP_FROM`] say the same.
const fn high_steps_agree() -> bool {
    // The key scaling's bits as an index of the table and as the number they
    // are compared as, counted together.
    let mut k = 0;
    let mut key_scale = 0;
    while k < 4 {
        let mut t = 0;
        while t < 4 {
            if (STEP_HIGH[k][t] != 0) != (key_scale >= HIGH_STEP_FROM[t]) {
                return false;
            }
            t += 1;
        }
        k += 1;
        key_scale += 1;
    }
    true
}
const _: () = assert!(high_steps_agree());

/// The key scaling of the rates, from the block and the note select bit of the
/// frequency number, whole when the register asks for it and else a quarter.
#[inline(always)]
#[expect(
    clippy::inline_always,
    reason = "it belongs in the loop of derive(), in each tier's form of a sample"
)]
pub(crate) fn key_scale(fnum: u32, block: u32, note_select_low: bool, scaled: bool) -> u32 {
    let bit8 = (fnum >> 8) & 1;
    let bit9 = (fnum >> 9) & 1;
    let ns = if note_select_low { bit8 } else { bit9 };
    let whole = (block << 1) | ns;
    let quarter = block >> 1;
    if scaled { whole } else { quarter }
}

/// A rate of an envelope with its key scaling, 15 at the most.
#[inline(always)]
#[expect(
    clippy::inline_always,
    reason = "it belongs in the loop of derive(), in each tier's form of a sample"
)]
pub(crate) fn scaled_rate(rate: u32, ksr: u32) -> u32 {
    (rate + (ksr >> 2)).min(15)
}

/// The attenuation of the key scale level and the total level, in steps of
/// the envelope. The frequency number has ten bits, the top four of which pick
/// the key scale level's attenuation.
#[inline(always)]
#[expect(
    clippy::inline_always,
    reason = "it belongs in the loop of derive(), in each tier's form of a sample"
)]
pub(crate) fn level_offset(fnum: u32, block: u32, ksl: u32, total_level: u32) -> u32 {
    let rom = KSL_ROM[index((fnum >> 6) & 15)];
    let sum = (rom & 63) + (block << 3);
    let kept = sum & 63;
    let k = if (rom & 64) != 0 || (sum & 64) != 0 { kept } else { 0 };
    ((k << 2) >> KSL_SHIFT[index(ksl & 3)]) + (total_level << 2)
}

/// For rust-speed's experiments: the key scale level's ROM by the frequency
/// number, the one lookup of [`level_offset`] that no expression takes the
/// place of.
#[cfg(feature = "experiment")]
#[inline(always)]
#[expect(
    clippy::inline_always,
    reason = "it belongs in the loops of derive(), in each tier's form of a sample"
)]
pub(crate) fn ksl_rom_of(fnum: u32) -> u32 {
    KSL_ROM[index((fnum >> 6) & 15)]
}

/// For rust-speed's experiments: [`level_offset`] from the ROM's value, which
/// [`ksl_rom_of`] looks up, and with the shift of [`ksl_shift_of`].
#[cfg(feature = "experiment")]
#[inline(always)]
#[expect(
    clippy::inline_always,
    reason = "it belongs in the loops of derive(), in each tier's form of a sample"
)]
pub(crate) fn level_offset_from(rom: u32, block: u32, ksl: u32, total_level: u32) -> u32 {
    let sum = (rom & 63) + (block << 3);
    let kept = sum & 63;
    let k = if (rom & 64) != 0 || (sum & 64) != 0 { kept } else { 0 };
    ((k << 2) >> ksl_shift_of(ksl & 3)) + (total_level << 2)
}

/// For rust-speed's experiments: [`KSL_SHIFT`] by an expression, for a vector
/// loop without lookups.
#[cfg(feature = "experiment")]
pub(crate) const fn ksl_shift_of(k: u32) -> u32 {
    let shift = if k == 0 { 31 } else { k };
    if k == 3 { 0 } else { shift }
}

/// For rust-speed's experiments: [`MULTIPLE`] by an expression, twice the
/// register's four bits, but a half for 0 and the three numbers the chip takes
/// in place of 11, 13 and 15 (and 14).
#[cfg(feature = "experiment")]
pub(crate) const fn multiple_of(m: u32) -> u32 {
    let twice = if m == 0 { 1 } else { 2 * m };
    let twice = if m == 11 { 20 } else { twice };
    let twice = if m == 13 { 24 } else { twice };
    if m >= 14 { 30 } else { twice }
}

/// Whether the expressions say what the tables do, for every index.
#[cfg(feature = "experiment")]
const fn expressions_agree() -> bool {
    // Each index as an index of the table and as the number the expression
    // takes, counted together.
    let mut i = 0;
    let mut m = 0;
    while i < MULTIPLE.len() {
        if multiple_of(m) != MULTIPLE[i] {
            return false;
        }
        i += 1;
        m += 1;
    }
    let mut i = 0;
    let mut k = 0;
    while i < KSL_SHIFT.len() {
        if ksl_shift_of(k) != KSL_SHIFT[i] {
            return false;
        }
        i += 1;
        k += 1;
    }
    true
}
#[cfg(feature = "experiment")]
const _: () = assert!(expressions_agree());

/// A number of 32 bits as an index of a table, which every architecture the
/// core is built for holds whole.
#[inline(always)]
#[expect(
    clippy::inline_always,
    reason = "it belongs in the loop of derive(), in each tier's form of a sample"
)]
fn index(value: u32) -> usize {
    usize::try_from(value).expect("an index of 32 bits")
}

/// A waveform, for the operator: how far the phase is shifted up (1 for the
/// doubled ones), the bit of the shifted phase that turns it back, the bit of
/// the phase that silences the output (0 for none), the bit of the shifted
/// phase that makes the output negative (0 for none), and whether it is the
/// square wave and whether the sawtooth.
#[derive(Clone, Copy)]
pub(crate) struct Wave {
    /// How far the phase is shifted up.
    pub(crate) phase_shift: i32,
    /// The bit of the shifted phase that turns it back.
    pub(crate) turn_bit: i32,
    /// The bit of the phase that silences the output, or 0.
    pub(crate) mute_bit: i32,
    /// The bit of the shifted phase that makes the output negative, or 0.
    pub(crate) sign_bit: i32,
    /// 1 for the square wave.
    pub(crate) square: i32,
    /// 1 for the sawtooth.
    pub(crate) sawtooth: i32,
}

/// The waveform of the three bits of a slot's register.
#[inline(always)]
#[expect(
    clippy::inline_always,
    reason = "it belongs in the loop of derive(), in each tier's form of a sample"
)]
pub(crate) fn wave(waveform: u32) -> Wave {
    let w = waveform & 7;
    let mute_low = if w == 3 { 256 } else { 0 };
    Wave {
        phase_shift: i32::from(w == 4 || w == 5),
        turn_bit: if w == 7 { 512 } else { 256 },
        mute_bit: if w == 1 || w == 4 || w == 5 { 512 } else { mute_low },
        sign_bit: if w == 2 || w == 3 || w == 5 { 0 } else { 512 },
        square: i32::from(w == 6),
        sawtooth: i32::from(w == 7),
    }
}

/// What the envelope goes by in a pass: whether the timer's second half of its
/// count is on, the step the timer's lowest set bit gives, and the key scaling's
/// two bits from which the high step falls.
#[derive(Clone, Copy)]
pub(crate) struct EnvelopeTiming {
    /// The timer's second half is on (1) or off (0).
    pub(crate) second: i32,
    /// The step the timer's lowest set bit gives.
    pub(crate) add: i32,
    /// The key scaling's two bits from which the high step falls.
    pub(crate) high_from: i32,
}

/// What a slot's envelope goes by besides its state and level, as the slot read
/// its registers: whether it is keyed on, the rates, a byte for each state, the
/// rate with its key scaling and 16 when the register's rate is not 0, the key
/// scaling and its low two bits, and the level of the sustain in steps of 16
/// (31 for the register's 15).
#[derive(Clone, Copy)]
pub(crate) struct Envelope {
    /// Keyed on (not 0) or off.
    pub(crate) key: i32,
    /// The rates, a byte for each state.
    pub(crate) rates: u32,
    /// The key scaling.
    pub(crate) ksr: i32,
    /// Its low two bits.
    pub(crate) ksr_low: i32,
    /// The level of the sustain, in steps of 16.
    pub(crate) sustain_level: i32,
}

/// A slot's envelope after a pass.
#[derive(Clone, Copy)]
pub(crate) struct EnvelopeStep {
    /// The state for the next pass.
    pub(crate) state: i32,
    /// The level for the next pass.
    pub(crate) level: i32,
    /// Keyed on from release: the phase starts again.
    pub(crate) restart: bool,
}

/// A slot's envelope over a pass: its state and level for the next pass.
#[inline]
pub(crate) fn envelope(state: i32, level: i32, slot: &Envelope, t: &EnvelopeTiming) -> EnvelopeStep {
    let key = slot.key;
    let ksr = slot.ksr;
    let dokon = state == RELEASE && key != 0;
    let selected = if dokon { ATTACK } else { state };
    let code = (slot.rates >> (8 * selected)) & 0xff;
    let rate_hi = (code & 15).cast_signed();
    let rate_nonzero = (code >> 4) != 0;
    let second = t.second != 0;
    let sum = (rate_hi + t.add) & 15;
    let inclow = rate_hi < 12
        && rate_nonzero
        && second
        && (sum == 12 || (sum == 13 && (ksr & 2) != 0) || (sum == 14 && (ksr & 1) != 0));
    let stephi = slot.ksr_low >= t.high_from;
    // The step of the rate in this pass, as the shift of the increment: 1 for a
    // quarter, 2 for a half, 3 for the whole, 0 for none. From rate 12 up it
    // is a step a rate, and one more when the high step falls (at 12, when the
    // timer's second half is on, too), 3 at the most; below 12 it is a quarter
    // when the low increment falls.
    let high = i32::from(stephi || (rate_hi == 12 && second));
    let step_from_12 = (rate_hi - 12 + high).min(3);
    let step_below_12 = i32::from(inclow);
    let step = if rate_hi >= 12 { step_from_12 } else { step_below_12 };
    let maxrate = rate_hi == 15;
    let slreach = (level >> 4) == slot.sustain_level;
    let zeroreach = level == 0;
    let silent = (level & 0x1f8) == 0x1f8;
    let mut next = state;
    next = if state == DECAY && slreach { SUSTAIN } else { next };
    next = if state == ATTACK && zeroreach { DECAY } else { next };
    next = if key != 0 { next } else { RELEASE };
    next = if dokon { ATTACK } else { next };
    let linear = !dokon && !silent && (state >= SUSTAIN || (state == DECAY && !slreach));
    let exponent = state == ATTACK && key != 0 && !maxrate && !zeroreach;
    let instant = dokon && maxrate;
    let mute = state != ATTACK && silent && !dokon;
    let level_at_once = if instant { 0 } else { level };
    let level2 = if mute { 0x1ff } else { level_at_once };
    // The increment: 4 shifted down for a linear one, the complement of the
    // level's top eight bits for the attack's, with the ninth bit and, at a
    // quarter and a half, the bits below it set.
    let attack_add = (level >> 1) ^ 0xff;
    let linear_add = if linear { 4 } else { 0 };
    let add = if exponent { attack_add } else { linear_add };
    let attack_bits = 0x100 | ((0x60 << step) & 0xc0);
    let high_bits = if exponent { attack_bits } else { 0 };
    let stepped = (add >> (3 - step)) | high_bits;
    let addshift = if step == 0 { 0 } else { stepped };
    EnvelopeStep {
        state: next,
        level: (level2 + addshift) & 0x1ff,
        restart: dokon,
    }
}

/// The attenuation the operator takes: the level before this pass's step, with
/// the key scale level, the total level and the tremolo, all ones if they come
/// to more than nine bits hold.
#[inline]
pub(crate) fn attenuation(level: i32, offset: i32) -> i32 {
    let total = level + (offset & 0x1ff);
    if ((offset | total) & 0x200) != 0 { 0x1ff } else { total }
}

/// The vibrato of a pass: which bits of the frequency number go on it at the
/// deep and at the shallow depth, and whether they are taken off.
#[derive(Clone, Copy)]
pub(crate) struct Vibrato {
    /// The shift of the frequency number at the deep depth.
    pub(crate) deep_shift: u32,
    /// The bits of it that go on at the deep depth.
    pub(crate) deep_mask: u32,
    /// The shift at the shallow depth.
    pub(crate) shallow_shift: u32,
    /// The bits at the shallow depth.
    pub(crate) shallow_mask: u32,
    /// They are taken off the frequency number.
    pub(crate) down: bool,
}

/// The vibrato at a position of its count.
pub(crate) fn vibrato(position: u8) -> Vibrato {
    let sel1 = (position & 3) == 2;
    let sel2 = (position & 1) == 1;
    let (deep_mask, shallow_mask) = if sel1 {
        (7, 3)
    } else if sel2 {
        (3, 1)
    } else {
        (0, 0)
    };
    Vibrato {
        deep_shift: if sel1 { 7 } else { 8 },
        deep_mask,
        shallow_shift: if sel1 { 8 } else { 9 },
        shallow_mask,
        down: (position & 4) != 0,
    }
}

/// A slot's phase increment in a pass: the frequency number, with the vibrato
/// when the slot has it, in the block, times the multiple.
#[inline]
pub(crate) fn phase_increment(fnum: u32, block: u32, multiple: u32, vibrato: bool, deep: bool, v: &Vibrato) -> u32 {
    let shift = if deep { v.deep_shift } else { v.shallow_shift };
    let bits = if deep { v.deep_mask } else { v.shallow_mask };
    let swing = (fnum >> shift) & bits;
    let add = if vibrato { swing } else { 0 };
    let lower = fnum.wrapping_sub(add) & 1023;
    let higher = fnum + add;
    let number = if v.down { lower } else { higher };
    (((number << block) >> 1) * multiple) >> 1
}

/// The phase for the next pass, from nought when the slot starts again.
#[inline]
pub(crate) fn next_phase(phase: u32, increment: u32, restart: bool) -> u32 {
    let from = if restart { 0 } else { phase };
    (from + increment) & 0x7_ffff
}

/// What the operator reads of the phase: its top ten bits.
#[inline]
pub(crate) fn phase_out(phase: u32) -> i32 {
    ((phase >> 9) & 1023).cast_signed()
}

/// The modulation of an operator, by what it is modulated by (0 nothing, 1 the
/// stage before, 2 feedback): the low ten bits of the slot that feeds it, or
/// its feedback, the sum of its last two outputs shifted down.
#[inline]
pub(crate) fn modulation(source: i32, history: i32, feedback_shift: i32, by: i32) -> i32 {
    let stage = source & 1023;
    let feedback = (history >> feedback_shift) & 1023;
    let other = if by == 2 { feedback } else { 0 };
    if by == 1 { stage } else { other }
}

/// What a ROM holds at an index of eight bits.
#[inline]
fn look_up(rom: &[i32; 256], index: i32) -> i32 {
    rom[usize::try_from(index).expect("an index of eight bits")]
}

/// The operator's output: a 13-bit value, its negative in ones' complement,
/// held signed.
#[inline]
pub(crate) fn operator_output(phase: i32, wave: &Wave, eg_out: i32) -> i32 {
    let shifted = (phase << wave.phase_shift) & 1023;
    let turned = shifted ^ 511;
    let phase2 = if (shifted & wave.turn_bit) != 0 {
        turned
    } else {
        shifted
    };
    let mute = (phase & wave.mute_bit) != 0;
    let negative = (phase2 & wave.sign_bit) != 0;
    let low = phase2 & 255;
    let index = if wave.square != 0 { 255 } else { low };
    let ramp = (phase2 & 511) << 3;
    let sine = look_up(&LOGSIN_ROM, index);
    let shape = if wave.sawtooth != 0 { ramp } else { sine };
    let sum = shape + (eg_out << 3);
    let att = if (sum & 4096) != 0 { 4095 } else { sum };
    let magnitude = ((look_up(&EXP_ROM, att & 255) | 0x400) << 1) >> (att >> 8);
    let inverted = magnitude ^ 8191;
    let signed_value = if negative { inverted } else { magnitude };
    let value = if mute { 0 } else { signed_value };
    let wrapped = value - 8192;
    if (value & 0x1000) != 0 { wrapped } else { value }
}

/// The noise: a 23-bit shift register fed with the sum of its bits 22 and 8,
/// and with a one when it is empty.
#[expect(
    clippy::verbose_bit_mask,
    reason = "the chip's test of an empty register, its 23 bits nought, which a count of trailing zeros would hide"
)]
pub(crate) const fn noise_step(s: u32) -> u32 {
    let mut bit = ((s >> 22) ^ (s >> 8)) & 1;
    if (s & 0x7f_ffff) == 0 {
        bit = 1;
    }
    ((s << 1) | bit) & 0x7f_ffff
}

/// The 18 steps of a pass at once, for a register that is not empty: the bits
/// fed in go in below the old ones, the first nine of them the sums of bits the
/// register holds now, the last nine taking the first nine for bit 8.
pub(crate) const fn noise_pass(s: u32) -> u32 {
    let high = ((s >> 5) ^ (s << 9)) & 0x3_fe00;
    let low = ((s >> 5) ^ (high >> 9)) & 0x1ff;
    ((s << 18) | high | low) & 0x7f_ffff
}

/// Whether [`noise_pass`] makes of a register what 18 of [`noise_step`] make.
const fn noise_pass_steps(s: u32) -> bool {
    let mut t = s;
    let mut i = 0;
    while i < 18 {
        t = noise_step(t);
        i += 1;
    }
    noise_pass(s) == t
}
const _: () = assert!(noise_pass_steps(0x28_7844) && noise_pass_steps(0x00_0001) && noise_pass_steps(0x7f_ffff));

/// The bit of the noise the hi-hat reads: its top bit 6 steps after the pass's
/// first step, which is bit 16 as that step leaves it.
pub(crate) const fn hi_hat_noise(s: u32) -> u32 {
    (s >> 16) & 1
}

/// The bit of the noise the snare reads, 8 steps after the pass's first step:
/// bit 14 as that step leaves it.
pub(crate) const fn snare_noise(s: u32) -> u32 {
    (s >> 14) & 1
}

/// The bits of the hi-hat's and the top cymbal's phases that the rhythm
/// phases take, as the passes in assembly read and write them (`asm/pass.h`).
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub(crate) struct RhythmBits {
    /// Bit 2 of the hi-hat's phase.
    pub(crate) hh2: u32,
    /// Bit 3 of the hi-hat's phase.
    pub(crate) hh3: u32,
    /// Bit 7 of the hi-hat's phase.
    pub(crate) hh7: u32,
    /// Bit 8 of the hi-hat's phase.
    pub(crate) hh8: u32,
    /// Bit 3 of the top cymbal's phase.
    pub(crate) tc3: u32,
    /// Bit 5 of the top cymbal's phase.
    pub(crate) tc5: u32,
}

impl RhythmBits {
    /// The bits of the hi-hat's phase.
    pub(crate) fn take_hi_hat(&mut self, phase: i32) {
        let p = phase.cast_unsigned();
        self.hh2 = (p >> 2) & 1;
        self.hh3 = (p >> 3) & 1;
        self.hh7 = (p >> 7) & 1;
        self.hh8 = (p >> 8) & 1;
    }

    /// The bits of the top cymbal's phase.
    pub(crate) fn take_top_cymbal(&mut self, phase: i32) {
        let p = phase.cast_unsigned();
        self.tc3 = (p >> 3) & 1;
        self.tc5 = (p >> 5) & 1;
    }

    /// The ring of the hi-hat's and the top cymbal's bits.
    pub(crate) fn ring(&self) -> u32 {
        (self.hh2 ^ self.hh7) | (self.tc5 ^ self.hh3) | (self.tc5 ^ self.tc3)
    }

    /// The hi-hat's phase, by a bit of the noise.
    pub(crate) fn hi_hat(&self, noise: u32) -> i32 {
        let rm = self.ring();
        ((rm << 9) | if (noise ^ rm) != 0 { 0xd0 } else { 0x34 }).cast_signed()
    }

    /// The snare's phase, by a bit of the noise.
    pub(crate) fn snare(&self, noise: u32) -> i32 {
        ((self.hh8 << 9) | ((noise ^ self.hh8) << 8)).cast_signed()
    }

    /// The top cymbal's phase.
    pub(crate) fn top_cymbal(&self) -> i32 {
        ((self.ring() << 9) | 0x80).cast_signed()
    }
}

/// The counters of the chip: the one of the tremolo and the vibrato, and the
/// envelope's timer. The timer here is one ahead of the chip's, as it is
/// counted up after it is read.
#[repr(C)]
#[derive(Clone, Copy)]
pub(crate) struct Counters {
    /// The count of the tremolo and the vibrato.
    pub(crate) lfo: u32,
    /// 0 to 209, up and back down in the level.
    pub(crate) tremolo_position: u16,
    /// The tremolo takes a step after the next pass.
    pub(crate) tremolo_step: u8,
    /// The position of the vibrato, 0 to 7.
    pub(crate) vibrato_position: u8,
    /// The envelope's timer.
    pub(crate) eg_timer: u64,
    /// The timer's second half is on.
    pub(crate) eg_second: u8,
    /// The step the timer's lowest set bit gives.
    pub(crate) eg_add: u8,
    /// The timer's low two bits.
    pub(crate) eg_timer_low: u8,
    /// The timer carried out of its 36 bits.
    pub(crate) eg_carry: u8,
}

impl Counters {
    /// The counters as the chip starts them.
    pub(crate) const fn new() -> Self {
        Self {
            lfo: 0,
            tremolo_position: 0,
            tremolo_step: 0,
            vibrato_position: 0,
            eg_timer: 1,
            eg_second: 0,
            eg_add: 0,
            eg_timer_low: 0,
            eg_carry: 0,
        }
    }

    /// The level of the tremolo.
    pub(crate) fn tremolo_level(&self) -> i32 {
        let position = i32::from(self.tremolo_position);
        if position < 105 { position } else { 210 - position }
    }

    /// What the envelope goes by in this pass.
    pub(crate) fn envelope_timing(&self) -> EnvelopeTiming {
        EnvelopeTiming {
            second: i32::from(self.eg_second),
            add: i32::from(self.eg_add),
            high_from: HIGH_STEP_FROM[usize::from(self.eg_timer_low)],
        }
    }

    /// At the end of a pass. The tremolo takes a step two passes after the one
    /// whose count calls for it, the vibrato one pass after. A constant
    /// function, for the counters after a reset; it cannot yet call std's
    /// conversions, so a number that becomes a byte is taken as its low byte,
    /// which holds it whole.
    pub(crate) const fn advance(&mut self) {
        if self.tremolo_step != 0 {
            self.tremolo_position = (self.tremolo_position + 1) % 210;
        }
        self.tremolo_step = if (self.lfo & 0x3f) == 0x3f { 1 } else { 0 };
        if (self.lfo & 0x3ff) == 0x3ff {
            self.vibrato_position = (self.vibrato_position + 1) & 7;
        }
        self.lfo = (self.lfo + 1) & 0x3ff;
        if self.eg_second != 0 {
            // The timer's lowest set bit among its low 13 (13 for none).
            let shift = ((self.eg_timer & 0x1fff) | 0x2000).trailing_zeros();
            self.eg_add = if shift > 12 { 0 } else { (shift + 1).to_le_bytes()[0] };
            self.eg_timer_low = (self.eg_timer & 3).to_le_bytes()[0];
        }
        if self.eg_carry != 0 || self.eg_second != 0 {
            if self.eg_timer == 0xf_ffff_ffff {
                self.eg_timer = 0;
                self.eg_carry = 1;
            } else {
                self.eg_timer += 1;
                self.eg_carry = 0;
            }
        }
        self.eg_second ^= 1;
    }
}
