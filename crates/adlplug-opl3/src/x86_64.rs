// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! The passes written by hand in assembly for x86-64 (`asm/x86_64/`), declared,
//! the safe function that calls them, and the forms of a sample by them compiled
//! for the tiers of x86-64. Each pass does what [`Core::generate`] does between
//! taking its writes in and moving its counters on: the envelopes and the
//! phases, the rhythm phases, the operators stage by stage, the feedback history
//! and the sums of the two outputs. `pass_avx2.S` holds the pass for AVX2 in its
//! form for the processors without AVX-VNNI, which look tables up one lane at a
//! time, and in its form for those with it, which gather them and add up the
//! outputs with AVX-VNNI's instructions; `pass_avx512.S` holds the pass for
//! AVX-512, with its foundation alone, adding up with AVX-512 VNNI, looking the
//! tables up by VBMI, and with both.
//!
//! The functions follow the C calling convention of the target, and are
//! declared `extern "C"`, which is that convention in Rust. The files are
//! assembled from the same source for ELF (Linux) and for COFF (Windows), and
//! each function's entry and return are written for the convention of its
//! object format: for ELF it takes the lanes, the inputs and the frame in rdi,
//! rsi and rdx, as System V's convention passes them; for COFF in rcx, rdx and
//! r8, as the Windows x64 convention passes them, and it keeps the low halves of
//! xmm6 to xmm15, which that convention has a function keep, on the stack, with
//! what the unwinder needs to know of it (`.seh_*`). Besides those it uses only
//! registers that both conventions let a function overwrite: rax, rcx, rdx, r8
//! to r11, the vector registers and the mask registers; and it returns with the
//! upper halves of the vector registers zeroed (`vzeroupper`). So the one
//! declaration is right on both, where `extern "sysv64"`, System V's convention
//! on every target, would be wrong on Windows.
//!
//! The rest of a sample by such a pass, [`Core::generate_by_hand`] -- the writes
//! it takes in, what the pass goes by, the counters -- is compiled three times:
//! as the build is, and, by `#[target_feature]`, for x86-64-v3 and x86-64-v4,
//! the tiers of adlplug-cpu, so that a pass for AVX2 runs among code compiled
//! for x86-64-v3 and one for AVX-512 among code compiled for x86-64-v4. That
//! takes vectors of 512 bits, which LLVM, where it tunes for processors of
//! AVX-512, keeps to 256 unless told otherwise: `.cargo/config.toml` turns its
//! preference for 256 bits off (`-prefer-256-bit`) for every x86-64 build, and
//! the feature goes into every function's features, these among them.
//!
//! Whether the processor has a level's extensions, and the extensions of the
//! tier of the code around its pass, is asked once, as a core takes its pass
//! ([`Hand::new`]), through adlplug-cpu's [`Features`], and a [`Hand`] exists
//! only for a level and a tier the processor has: its existence is what makes
//! the calls sound, and each call is then the one unsafe operation of its
//! block. Asking at each call would run CPUID for the processor's maker at every
//! sample, an instruction of a hundred cycles and more, and of thousands under a
//! hypervisor, which traps it, against a pass of some hundred nanoseconds.
//!
//! [`Core::generate`]: crate::Core::generate
#![allow(
    unsafe_code,
    reason = "the calls into assembly and into the forms of a sample compiled for the tiers, each the one operation of a block whose comment says why it is sound"
)]

use adlplug_cpu::{Features, Level, Tier};

use crate::core::Core;
use crate::pass::{Lanes, PassInputs};

/// A pass written by hand: the lanes, what the pass goes by besides, and the
/// frame of two samples, output A then output B, that it writes.
type Function = unsafe extern "C" fn(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);

