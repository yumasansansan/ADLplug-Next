// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! Where the passes find what they read and write: the lanes of the core
//! ([`Lanes`]), which every pass works on, and what a pass written by hand goes
//! by besides ([`PassInputs`], x86-64 and AArch64). The lanes are laid out as
//! the C++ version laid them out, as the members of its core from its rates to
//! its shared feedback history, in their order and of their types, and after
//! them the phase increments, which the passes take from the lanes where the C++
//! version's passes worked them out; what a pass goes by besides is the C++
//! version's without what those passes took for the increments. The passes in
//! assembly take the offsets from `asm/pass.h`, and the assertions here check
//! each of them against the structures as they are compiled, with the numbers
//! read from `asm/pass.h` itself, so that a field out of its place stops the
//! build.
#![forbid(unsafe_code)]

use std::mem::offset_of;

#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
use crate::roms::{EXP_ROM, LOGSIN_ROM};
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
use crate::ymf262;
use crate::ymf262::{Counters, RhythmBits};

/// The lanes the slots are kept in: the 36 slots in stages of 16, 16, 8 and 8
/// lanes (`core.rs` says how), every loop over them a whole number of vectors.
pub(crate) const LANES: usize = 48;

/// By lane: what the passes work with, worked out from what the slots read when
/// that changes (`core.rs`, `ymf262.rs` say what each is), and the state after
/// them. They come first in the core, as what every pass reads, aligned for the
/// vectors, each array of 32 bits a whole number of 64 bytes, and nothing
/// smaller pads them apart; the passes in assembly read and write them at the
/// offsets of `asm/pass.h`.
#[repr(C, align(64))]
pub(crate) struct Lanes {
    /// The rates of the envelope, a byte for each state.
    pub(crate) rates: [u32; LANES],
    /// The key scaling.
    pub(crate) key_scale: [i32; LANES],
    /// Its low two bits.
    pub(crate) key_scale_low: [i32; LANES],
    /// The level of the sustain, in steps of 16.
    pub(crate) sustain: [i32; LANES],
    /// Keyed on (1) or off (0).
    pub(crate) key_on: [i32; LANES],
    /// The attenuation of the key scale level and the total level.
    pub(crate) level_offset: [i32; LANES],
    /// The depth of the tremolo: 0 none, 1 shallow, 2 deep.
    pub(crate) tremolo_depth: [i32; LANES],
    /// The frequency number, for the increments.
    pub(crate) fnum_v: [u32; LANES],
    /// The block, for the increments.
    pub(crate) block_v: [u32; LANES],
    /// Twice the multiple of the frequency, for the increments.
    pub(crate) multiple_v: [u32; LANES],
    /// The vibrato is on (1) or off (0), for the increments.
    pub(crate) vibrato: [u32; LANES],
    /// The vibrato is deep (1) or shallow (0), for the increments.
    pub(crate) vibrato_depth: [u32; LANES],
    /// The waveform: how far the phase is shifted up.
    pub(crate) phase_shift: [i32; LANES],
    /// The waveform: the bit of the shifted phase that turns it back.
    pub(crate) turn_bit: [i32; LANES],
    /// The waveform: the bit of the phase that silences the output.
    pub(crate) mute_bit: [i32; LANES],
    /// The waveform: the bit of the shifted phase that makes the output
    /// negative.
    pub(crate) sign_bit: [i32; LANES],
    /// The waveform is the square wave.
    pub(crate) square: [i32; LANES],
    /// The waveform is the sawtooth.
    pub(crate) sawtooth: [i32; LANES],
    /// What the operator is modulated by: 0 nothing, 1 the stage before, 2
    /// feedback.
    pub(crate) modulated_by: [i32; LANES],
    /// How far its feedback is shifted down.
    pub(crate) feedback_shift: [i32; LANES],
    /// How loud the slot is on output A in its pass's sample.
    pub(crate) a_now: [i32; LANES],
    /// How loud on output A in the next sample.
    pub(crate) a_late: [i32; LANES],
    /// How loud on output B in its pass's sample.
    pub(crate) b_now: [i32; LANES],
    /// How loud on output B in the next sample.
    pub(crate) b_late: [i32; LANES],
    /// [`Lanes::a_now`] with soft panning, in sixteenths of sixteen bits.
    pub(crate) soft_a_now: [i32; LANES],
    /// [`Lanes::a_late`] with soft panning.
    pub(crate) soft_a_late: [i32; LANES],
    /// [`Lanes::b_now`] with soft panning.
    pub(crate) soft_b_now: [i32; LANES],
    /// [`Lanes::b_late`] with soft panning.
    pub(crate) soft_b_late: [i32; LANES],
    /// Soft panning: the gain of output A of the slot's channel, 65536 for full.
    pub(crate) gain_a: [i32; LANES],
    /// The gain of output B.
    pub(crate) gain_b: [i32; LANES],
    /// The slot, -1 for a lane that has none: a constant, kept here all the same
    /// (the reset fills it), since from a constant table the compiler works a
    /// comparison with it out from the lane's number instead, in 64 bits, as 48
    /// lanes do not fit in 32, and the vectorized loops pay for that.
    pub(crate) lane_slot: [i32; LANES],
    /// The state: the phase.
    pub(crate) phase: [u32; LANES],
    /// The level of the envelope, as the portable pass keeps it.
    pub(crate) level: [i32; LANES],
    /// The state of the envelope, as the portable pass keeps it.
    pub(crate) state: [i32; LANES],
    /// The operator's output of this pass.
    pub(crate) out: [i32; LANES],
    /// Of the last pass.
    pub(crate) out_1: [i32; LANES],
    /// Of the pass before.
    pub(crate) out_2: [i32; LANES],
    /// The attenuation the operator takes in this pass.
    pub(crate) eg_out: [i32; LANES],
    /// What the operator reads of the phase in this pass.
    pub(crate) phase_out: [i32; LANES],
    /// Keyed on from release in this pass (1).
    pub(crate) restart: [i32; LANES],
    /// The levels of the envelopes as the passes written by hand keep them: in
    /// 16 bits, in the order the core's pass says (`pass_avx2.S`,
    /// `pass_avx512.S`, `pass_neon.S`), a core keeping to one pass.
    pub(crate) level16: [i16; LANES],
    /// The states of the envelopes, likewise.
    pub(crate) state16: [i16; LANES],
    /// What the slots heard late made on output A in the last pass, soft
    /// panned.
    pub(crate) a_carry_soft: i64,
    /// And on output B.
    pub(crate) b_carry_soft: i64,
    /// The counters of the chip.
    pub(crate) counters: Counters,
    /// What the slots heard late made on output A in the last pass, without
    /// soft panning.
    pub(crate) a_carry: i32,
    /// And on output B.
    pub(crate) b_carry: i32,
    /// The noise, as the pass's first step leaves it.
    pub(crate) noise: u32,
    /// The bits of the hi-hat's and the top cymbal's phases.
    pub(crate) rhythm_bits: RhythmBits,
    /// The chip stores the last two values of slots m and m + 18 for feedback
    /// in one queue of four, which moves on as either value is stored. Rhythm
    /// mode stops storing those of slots 13 and 14, and so mixes theirs with
    /// those of slots 31 and 32; the two queues are kept as they are.
    pub(crate) shared_history: [[i32; 4]; 2],
    /// The phase increments of the lanes in this pass, which every pass takes:
    /// worked out from the frequency numbers, the blocks, the multiples and the
    /// vibrato when one of them changes (`Core::refresh_increments`), not in
    /// each pass, as they stay the same between the writes and the vibrato's
    /// steps.
    pub(crate) increment: Aligned<[u32; LANES]>,
    /// The vibrato's position that the increments are for, of 0 to 7, or
    /// [`u8::MAX`] when what they come of has changed since they were worked
    /// out.
    pub(crate) increments_for: u8,
}

