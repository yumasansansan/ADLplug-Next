// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! ADLplug-Next OPL3, the project's own emulator core of the YMF262 (OPL3):
//! [`Core`] makes what the low-level core drawn from a die shot of the chip
//! (YMF262-LLE) makes as libADLMIDI drives it, sample for sample, and makes it
//! fast, working the 36 slots out together. It is the C++ version's core in
//! Rust, which makes the same samples for every write.
//!
//! A core works its passes over the slots out at a level of the processor
//! (`adlplug-cpu`): on x86-64 from `avx2` on and on AArch64 by a pass written by
//! hand in assembly (`asm/x86_64/`, `asm/aarch64/`, which `build.rs` assembles),
//! and else by the portable pass, plain loops over the slots in Rust. All make
//! the same samples, which the tests check at every level the processor has.
//!
//! `core.rs` holds the core: its queue of writes, its registers and when each
//! slot reads them, and the portable pass; `ymf262.rs` what a slot works out,
//! following YMF262-LLE and Nuked OPL3; `roms.rs` the chip's two ROMs; `pass.rs`
//! the lanes and what a pass goes by, laid out for the assembly at the offsets
//! of `asm/pass.h`; and `x86_64.rs` and `aarch64.rs` the calls into the
//! assembly, the modules with unsafe code, which the others forbid.
#![deny(unsafe_code)]

#[cfg(target_arch = "aarch64")]
mod aarch64;
mod core;
mod pass;
mod roms;
#[cfg(target_arch = "x86_64")]
mod x86_64;
mod ymf262;

pub use crate::core::Core;
