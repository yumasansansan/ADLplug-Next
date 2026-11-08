// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! The pass written by hand in assembly for AArch64 (`asm/aarch64/`), declared,
//! and the safe function that calls it. It does what [`Core::generate`] does
//! between taking its writes in and moving its counters on: the envelopes and
//! the phases, the rhythm phases, the operators stage by stage, the feedback
//! history and the sums of the two outputs. `pass_neon.S` holds the pass for
//! NEON (Advanced SIMD), which every AArch64 processor has, Apple's and those
//! that Windows and Linux run on alike, and, as this branch's experiment has
//! it, its form that looks the tables up by SVE's gathers.
//!
//! The function follows the procedure call standard of AArch64 (AAPCS64),
//! which macOS, Windows and Linux share for what it takes and keeps: it takes
//! the lanes, the inputs and the frame in x0, x1 and x2, and leaves alone the
//! registers that the standard has a function keep, the low halves of v8 to v15
//! among them, and x18, which Apple's and Windows' conventions keep for the
//! system, and the stack. So the one `extern "C"` declaration is right on all
//! three, and the function needs no unwinding information on any of them.
//!
//! NEON is AArch64's baseline, which the build is compiled for as well: the
//! code around the pass for NEON is the build's. Around the form for SVE it is
//! compiled for SVE as well (`#[target_feature]`), where the processor has SVE,
//! as `x86_64.rs` compiles it for x86-64-v3 and v4. A [`Hand`] is made once the
//! processor is asked for its level and tier ([`Hand::new`]); its existence is
//! what makes the calls sound, and each call is then the one unsafe operation of
//! its block.
//!
//! [`Core::generate`]: crate::Core::generate
#![allow(
    unsafe_code,
    reason = "the calls into assembly and into the form of a sample compiled for SVE, each the one operation of a block whose comment says why it is sound"
)]

use adlplug_cpu::{Features, Level, Tier};

use crate::core::Core;
use crate::pass::{Lanes, PassInputs};

/// A pass written by hand: the lanes, what the pass goes by besides, and the
/// frame of two samples, output A then output B, that it writes.
type Function = unsafe extern "C" fn(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);

unsafe extern "C" {
    /// The pass for NEON (`pass_neon.S`).
    fn adlplug_opl3_pass_neon(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    /// The pass for NEON with SVE's gathers (`pass_neon.S`).
    fn adlplug_opl3_pass_sve(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
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
    /// its extensions, as adlplug-cpu reports them: `neon`'s and `sve`'s, and
    /// none for the levels of another architecture. The code around it takes the
    /// level's tier, or the baseline where the processor has not its extensions
    /// ([`Features::tier`]).
    pub(crate) fn new(level: Level) -> Option<Self> {
        let function: Function = match level {
            Level::Neon => adlplug_opl3_pass_neon,
            Level::Sve => adlplug_opl3_pass_sve,
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

    /// One pass over the lanes: the two samples, output A then output B.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "the call into the assembly belongs in the code of a sample around it"
    )]
    pub(crate) fn run(self, lanes: &mut Lanes, inputs: &PassInputs) -> [i32; 2] {
        let mut frame = [0; 2];
        // SAFETY: a `Hand` is made only where the processor has the extensions
        // of its level, as adlplug-cpu reports them, and they are those its
        // function executes: NEON, and SVE for the form that gathers. The
        // function follows AAPCS64, as declared
        // (the module's documentation says how), keeps the registers that it
        // has a function keep, does not touch the stack, and keeps no pointer
        // it is given. It reads and writes the lanes within `Lanes`, at the
        // offsets of asm/pass.h, which pass.rs checks against the structure,
        // with loads and stores of 16 bytes and 32 bytes in pairs, which need no
        // alignment; it reads `inputs` within `PassInputs`, and through the
        // pointers that `PassInputs::new`, the one maker of one, sets to the
        // core's tables, the 256 entries of 32 bits of each, at indices of eight
        // bits; and it writes the two values of `frame`.
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
