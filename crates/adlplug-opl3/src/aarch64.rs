// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! The passes written by hand in assembly for AArch64 (`asm/aarch64/`),
//! declared, the safe function that calls them, and the forms of a sample by
//! them compiled for SVE and SVE2. Each pass does what [`Core::generate`] does
//! between taking its writes in and moving its counters on: the envelopes and
//! the phases, the rhythm phases, the operators stage by stage, the feedback
//! history and the sums of the two outputs. `pass_neon.S` holds the pass for
//! NEON (Advanced SIMD), which every AArch64 processor has, Apple's and those
//! that Windows and Linux run on alike, and its forms for SHA3, for SVE and for
//! SVE2.
//!
//! The functions follow the procedure call standard of AArch64 (AAPCS64), which
//! macOS, Windows and Linux share for what it takes and keeps: each takes the
//! lanes, the inputs and the frame in x0, x1 and x2, and leaves alone the
//! registers that the standard has a function keep, the low halves of v8 to v15
//! among them, and x18, which Apple's and Windows' conventions keep for the
//! system, and the stack. So the one `extern "C"` declaration is right on all
//! three, and the functions need no unwinding information on any of them.
//!
//! NEON is AArch64's baseline, which the build is compiled for as well: the code
//! around the passes for NEON and SHA3 is the build's. Around the passes for SVE
//! and SVE2 it is compiled for those as well (`#[target_feature]`), as
//! `x86_64.rs` compiles it for x86-64-v3 and v4. Whether the processor has a
//! level's extensions, and the extensions of the tier of the code around its
//! pass, is asked once, as a core takes its pass ([`Hand::new`]), and a [`Hand`]
//! exists only for a level and a tier the processor has: its existence is what
//! makes the calls sound, and each call is then the one unsafe operation of its
//! block.
//!
//! [`Core::generate`]: crate::Core::generate
#![allow(
    unsafe_code,
    reason = "the calls into assembly and into the forms of a sample compiled for SVE and SVE2, and the experiments' chain of additions in assembly, each the one operation of a block whose comment says why it is sound"
)]

use adlplug_cpu::{Features, Level, Tier};

use crate::core::Core;
#[cfg(feature = "experiment")]
use crate::core::experiment::Part;
use crate::pass::{Lanes, PassInputs};

/// A pass written by hand: the lanes, what the pass goes by besides, and the
/// frame of two samples, output A then output B, that it writes.
type Function = unsafe extern "C" fn(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);