unsafe extern "C" {
    /// The pass for AVX2, looking tables up one lane at a time
    /// (`pass_avx2.S`).
    fn adlplug_opl3_pass_avx2(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    /// The pass for AVX2 that gathers and adds up with AVX-VNNI
    /// (`pass_avx2.S`).
    fn adlplug_opl3_pass_avx2_vnni(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    /// The pass for AVX-512 F, DQ, BW and VL (`pass_avx512.S`).
    fn adlplug_opl3_pass_avx512(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    /// The pass for AVX-512, adding up with AVX-512 VNNI (`pass_avx512.S`).
    fn adlplug_opl3_pass_avx512_vnni(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    /// The pass for AVX-512, looking the tables up by VBMI (`pass_avx512.S`).
    fn adlplug_opl3_pass_avx512_vbmi(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
    /// The pass for AVX-512 with VBMI and VNNI (`pass_avx512.S`).
    fn adlplug_opl3_pass_avx512_vbmi_vnni(lanes: *mut Lanes, inputs: *const PassInputs, frame: *mut i32);
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
    /// its extensions, as adlplug-cpu reports them: none for the portable
    /// form's level, `v2`, and for the levels of another architecture. The code
    /// around it takes the level's tier, or the most below it that the
    /// processor has ([`Features::tier`]).
    pub(crate) fn new(level: Level) -> Option<Self> {
        let function: Function = match level {
            Level::Avx2 => adlplug_opl3_pass_avx2,
            Level::Avx2Vnni => adlplug_opl3_pass_avx2_vnni,
            Level::Avx512 => adlplug_opl3_pass_avx512,
            Level::Avx512Vnni => adlplug_opl3_pass_avx512_vnni,
            Level::Avx512Vbmi => adlplug_opl3_pass_avx512_vbmi,
            Level::Avx512VbmiVnni => adlplug_opl3_pass_avx512_vbmi_vnni,
            Level::V2 | Level::Neon | Level::NeonSha3 | Level::Sve | Level::Sve2 => return None,
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
            Tier::V4 => {
                // SAFETY: a `Hand` takes x86-64-v4 only where the processor has
                // the extensions of that tier, as adlplug-cpu reports them, with
                // the system saving the registers of AVX and AVX-512
                // (`Features::tier`); and they are those that `generate_v4` is
                // compiled for.
                unsafe { generate_v4(core, self) }
            }
            Tier::V3 => {
                // SAFETY: a `Hand` takes x86-64-v3 only where the processor has
                // the extensions of that tier, as adlplug-cpu reports them, with
                // the system saving the registers of AVX (`Features::tier`); and
                // they are those that `generate_v3` is compiled for.
                unsafe { generate_v3(core, self) }
            }
            Tier::Baseline | Tier::Sve | Tier::Sve2 => core.generate_by_hand(self),
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
        // registers they use, and they are those its function executes: AVX2,
        // with AVX-VNNI for the form that adds up with it; AVX-512 F, DQ, BW
        // and VL with AVX2, and VNNI and VBMI for the forms named with them.
        // The function follows the target's C convention, as declared (the
        // module's documentation says how), returns with the stack as it found
        // it and the upper halves of the vector registers zeroed, and keeps no
        // pointer it is given. It reads and writes the lanes within `Lanes`, at
        // the offsets of asm/pass.h, which pass.rs checks against the
        // structure, with aligned loads and stores of 32 bytes, which its
        // alignment of 64 allows; it reads `inputs` within `PassInputs`, whose
        // alignment of 32 its aligned loads need, and through the pointers that
        // `PassInputs::new`, the one maker of one, sets to the core's tables, as
        // many entries of each as they hold (256 of 32 or 16 bits, or 512
        // bytes), at indices of eight bits; and it writes the two values of
        // `frame`.
        unsafe { (self.function)(lanes, inputs, frame.as_mut_ptr()) };
        frame
    }
}

/// A sample by a pass written by hand ([`Core::generate_by_hand`]), compiled for
/// x86-64-v3: AVX, AVX2, BMI1, BMI2, F16C, FMA, LZCNT and MOVBE, the extensions
/// of adlplug-cpu's `Tier::V3`.
#[target_feature(enable = "avx,avx2,bmi1,bmi2,f16c,fma,lzcnt,movbe")]
fn generate_v3(core: &mut Core, hand: Hand) -> [i32; 2] {
    core.generate_by_hand(hand)
}

/// A sample by a pass written by hand ([`Core::generate_by_hand`]), compiled for
/// x86-64-v4: x86-64-v3's extensions, and AVX-512 F, BW, CD, DQ and VL, those of
/// adlplug-cpu's `Tier::V4`; with vectors of 512 bits (the module's
/// documentation says how).
#[target_feature(enable = "avx,avx2,bmi1,bmi2,f16c,fma,lzcnt,movbe,avx512f,avx512bw,avx512cd,avx512dq,avx512vl")]
fn generate_v4(core: &mut Core, hand: Hand) -> [i32; 2] {
    core.generate_by_hand(hand)
}