/// Values on a line of the cache of their own, for the lanes after the single
/// values, which the passes load 16, 32 and 64 bytes at a time.
#[repr(C, align(64))]
pub(crate) struct Aligned<T>(pub(crate) T);

impl Lanes {
    /// Lanes of noughts, which the core's reset then fills.
    pub(crate) fn new() -> Self {
        Self {
            rates: [0; LANES],
            key_scale: [0; LANES],
            key_scale_low: [0; LANES],
            sustain: [0; LANES],
            key_on: [0; LANES],
            level_offset: [0; LANES],
            tremolo_depth: [0; LANES],
            fnum_v: [0; LANES],
            block_v: [0; LANES],
            multiple_v: [0; LANES],
            vibrato: [0; LANES],
            vibrato_depth: [0; LANES],
            phase_shift: [0; LANES],
            turn_bit: [0; LANES],
            mute_bit: [0; LANES],
            sign_bit: [0; LANES],
            square: [0; LANES],
            sawtooth: [0; LANES],
            modulated_by: [0; LANES],
            feedback_shift: [0; LANES],
            a_now: [0; LANES],
            a_late: [0; LANES],
            b_now: [0; LANES],
            b_late: [0; LANES],
            soft_a_now: [0; LANES],
            soft_a_late: [0; LANES],
            soft_b_now: [0; LANES],
            soft_b_late: [0; LANES],
            gain_a: [0; LANES],
            gain_b: [0; LANES],
            lane_slot: [0; LANES],
            phase: [0; LANES],
            level: [0; LANES],
            state: [0; LANES],
            out: [0; LANES],
            out_1: [0; LANES],
            out_2: [0; LANES],
            eg_out: [0; LANES],
            phase_out: [0; LANES],
            restart: [0; LANES],
            level16: [0; LANES],
            state16: [0; LANES],
            a_carry_soft: 0,
            b_carry_soft: 0,
            counters: Counters::new(),
            a_carry: 0,
            b_carry: 0,
            noise: 0,
            rhythm_bits: RhythmBits::default(),
            shared_history: [[0; 4]; 2],
            increment: Aligned([0; LANES]),
            increments_for: u8::MAX,
        }
    }
}