unsafe extern "C" {
    /// The pass for NEON (`pass_neon.S`).
    fn adlplug_opl3_pass_neon(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    /// The pass for NEON with SHA3's BCAX (`pass_neon.S`).
    fn adlplug_opl3_pass_neon_sha3(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    /// The pass for SVE, with its gathers and its shifts (`pass_neon.S`).
    fn adlplug_opl3_pass_sve(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    /// The pass for SVE2, with SVE2's BCAX and BSL1N besides (`pass_neon.S`).
    fn adlplug_opl3_pass_sve2(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
}

/// The pass written by hand for a level that the processor has, and the tier of
/// the code around it, one the processor has: made by [`Hand::new`] alone,
/// which asks the processor, so that one that exists runs.
#[derive(Debug, Clone, Copy)]
pub(crate) struct Hand {
    /// The level's pass.
    function: Function,
    /// The tier of the code around it.
    tier: Tier,
}

impl Hand {
    /// The pass of a level, where it has one in assembly and the processor has
    /// its extensions, as adlplug-cpu reports them: `neon`'s, `neon-sha3`'s,
    /// `sve`'s and `sve2`'s, and none for the levels of another architecture.
    /// The code around it takes the level's tier, or the most below it that
    /// the processor has ([`Features::tier`]).
    pub(crate) fn new(level: Level) -> Option<Self> {
        let function: Function = match level {
            Level::Neon => adlplug_opl3_pass_neon,
            Level::NeonSha3 => adlplug_opl3_pass_neon_sha3,
            Level::Sve => adlplug_opl3_pass_sve,
            Level::Sve2 => adlplug_opl3_pass_sve2,
            Level::V2
            | Level::Avx2
            | Level::Avx2Vnni
            | Level::Avx512
            | Level::Avx512Vnni
            | Level::Avx512Vbmi
            | Level::Avx512VbmiVnni => return None,
        };
        let features = Features::detect();
        features.has(level).then(|| Self {
            function,
            tier: features.tier(level),
        })
    }

    /// The tier of the code around the pass.
    pub(crate) const fn tier(self) -> Tier {
        self.tier
    }

    /// One sample by the pass, in the form of [`Core::generate_by_hand`]
    /// compiled for the tier.
    pub(crate) fn generate(self, core: &mut Core) -> [i32; 2] {
        match self.tier {
            Tier::Sve2 => {
                // SAFETY: a `Hand` takes the tier sve2 only where the processor
                // has SVE and SVE2, as adlplug-cpu reports them, with the system
                // saving their registers (`Features::tier`); and they are what
                // `generate_sve2` is compiled for.
                unsafe { generate_sve2(core, self) }
            }
            Tier::Sve => {
                // SAFETY: a `Hand` takes the tier sve only where the processor
                // has SVE, as adlplug-cpu reports it, with the system saving its
                // registers (`Features::tier`); and it is what `generate_sve` is
                // compiled for.
                unsafe { generate_sve(core, self) }
            }
            Tier::Baseline | Tier::V3 | Tier::V4 => core.generate_by_hand(self),
        }
    }

    /// A part of a sample (`Core::experiment_part`), in the form compiled for
    /// the tier, for the experiments' measurements alone.
    #[cfg(feature = "experiment")]
    pub(crate) fn part(self, core: &mut Core, part: Part) {
        match self.tier {
            Tier::Sve2 => {
                // SAFETY: as in `generate`, for `part_sve2`.
                unsafe { part_sve2(core, part) }
            }
            Tier::Sve => {
                // SAFETY: as in `generate`, for `part_sve`.
                unsafe { part_sve(core, part) }
            }
            Tier::Baseline | Tier::V3 | Tier::V4 => core.experiment_part(part),
        }
    }

    /// One pass over the lanes: the two samples, output A then output B.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "the call into the assembly belongs in each tier's form of a sample"
    )]
    pub(crate) fn run(self, lanes: &mut Lanes, inputs: &PassInputs) -> [i32; 2] {
        let mut frame = [0; 2];
        // SAFETY: a `Hand` is made only where the processor has the extensions
        // of its level, as adlplug-cpu reports them, with the system saving the
        // registers they use, and they are those its function executes: NEON,
        // with SHA3 for the form named with it, SVE for the form for SVE, and
        // SVE and SVE2 for the form for SVE2. The function follows AAPCS64, as
        // declared (the module's documentation says how), keeps the registers
        // that it has a function keep, does not touch the stack, and keeps no
        // pointer it is given. It reads and writes the lanes within `Lanes`, at
        // the offsets of asm/pass.h, which pass.rs checks against the
        // structure, with loads and stores of 16 bytes and 32 bytes in pairs,
        // which need no alignment; it reads `inputs` within `PassInputs`, and
        // through the pointers that `PassInputs::new`, the one maker of one,
        // sets to the core's tables, the 256 entries of 32 bits of each, at
        // indices of eight bits; and it writes the two values of `frame`.
        unsafe { (self.function)(lanes, inputs, frame.as_mut_ptr()) };
        frame
    }
}

/// A sample by a pass written by hand ([`Core::generate_by_hand`]), compiled for
/// SVE, the extension of adlplug-cpu's `Tier::Sve`.
#[target_feature(enable = "sve")]
fn generate_sve(core: &mut Core, hand: Hand) -> [i32; 2] {
    core.generate_by_hand(hand)
}

/// A sample by a pass written by hand ([`Core::generate_by_hand`]), compiled for
/// SVE2, the extensions of adlplug-cpu's `Tier::Sve2`.
#[target_feature(enable = "sve2")]
fn generate_sve2(core: &mut Core, hand: Hand) -> [i32; 2] {
    core.generate_by_hand(hand)
}

/// `Core::experiment_part` compiled for SVE, for the experiments'
/// measurements.
#[cfg(feature = "experiment")]
#[target_feature(enable = "sve")]
fn part_sve(core: &mut Core, part: Part) {
    core.experiment_part(part);
}

/// `Core::experiment_part` compiled for SVE2, for the experiments'
/// measurements.
#[cfg(feature = "experiment")]
#[target_feature(enable = "sve2")]
fn part_sve2(core: &mut Core, part: Part) {
    core.experiment_part(part);
}

/// Eight additions of `step`, each waiting for the one before, `rounds` times,
/// for the experiments' measure of the clock (`Core::experiment_clock`): of a
/// register, not of a number written in the instruction: a processor may take
/// those as it renames the registers, without waiting, as the performance
/// cores of an Intel Core i7-1360P did, five additions of 1 in a cycle.
#[cfg(feature = "experiment")]
pub(crate) fn chain(rounds: u64, step: u64) -> u64 {
    let mut value = 0_u64;
    let mut count = rounds;
    // SAFETY: the instructions change only the two registers given to them to
    // change, and the flags, which `asm!` takes as changed, and read besides
    // only the one given them to read; they touch no memory and no stack. The
    // loop ends when the count, which is at least one, reaches nought.
    unsafe {
        std::arch::asm!(
            "2:",
            "add {value}, {value}, {step}",
            "add {value}, {value}, {step}",
            "add {value}, {value}, {step}",
            "add {value}, {value}, {step}",
            "add {value}, {value}, {step}",
            "add {value}, {value}, {step}",
            "add {value}, {value}, {step}",
            "add {value}, {value}, {step}",
            "subs {count}, {count}, #1",
            "b.ne 2b",
            value = inout(reg) value,
            count = inout(reg) count,
            step = in(reg) step,
            options(nomem, nostack),
        );
    }
    value + count
}
