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
//! it, its form that looks the tables up by SVE's gathers, with the further
//! forms of the experiments, which the feature `experiment` declares.
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
    reason = "the calls into assembly and into the forms of a sample compiled for SVE and SVE2, and the experiments' chain of additions in assembly, each the one operation of a block whose comment says why it is sound"
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
    /// The pass for SVE that shifts down by SVE's shifts by a vector.
    fn adlplug_opl3_pass_sve_shifts(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON without its lookups, for the experiments' measurements alone.
    fn adlplug_opl3_pass_neon_no_lookups(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON without its envelopes, for the experiments' measurements alone.
    fn adlplug_opl3_pass_neon_no_envelopes(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON without its phases, for the experiments' measurements alone.
    fn adlplug_opl3_pass_neon_no_phases(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON without its operators, for the experiments' measurements alone.
    fn adlplug_opl3_pass_neon_no_operators(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON without its sums, for the experiments' measurements alone.
    fn adlplug_opl3_pass_neon_no_sums(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON without its history, for the experiments' measurements alone.
    fn adlplug_opl3_pass_neon_no_history(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE without its lookups, for the experiments' measurements alone.
    fn adlplug_opl3_pass_sve_no_lookups(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE without its envelopes, for the experiments' measurements alone.
    fn adlplug_opl3_pass_sve_no_envelopes(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE without its phases, for the experiments' measurements alone.
    fn adlplug_opl3_pass_sve_no_phases(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE without its operators, for the experiments' measurements alone.
    fn adlplug_opl3_pass_sve_no_operators(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE without its sums, for the experiments' measurements alone.
    fn adlplug_opl3_pass_sve_no_sums(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE without its history, for the experiments' measurements alone.
    fn adlplug_opl3_pass_sve_no_history(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON with its operators tidied: masks by cmtst, the shape by bit, lookups by the low bytes, an experiment.
    fn adlplug_opl3_pass_neon_tidy(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON with the operators' constants held in registers, an experiment.
    fn adlplug_opl3_pass_neon_consts(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON with each envelope's rate by tbl, an experiment.
    fn adlplug_opl3_pass_neon_rates(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON with the phases moved on by the lanes' increments, an experiment.
    fn adlplug_opl3_pass_neon_increments(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON with the lookups' indices taken out through memory, an experiment.
    fn adlplug_opl3_pass_neon_extract(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON with the looked-up values put in by loads into lanes, an experiment.
    fn adlplug_opl3_pass_neon_insert(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON with both of those, an experiment.
    fn adlplug_opl3_pass_neon_extract_insert(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON tidied, with the constants, the rates by tbl and the increments, an experiment.
    fn adlplug_opl3_pass_neon_all(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON with all of those and the indices through memory, an experiment.
    fn adlplug_opl3_pass_neon_all_extract(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON with all of those and the loads into lanes, an experiment.
    fn adlplug_opl3_pass_neon_all_insert(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE with its operators tidied, an experiment.
    fn adlplug_opl3_pass_sve_tidy(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE with the operators' constants held in registers, an experiment.
    fn adlplug_opl3_pass_sve_consts(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE with each envelope's rate by tbl, an experiment.
    fn adlplug_opl3_pass_sve_rates(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE with the phases moved on by the lanes' increments, an experiment.
    fn adlplug_opl3_pass_sve_increments(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE tidied, with the constants, the rates by tbl and the increments, an experiment.
    fn adlplug_opl3_pass_sve_all(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE's shifts with all of those, an experiment.
    fn adlplug_opl3_pass_sve_shifts_all(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON with the envelopes' masks put together by bit and bif, an experiment.
    fn adlplug_opl3_pass_neon_logic(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON with all of those and the masks put together, an experiment.
    fn adlplug_opl3_pass_neon_all_logic(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON with all of those, the masks put together and the indices through memory, an experiment.
    fn adlplug_opl3_pass_neon_all_logic_extract(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON tidied, with SHA3's BCAX, an experiment.
    fn adlplug_opl3_pass_neon_tidy_sha3(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for NEON with all of those, the masks, the indices through memory and SHA3's BCAX, an experiment.
    fn adlplug_opl3_pass_neon_all_logic_extract_sha3(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE with the envelopes' masks put together, an experiment.
    fn adlplug_opl3_pass_sve_logic(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE with all of those and the masks put together, an experiment.
    fn adlplug_opl3_pass_sve_all_logic(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE tidied, with SVE2's BCAX and BSL1N, an experiment.
    fn adlplug_opl3_pass_sve_tidy_sve2(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE with all of those, the masks put together and SVE2's instructions, an experiment.
    fn adlplug_opl3_pass_sve_all_logic_sve2(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    #[cfg(feature = "experiment")]
    /// The form for SVE's shifts with all of those and SVE2's instructions, an experiment.
    fn adlplug_opl3_pass_sve_shifts_all_logic_sve2(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
}

/// The forms of the passes for NEON and SVE without one of their parts each,
/// by name, for the experiments' measurements alone: the difference of their
/// time from the whole pass's is the part's. They need what their whole forms
/// need, and read and write only what those do.
#[cfg(feature = "experiment")]
const NEON_ABLATIONS: [(&str, Function); 6] = [
    ("no lookups", adlplug_opl3_pass_neon_no_lookups),
    ("no envelopes", adlplug_opl3_pass_neon_no_envelopes),
    ("no phases", adlplug_opl3_pass_neon_no_phases),
    ("no operators", adlplug_opl3_pass_neon_no_operators),
    ("no sums", adlplug_opl3_pass_neon_no_sums),
    ("no history", adlplug_opl3_pass_neon_no_history),
];
/// The same for SVE's form.
#[cfg(feature = "experiment")]
const SVE_ABLATIONS: [(&str, Function); 6] = [
    ("no lookups", adlplug_opl3_pass_sve_no_lookups),
    ("no envelopes", adlplug_opl3_pass_sve_no_envelopes),
    ("no phases", adlplug_opl3_pass_sve_no_phases),
    ("no operators", adlplug_opl3_pass_sve_no_operators),
    ("no sums", adlplug_opl3_pass_sve_no_sums),
    ("no history", adlplug_opl3_pass_sve_no_history),
];

/// The other ways of writing the passes for NEON and SVE that this branch
/// tries, by name, for the experiments alone (`pass_neon.S` says what each
/// does): each makes the samples of its whole form, which the experiments check
/// before they measure it, and needs what that form needs.
#[cfg(feature = "experiment")]
const NEON_VARIANTS: [(&str, Function); 13] = [
    ("tidy", adlplug_opl3_pass_neon_tidy),
    ("consts", adlplug_opl3_pass_neon_consts),
    ("rates", adlplug_opl3_pass_neon_rates),
    ("increments", adlplug_opl3_pass_neon_increments),
    ("extract", adlplug_opl3_pass_neon_extract),
    ("insert", adlplug_opl3_pass_neon_insert),
    ("extract, insert", adlplug_opl3_pass_neon_extract_insert),
    ("logic", adlplug_opl3_pass_neon_logic),
    ("all", adlplug_opl3_pass_neon_all),
    ("all, extract", adlplug_opl3_pass_neon_all_extract),
    ("all, insert", adlplug_opl3_pass_neon_all_insert),
    ("all, logic", adlplug_opl3_pass_neon_all_logic),
    ("all, logic, extract", adlplug_opl3_pass_neon_all_logic_extract),
];
/// The same with SHA3's BCAX, for the processors with SHA3.
#[cfg(feature = "experiment")]
const NEON_SHA3_VARIANTS: [(&str, Function); 2] = [
    ("tidy, sha3", adlplug_opl3_pass_neon_tidy_sha3),
    (
        "all, logic, extract, sha3",
        adlplug_opl3_pass_neon_all_logic_extract_sha3,
    ),
];
/// The same for SVE's form.
#[cfg(feature = "experiment")]
const SVE_VARIANTS: [(&str, Function); 7] = [
    ("tidy", adlplug_opl3_pass_sve_tidy),
    ("consts", adlplug_opl3_pass_sve_consts),
    ("rates", adlplug_opl3_pass_sve_rates),
    ("increments", adlplug_opl3_pass_sve_increments),
    ("logic", adlplug_opl3_pass_sve_logic),
    ("all", adlplug_opl3_pass_sve_all),
    ("all, logic", adlplug_opl3_pass_sve_all_logic),
];
/// The same with SVE2's instructions, for the levels of SVE2.
#[cfg(feature = "experiment")]
const SVE2_VARIANTS: [(&str, Function); 2] = [
    ("tidy, sve2", adlplug_opl3_pass_sve_tidy_sve2),
    ("all, logic, sve2", adlplug_opl3_pass_sve_all_logic_sve2),
];
/// The same for the form with SVE's shifts, at its level of SVE2.
#[cfg(feature = "experiment")]
const SVE_SHIFTS_VARIANTS: [(&str, Function); 2] = [
    ("all", adlplug_opl3_pass_sve_shifts_all),
    ("all, logic, sve2", adlplug_opl3_pass_sve_shifts_all_logic_sve2),
];

/// The pass written by hand for a level that the processor has, and the tier of
/// the code around it, one the processor has: made by [`Hand::new`] alone,
/// which asks the processor, so that one that exists runs.
#[derive(Debug, Clone, Copy)]
pub(crate) struct Hand {
    /// The level's pass.
    function: Function,
    /// The tier of the code around it.
    tier: Tier,
    /// The level, by which the experiments find the forms without a part.
    #[cfg(feature = "experiment")]
    level: Level,
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
            Level::Sve | Level::Sve2 => adlplug_opl3_pass_sve,
            Level::Sve2Shifts => adlplug_opl3_pass_sve_shifts,
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
            #[cfg(feature = "experiment")]
            level,
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
            Tier::Sve2 => {
                // SAFETY: a `Hand` takes the tier sve2 only where the processor
                // has SVE and SVE2, as adlplug-cpu reports them, with the system
                // saving their registers (`Features::tier`); and they are what
                // `generate_sve2` is compiled for.
                unsafe { generate_sve2(core, self) }
            }
            Tier::Baseline | Tier::V3 | Tier::V4 => core.generate_by_hand(self),
        }
    }

    /// The forms of the pass without one of its parts each, for the
    /// experiments' measurements alone, with the same tier: for the passes for
    /// NEON and for SVE (`sve` and `sve2`), and none for the others.
    #[cfg(feature = "experiment")]
    pub(crate) fn ablations(self) -> Vec<(&'static str, Self)> {
        let forms: &[(&str, Function)] = match self.level {
            Level::Neon => &NEON_ABLATIONS,
            Level::Sve | Level::Sve2 => &SVE_ABLATIONS,
            _ => &[],
        };
        forms
            .iter()
            .map(|&(name, function)| (name, Self { function, ..self }))
            .collect()
    }

    /// The other ways of writing the pass that the experiments try, with the
    /// same tier: for the passes for NEON, with SHA3's BCAX where the processor
    /// has SHA3, for SVE (`sve`, and `sve2` with SVE2's instructions as well)
    /// and for SVE's shifts, and none for the others.
    #[cfg(feature = "experiment")]
    pub(crate) fn variants(self) -> Vec<(&'static str, Self)> {
        let sha3 = Features::detect().sha3;
        let forms: Vec<&[(&str, Function)]> = match self.level {
            Level::Neon if sha3 => vec![&NEON_VARIANTS, &NEON_SHA3_VARIANTS],
            Level::Neon => vec![&NEON_VARIANTS],
            Level::Sve => vec![&SVE_VARIANTS],
            Level::Sve2 => vec![&SVE_VARIANTS, &SVE2_VARIANTS],
            Level::Sve2Shifts => vec![&SVE_SHIFTS_VARIANTS],
            _ => Vec::new(),
        };
        forms
            .into_iter()
            .flatten()
            .map(|&(name, function)| (name, Self { function, ..self }))
            .collect()
    }

    /// What the slots read, the routing and what the passes go by worked out
    /// once (`Core::rederive`), in the form compiled for the tier, for the
    /// experiments' measurements alone.
    #[cfg(feature = "experiment")]
    pub(crate) fn rederive(self, core: &mut Core) {
        match self.tier {
            Tier::Sve => {
                // SAFETY: as in `generate`, for `rederive_sve`.
                unsafe { rederive_sve(core) }
            }
            Tier::Sve2 => {
                // SAFETY: as in `generate`, for `rederive_sve2`.
                unsafe { rederive_sve2(core) }
            }
            Tier::Baseline | Tier::V3 | Tier::V4 => core.rederive(),
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
        // function executes: NEON, and SVE for the forms that gather or shift
        // by it (`ablations` and `variants` give a Hand only forms that need
        // what the whole form needs, or with SHA3's instructions where the
        // processor has SHA3, or with SVE2's at the levels of SVE2). The
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

/// A sample by a pass written by hand ([`Core::generate_by_hand`]), compiled for
/// SVE2, the extensions of adlplug-cpu's `Tier::Sve2`.
#[target_feature(enable = "sve2")]
fn generate_sve2(core: &mut Core, hand: Hand) -> [i32; 2] {
    core.generate_by_hand(hand)
}

/// `Core::rederive` compiled for SVE, for the experiments' measurements.
#[cfg(feature = "experiment")]
#[target_feature(enable = "sve")]
fn rederive_sve(core: &mut Core) {
    core.rederive();
}

/// `Core::rederive` compiled for SVE2, for the experiments' measurements.
#[cfg(feature = "experiment")]
#[target_feature(enable = "sve2")]
fn rederive_sve2(core: &mut Core) {
    core.rederive();
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