/// What a pass written by hand goes by besides the lanes: vectors of lanes, in
/// 16 bits for the envelope, then single values, worked out of the core's
/// counters, its noise and its routing for the pass ([`PassInputs::new`]), and
/// the tables the pass looks up.
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
#[repr(C, align(32))]
pub(crate) struct PassInputs {
    /// The key scaling from which the timer's high step falls, less 1.
    high_from_less_1: [i16; 16],
    /// The step the timer's lowest set bit gives.
    add: [i16; 16],
    /// By the tremolo's depth (0 to 2): nought, the shallow, the deep; twice.
    tremolo: [u8; 32],
    /// The envelope timer's second half is on.
    eg_second: i32,
    /// The hi-hat takes a phase of its own.
    rhythm_13: i32,
    /// The snare takes a phase of its own.
    rhythm_16: i32,
    /// The top cymbal takes a phase of its own.
    rhythm_17: i32,
    /// The value of slot 13 is held out of its feedback queue.
    hold_13: i32,
    /// The value of slot 14 is held out of its feedback queue.
    hold_14: i32,
    /// The outputs are soft panned.
    soft_pan: i32,
    /// The bit of the noise the hi-hat reads.
    hi_hat_noise: i32,
    /// The bit of the noise the snare reads.
    snare_noise: i32,
    /// The log-sine ROM.
    logsin: *const i32,
    /// The exponent ROM with the 0x400 above it, shifted up one.
    exp: *const i32,
    /// The log-sine ROM in 16 bits, for the pass for AVX-512.
    logsin16: *const u16,
    /// The exponent ROM as above in 16 bits.
    exp16: *const u16,
    /// The log-sine ROM as its low bytes, then its high bytes, for the forms of
    /// the pass for AVX-512 with VBMI.
    logsin8: *const u8,
    /// The exponent ROM likewise.
    exp8: *const u8,
}

#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
impl PassInputs {
    /// What a pass written by hand goes by for the pass that the counters and
    /// the noise stand at: what they give for it, what the routing says of the
    /// drums (whether the hi-hat, the snare and the top cymbal take phases of
    /// their own) and of the shared feedback history (whether the values of
    /// slots 13 and 14 are held out of their queues), whether the outputs are
    /// soft panned, and the core's tables. It is the one way to make one, so
    /// that the tables it points at are always the core's.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "made in each tier's form of a sample, it is stored as wide as the pass loads it, and then loaded from the stores"
    )]
    pub(crate) fn new(counters: &Counters, noise: u32, rhythm: [u8; 3], hold: [u8; 2], soft_pan: bool) -> Self {
        let timing = counters.envelope_timing();
        let high_from_less_1 = i16::try_from(timing.high_from - 1).expect("a key scaling of two bits");
        let add = i16::try_from(timing.add).expect("a step of the timer below 14");
        let tremolo_level = counters.tremolo_level();
        let shallow = u8::try_from(tremolo_level >> 4).expect("a tremolo of a byte");
        let deep = u8::try_from(tremolo_level >> 2).expect("a tremolo of a byte");
        let mut tremolo = [0; 32];
        tremolo[1] = shallow;
        tremolo[2] = deep;
        tremolo[17] = shallow;
        tremolo[18] = deep;
        let [rhythm_13, rhythm_16, rhythm_17] = rhythm.map(i32::from);
        let [hold_13, hold_14] = hold.map(i32::from);
        Self {
            high_from_less_1: [high_from_less_1; 16],
            add: [add; 16],
            tremolo,
            eg_second: timing.second,
            rhythm_13,
            rhythm_16,
            rhythm_17,
            hold_13,
            hold_14,
            soft_pan: i32::from(soft_pan),
            hi_hat_noise: ymf262::hi_hat_noise(noise).cast_signed(),
            snare_noise: ymf262::snare_noise(noise).cast_signed(),
            logsin: LOGSIN.0.as_ptr(),
            exp: EXP_MAGNITUDE.0.as_ptr(),
            logsin16: LOGSIN_ROM16.0.as_ptr(),
            exp16: EXP_MAGNITUDE16.0.as_ptr(),
            logsin8: LOGSIN_ROM8.0.as_ptr(),
            exp8: EXP_MAGNITUDE8.0.as_ptr(),
        }
    }
}

