// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! What the parts of a sample take, at every level of the passes this
//! processor has, for rust-speed's experiments (the feature `experiment`).
//!
//! First the processor's clock, as a chain of additions takes it, by which the
//! nanoseconds are cycles as well. Then the other ways of writing each level's
//! pass that the experiments try, each checked to make the samples of a core of
//! its level, as the tests check the levels against the portable pass
//! (`tests/levels.rs`): the random writes of the tests (`tests/common/mod.rs`)
//! for 20000 samples of each of eight seeds, with their soft panning; the run
//! fails at its end if one does not. Then the parts: each core given the writes
//! of seed 1 for 20000 samples, without soft panning and with the soft panning
//! of those tests (the levels marked `soft`), and then measured as it stands,
//! the pass alone, the writes' handling alone, the passes for NEON and SVE
//! without one part each, and the other ways of writing them, the best of five
//! rounds of 20000.
//!
//!     cargo run --release --locked -p adlplug-opl3 --features experiment --example profile
//!
//! With `levels`, the levels the processor has, one to a line; with `variants
//! <level>`, the variants of the level's pass, likewise; with `clock`, the
//! clock alone; with `check`, the check of the variants alone, which fails if
//! one makes other samples. With `run <level> <part> <iterations>
//! [<variant>]`, a core of that level (or `portable`) given the same writes
//! without soft panning, taking the variant where one is named, runs one part,
//! `pass` or `writes`, that many times, for a profiler such as perf.

#[path = "../tests/common/mod.rs"]
mod common;

use std::hint::black_box;

use adlplug_cpu::{Features, Level};
use adlplug_opl3::Core;

const SAMPLES: u32 = 20000;
const ITERATIONS: u32 = 20000;
const ROUNDS: u32 = 5;

/// A core of a level, or by the portable pass for `portable`.
fn core(level: &str) -> Option<Core> {
    if level == "portable" {
        Some(Core::portable())
    } else {
        Core::at(Level::from_name(level)?)
    }
}

/// A core's samples for the writes of a seed, with the soft panning of the
/// tests where `soft`.
fn render(core: &mut Core, seed: u32, soft: bool) -> Vec<[i32; 2]> {
    let writes = common::random_writes(seed, SAMPLES);
    let mut next = writes.iter().peekable();
    let mut out = Vec::with_capacity(usize::try_from(SAMPLES).expect("a count of samples"));
    for n in 0..SAMPLES {
        while let Some(write) = next.next_if(|write| write.at <= n) {
            core.write(write.address, write.value);
        }
        if soft && let Some((address, pan)) = common::soft_pan(n, seed) {
            core.write_pan(address, pan);
        }
        out.push(core.generate());
    }
    out
}

/// A core of a level, or portable, given the writes of seed 1, soft panned or
/// not.
fn warmed(level: &str, soft: bool) -> Option<Core> {
    let mut core = core(level)?;
    black_box(render(&mut core, 1, soft));
    Some(core)
}

/// The samples of the eight seeds by a core of the level that differ from
/// those of a core of the level that takes the variant.
fn differ(level: Level, variant: &str) -> usize {
    (1..=8)
        .map(|seed| {
            let mut own = Core::at(level).expect("a level the processor has");
            let mut other = Core::at(level).expect("a level the processor has");
            assert!(other.experiment_take(variant), "a variant of the level");
            let a = render(&mut own, seed, true);
            let b = render(&mut other, seed, true);
            a.iter().zip(&b).filter(|(a, b)| a != b).count()
        })
        .sum()
}

/// Each variant of each level's pass checked against the level: a line for
/// each, and those that make other samples.
fn check_variants() -> Vec<String> {
    println!();
    println!("{:<14} {:<28} {:>14}", "level", "variant", "samples differ");
    let mut wrong = Vec::new();
    for level in Features::detect().levels() {
        let variants = Core::at(level)
            .expect("a level the processor has")
            .experiment_variants();
        for variant in variants {
            let n = differ(level, variant);
            println!("{:<14} {variant:<28} {n:>14}", level.name());
            if n != 0 {
                wrong.push(format!("{variant} of {level}"));
            }
        }
    }
    wrong
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    match args.as_slice() {
        [run, level, part, iterations, variant @ ..] if run == "run" && variant.len() <= 1 => {
            let mut core = warmed(level, false).expect("a level the processor has, or portable");
            if let Some(variant) = variant.first() {
                assert!(core.experiment_take(variant), "a variant of {level}: {variant}");
            }
            core.experiment_run(part, iterations.parse().expect("a count of iterations"));
            return;
        }
        [levels] if levels == "levels" => {
            for level in Features::detect().levels() {
                println!("{level}");
            }
            return;
        }
        [variants, level] if variants == "variants" => {
            let core = core(level).expect("a level the processor has, or portable");
            for variant in core.experiment_variants() {
                println!("{variant}");
            }
            return;
        }
        [clock] if clock == "clock" => {
            println!(
                "the clock, by a chain of additions: {:.3} GHz",
                Core::experiment_clock()
            );
            return;
        }
        [check] if check == "check" => {
            let wrong = check_variants();
            assert!(
                wrong.is_empty(),
                "variants that make other samples than their levels: {}",
                wrong.join(", ")
            );
            return;
        }
        [] => {}
        _ => {
            panic!(
                "profile, profile levels, profile variants <level>, profile clock, profile check, \
                 or profile run <level> <part> <iterations> [<variant>]"
            )
        }
    }
    let ghz = Core::experiment_clock();
    println!("the clock, by a chain of additions: {ghz:.3} GHz");
    let wrong = check_variants();
    let levels = Features::detect().levels();

    println!();
    println!("{:<18} {:<34} {:>8} {:>8}", "level", "part", "ns", "cycles");
    let names = std::iter::once("portable").chain(levels.into_iter().map(Level::name));
    for level in names {
        for soft in [false, true] {
            let mut core = warmed(level, soft).expect("a level the processor has");
            let name = if soft {
                format!("{level} soft")
            } else {
                level.to_owned()
            };
            for (part, ns) in core.experiment_parts(ITERATIONS, ROUNDS) {
                let cycles = ns * ghz;
                println!("{name:<18} {part:<34} {ns:>8.1} {cycles:>8.0}");
            }
        }
    }
    // The measurements of a variant that makes other samples are of no use,
    // but those of the others are: they are all printed, and then the run
    // fails.
    assert!(
        wrong.is_empty(),
        "variants that make other samples than their levels: {}",
        wrong.join(", ")
    );
}
