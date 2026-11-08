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
//! First the CPU's clock, as a chain of additions takes it, by which the
//! nanoseconds are cycles as well. Then the other ways of writing the core's own
//! code that the experiments try (`Core::experiment_variants`), each checked to
//! make the samples of a core without it, as the tests check the levels against
//! the portable pass (`tests/levels.rs`): the random writes of the tests
//! (`tests/common/mod.rs`) for 20000 samples of each of eight seeds, with their
//! soft panning; the run fails at its end if one does not. Then the parts: each
//! core given the writes of seed 1 for 20000 samples, without soft panning and
//! with the soft panning of those tests (the levels marked `soft`), and then
//! measured as it stands, the best of five rounds of 20000 of each part
//! (`Core::experiment_parts`): the pass alone, a whole sample with no write to
//! take, and the writes' handling and its parts, what the slots read, the
//! routing and derive for every lane, by each way of writing derive on
//! x86-64. Last, whole samples with their writes, by the core's own code and
//! by each variant, as the timing program measures them: the writes of the
//! eight seeds and their soft panning, the best of five runs.
//!
//!     cargo run --release --locked -p adlplug-opl3 --features experiment --example profile
//!
//! With `levels`, the levels the processor has, one to a line; with `variants
//! <level>`, the variants of the level's pass, likewise; with `clock`, the
//! clock alone; with `check`, the check of the variants alone, which fails if
//! one makes other samples. With `run <level> <part> <iterations>
//! [<variant>]`, a core of that level (or `portable`) given the same writes
//! without soft panning, taking the variant where one is named, runs one part
//! of those, by its name in the table of the parts, that many times, for a
//! profiler such as perf; and with the part `samples`, cores of the level make
//! that many whole samples with their writes, as the last table measures them.

#[path = "../tests/common/mod.rs"]
mod common;

use std::hint::black_box;

use adlplug_cpu::{Features, Level};
use adlplug_opl3::Core;

const SAMPLES: u32 = 20000;
const ITERATIONS: u32 = 20000;
const ROUNDS: u32 = 5;
const RUNS: u32 = 5;

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

/// The samples of the eight seeds by a core of the level (or portable) that
/// differ from those of a core of the level that takes the variant.
fn differ(level: &str, variant: &str) -> usize {
    (1..=8)
        .map(|seed| {
            let mut own = core(level).expect("a level the processor has, or portable");
            let mut other = core(level).expect("a level the processor has, or portable");
            assert!(other.experiment_take(variant), "a variant of {level}: {variant}");
            let a = render(&mut own, seed, true);
            let b = render(&mut other, seed, true);
            a.iter().zip(&b).filter(|(a, b)| a != b).count()
        })
        .sum()
}

/// The writes of the eight seeds, for [`samples`].
fn seeds_writes() -> Vec<Vec<common::Write>> {
    (1..=8).map(|seed| common::random_writes(seed, SAMPLES)).collect()
}

/// The sum of `count` samples with their writes by cores of the level (or
/// portable) that take the variant, or their own code, as the timing program
/// makes them (`examples/timing.rs`): the writes of the eight seeds in turn,
/// `writes`, with their soft panning, a new core for each seed's 20000 samples.
fn samples(level: &str, variant: Option<&str>, writes: &[Vec<common::Write>], count: u32) -> i64 {
    let mut sum = 0_i64;
    let mut left = count;
    for (seed, writes) in (1..=8).zip(writes).cycle() {
        if left == 0 {
            break;
        }
        let mut core = core(level).expect("a level the processor has, or portable");
        if let Some(variant) = variant {
            assert!(core.experiment_take(variant), "a variant of {level}: {variant}");
        }
        let mut next = writes.iter().peekable();
        let these = SAMPLES.min(left);
        for n in 0..these {
            while let Some(write) = next.next_if(|write| write.at <= n) {
                core.write(write.address, write.value);
            }
            if let Some((address, pan)) = common::soft_pan(n, seed) {
                core.write_pan(address, pan);
            }
            let [a, b] = core.generate();
            sum += i64::from(a) + i64::from(b);
        }
        left -= these;
    }
    sum
}

/// The nanoseconds of a sample with its writes by a core of the level (or
/// portable) that takes the variant, or its own code ([`samples`]): those of
/// the eight seeds, whose writes are made before the clock starts, the best of
/// [`RUNS`] runs.
fn per_sample(level: &str, variant: Option<&str>, writes: &[Vec<common::Write>]) -> f64 {
    let mut best = f64::INFINITY;
    for _ in 0..RUNS {
        let started = std::time::Instant::now();
        black_box(samples(level, variant, writes, 8 * SAMPLES));
        best = best.min(started.elapsed().as_secs_f64() * 1e9 / (8.0 * f64::from(SAMPLES)));
    }
    best
}

/// Each variant of the portable core's and of each level's checked against the
/// core without it: a line for each, and those that make other samples.
fn check_variants() -> Vec<String> {
    println!();
    println!("{:<14} {:<28} {:>14}", "level", "variant", "samples differ");
    let mut wrong = Vec::new();
    let names = std::iter::once("portable").chain(Features::detect().levels().into_iter().map(Level::name));
    for level in names {
        let variants = core(level)
            .expect("a level the processor has, or portable")
            .experiment_variants();
        for variant in variants {
            let n = differ(level, variant);
            println!("{level:<14} {variant:<28} {n:>14}");
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
        [run, level, part, count, variant @ ..] if run == "run" && part == "samples" && variant.len() <= 1 => {
            let writes = seeds_writes();
            let count = count.parse().expect("a count of samples");
            black_box(samples(level, variant.first().map(String::as_str), &writes, count));
            return;
        }
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
    println!();
    println!(
        "{:<18} {:<34} {:>8} {:>8}",
        "level", "samples with writes", "ns", "cycles"
    );
    let writes = seeds_writes();
    let names = std::iter::once("portable").chain(Features::detect().levels().into_iter().map(Level::name));
    for level in names {
        let variants = core(level)
            .expect("a level the processor has, or portable")
            .experiment_variants();
        for variant in std::iter::once(None).chain(variants.into_iter().map(Some)) {
            let ns = per_sample(level, variant, &writes);
            let cycles = ns * ghz;
            let name = variant.unwrap_or("its own code");
            println!("{level:<18} {name:<34} {ns:>8.1} {cycles:>8.0}");
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