/// A table of the passes written by hand, aligned to a line of the cache.
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
#[repr(C, align(64))]
struct Table<T>(T);

/// The log-sine's ROM as the passes written by hand look it up: a copy of
/// their own, so that the address of the one the portable pass looks up stays
/// in the crate. Where it went out to the assembly, the compiler could not tell
/// that the lanes do not hold it, and the operators' loops, which look it up
/// and write the lanes, were vectorized for no level of x86-64 ("cannot
/// identify array bounds", as LLVM says).
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
static LOGSIN: Table<[i32; 256]> = Table(LOGSIN_ROM);

/// The exponent's ROM as the passes written by hand look it up: with the 0x400
/// that the operator puts above it, shifted up one.
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
static EXP_MAGNITUDE: Table<[i32; 256]> = Table({
    let mut t = [0; 256];
    let mut i = 0;
    while i < 256 {
        t[i] = (EXP_ROM[i] | 0x400) << 1;
        i += 1;
    }
    t
});

/// The log-sine's ROM in 16 bits, which every entry fits, for the pass for
/// AVX-512 to hold in its registers.
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
static LOGSIN_ROM16: Table<[u16; 256]> = Table(words(&LOGSIN_ROM));
/// The exponent's ROM as the passes written by hand look it up, likewise.
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
static EXP_MAGNITUDE16: Table<[u16; 256]> = Table(words(&EXP_MAGNITUDE.0));
/// The log-sine's ROM as its low bytes, then its high bytes, for the forms of
/// the pass for AVX-512 that look it up by VBMI.
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
static LOGSIN_ROM8: Table<[u8; 512]> = Table(bytes_of(&LOGSIN_ROM16.0));
/// The exponent's ROM likewise.
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
static EXP_MAGNITUDE8: Table<[u8; 512]> = Table(bytes_of(&EXP_MAGNITUDE16.0));

/// The entries of a table, each of which fits 16 bits, in 16 bits: their low
/// two bytes, the two above them being nought, as is checked (a constant
/// function cannot yet call std's conversions).
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
const fn words(values: &[i32; 256]) -> [u16; 256] {
    let mut t = [0; 256];
    let mut i = 0;
    while i < 256 {
        let [low, high, third, fourth] = values[i].to_le_bytes();
        assert!(third == 0 && fourth == 0, "an entry of 16 bits");
        t[i] = u16::from_le_bytes([low, high]);
        i += 1;
    }
    t
}

/// A table of 16 bits as its low bytes, then its high bytes.
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
const fn bytes_of(words: &[u16; 256]) -> [u8; 512] {
    let mut t = [0; 512];
    let mut i = 0;
    while i < 256 {
        let [low, high] = words[i].to_le_bytes();
        t[i] = low;
        t[256 + i] = high;
        i += 1;
    }
    t
}

/// `asm/pass.h`, whose numbers the assertions below read.
const PASS_H: &[u8] = include_bytes!("../asm/pass.h");

/// The number `asm/pass.h` defines as `name`: the decimal digits after
/// `#define <name> ` at the start of a line. A name it does not define, or
/// defines as anything but a number, stops the build.
const fn defined(name: &str) -> usize {
    let name = name.as_bytes();
    let mut line = 0;
    while line < PASS_H.len() {
        if defines_at(line, name) {
            let start = line + DEFINE.len() + name.len() + 1;
            let mut end = start;
            let mut number = 0;
            while end < PASS_H.len() && PASS_H[end].is_ascii_digit() {
                number = number * 10 + digit(PASS_H[end]);
                end += 1;
            }
            assert!(
                end > start && end < PASS_H.len() && PASS_H[end] == b'\n',
                "asm/pass.h defines a name as something other than a number"
            );
            return number;
        }
        while line < PASS_H.len() && PASS_H[line] != b'\n' {
            line += 1;
        }
        line += 1;
    }
    panic!("asm/pass.h does not define a name that the Rust code checks")
}

/// What a line of `asm/pass.h` defines a name with.
const DEFINE: &[u8] = b"#define ";

/// Whether the line of `asm/pass.h` from byte `line` defines `name`.
const fn defines_at(line: usize, name: &[u8]) -> bool {
    let end = line + DEFINE.len() + name.len();
    if end >= PASS_H.len() {
        return false;
    }
    let mut k = 0;
    while k < DEFINE.len() {
        if PASS_H[line + k] != DEFINE[k] {
            return false;
        }
        k += 1;
    }
    let mut k = 0;
    while k < name.len() {
        if PASS_H[line + DEFINE.len() + k] != name[k] {
            return false;
        }
        k += 1;
    }
    PASS_H[end] == b' '
}

/// The value of a decimal digit, as the low byte of a number that is nought
/// above it: a constant function cannot yet call std's conversions.
const fn digit(byte: u8) -> usize {
    let mut bytes = [0; size_of::<usize>()];
    bytes[0] = byte - b'0';
    usize::from_le_bytes(bytes)
}

// The lanes as the passes in assembly read and write them: aligned for their
// aligned loads and stores of 32 bytes, at the offsets of asm/pass.h.
const _: () = {
    assert!(align_of::<Lanes>() == 64);
    assert!(offset_of!(Lanes, rates) == defined("ADLPLUG_OPL3_RATES"));
    assert!(offset_of!(Lanes, key_scale_low) == defined("ADLPLUG_OPL3_KEY_SCALE_LOW"));
    assert!(offset_of!(Lanes, sustain) == defined("ADLPLUG_OPL3_SUSTAIN"));
    assert!(offset_of!(Lanes, key_on) == defined("ADLPLUG_OPL3_KEY_ON"));
    assert!(offset_of!(Lanes, level_offset) == defined("ADLPLUG_OPL3_LEVEL_OFFSET"));
    assert!(offset_of!(Lanes, tremolo_depth) == defined("ADLPLUG_OPL3_TREMOLO_DEPTH"));
    assert!(offset_of!(Lanes, phase_shift) == defined("ADLPLUG_OPL3_PHASE_SHIFT"));
    assert!(offset_of!(Lanes, turn_bit) == defined("ADLPLUG_OPL3_TURN_BIT"));
    assert!(offset_of!(Lanes, mute_bit) == defined("ADLPLUG_OPL3_MUTE_BIT"));
    assert!(offset_of!(Lanes, sign_bit) == defined("ADLPLUG_OPL3_SIGN_BIT"));
    assert!(offset_of!(Lanes, square) == defined("ADLPLUG_OPL3_SQUARE"));
    assert!(offset_of!(Lanes, sawtooth) == defined("ADLPLUG_OPL3_SAWTOOTH"));
    assert!(offset_of!(Lanes, modulated_by) == defined("ADLPLUG_OPL3_MODULATED_BY"));
    assert!(offset_of!(Lanes, feedback_shift) == defined("ADLPLUG_OPL3_FEEDBACK_SHIFT"));
    assert!(offset_of!(Lanes, a_now) == defined("ADLPLUG_OPL3_A_NOW"));
    assert!(offset_of!(Lanes, a_late) == defined("ADLPLUG_OPL3_A_LATE"));
    assert!(offset_of!(Lanes, b_now) == defined("ADLPLUG_OPL3_B_NOW"));
    assert!(offset_of!(Lanes, b_late) == defined("ADLPLUG_OPL3_B_LATE"));
    assert!(offset_of!(Lanes, soft_a_now) == defined("ADLPLUG_OPL3_SOFT_A_NOW"));
    assert!(offset_of!(Lanes, soft_a_late) == defined("ADLPLUG_OPL3_SOFT_A_LATE"));
    assert!(offset_of!(Lanes, soft_b_now) == defined("ADLPLUG_OPL3_SOFT_B_NOW"));
    assert!(offset_of!(Lanes, soft_b_late) == defined("ADLPLUG_OPL3_SOFT_B_LATE"));
    assert!(offset_of!(Lanes, phase) == defined("ADLPLUG_OPL3_PHASE"));
    assert!(offset_of!(Lanes, out) == defined("ADLPLUG_OPL3_OUT"));
    assert!(offset_of!(Lanes, out_1) == defined("ADLPLUG_OPL3_OUT_1"));
    assert!(offset_of!(Lanes, out_2) == defined("ADLPLUG_OPL3_OUT_2"));
    assert!(offset_of!(Lanes, eg_out) == defined("ADLPLUG_OPL3_EG_OUT"));
    assert!(offset_of!(Lanes, phase_out) == defined("ADLPLUG_OPL3_PHASE_OUT"));
    assert!(offset_of!(Lanes, level16) == defined("ADLPLUG_OPL3_LEVEL16"));
    assert!(offset_of!(Lanes, state16) == defined("ADLPLUG_OPL3_STATE16"));
    assert!(offset_of!(Lanes, a_carry_soft) == defined("ADLPLUG_OPL3_CARRY_SOFT"));
    assert!(offset_of!(Lanes, b_carry_soft) == defined("ADLPLUG_OPL3_CARRY_SOFT") + 8);
    assert!(offset_of!(Lanes, a_carry) == defined("ADLPLUG_OPL3_CARRY"));
    assert!(offset_of!(Lanes, b_carry) == defined("ADLPLUG_OPL3_CARRY") + 4);
    assert!(offset_of!(Lanes, rhythm_bits) == defined("ADLPLUG_OPL3_RHYTHM_BITS"));
    assert!(offset_of!(RhythmBits, hh2) == 0 && offset_of!(RhythmBits, hh3) == 4);
    assert!(offset_of!(RhythmBits, hh7) == 8 && offset_of!(RhythmBits, hh8) == 12);
    assert!(offset_of!(RhythmBits, tc3) == 16 && offset_of!(RhythmBits, tc5) == 20);
    assert!(offset_of!(Lanes, shared_history) == defined("ADLPLUG_OPL3_SHARED_HISTORY"));
    assert!(offset_of!(Lanes, increment) == defined("ADLPLUG_OPL3_INCREMENT"));
};

// What a pass goes by besides, likewise: aligned for the aligned loads of 32
// bytes the pass for AVX2 makes of it.
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
const _: () = {
    assert!(align_of::<PassInputs>() == 32);
    assert!(offset_of!(PassInputs, high_from_less_1) == defined("ADLPLUG_OPL3_IN_HIGH_FROM_LESS_1"));
    assert!(offset_of!(PassInputs, add) == defined("ADLPLUG_OPL3_IN_ADD"));
    assert!(offset_of!(PassInputs, tremolo) == defined("ADLPLUG_OPL3_IN_TREMOLO"));
    assert!(offset_of!(PassInputs, eg_second) == defined("ADLPLUG_OPL3_IN_EG_SECOND"));
    assert!(offset_of!(PassInputs, rhythm_13) == defined("ADLPLUG_OPL3_IN_RHYTHM_13"));
    assert!(offset_of!(PassInputs, rhythm_16) == defined("ADLPLUG_OPL3_IN_RHYTHM_16"));
    assert!(offset_of!(PassInputs, rhythm_17) == defined("ADLPLUG_OPL3_IN_RHYTHM_17"));
    assert!(offset_of!(PassInputs, hold_13) == defined("ADLPLUG_OPL3_IN_HOLD_13"));
    assert!(offset_of!(PassInputs, hold_14) == defined("ADLPLUG_OPL3_IN_HOLD_14"));
    assert!(offset_of!(PassInputs, soft_pan) == defined("ADLPLUG_OPL3_IN_SOFT_PAN"));
    assert!(offset_of!(PassInputs, hi_hat_noise) == defined("ADLPLUG_OPL3_IN_HI_HAT_NOISE"));
    assert!(offset_of!(PassInputs, snare_noise) == defined("ADLPLUG_OPL3_IN_SNARE_NOISE"));
    assert!(offset_of!(PassInputs, logsin) == defined("ADLPLUG_OPL3_IN_LOGSIN"));
    assert!(offset_of!(PassInputs, exp) == defined("ADLPLUG_OPL3_IN_EXP"));
    assert!(offset_of!(PassInputs, logsin16) == defined("ADLPLUG_OPL3_IN_LOGSIN16"));
    assert!(offset_of!(PassInputs, exp16) == defined("ADLPLUG_OPL3_IN_EXP16"));
    assert!(offset_of!(PassInputs, logsin8) == defined("ADLPLUG_OPL3_IN_LOGSIN8"));
    assert!(offset_of!(PassInputs, exp8) == defined("ADLPLUG_OPL3_IN_EXP8"));
};
