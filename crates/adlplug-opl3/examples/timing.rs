// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! The extensions of the processor as adlplug-cpu detects them, and how long
//! the core takes for a sample, at every level of the passes this processor
//! has, with the tier of the code around the pass: the random writes
//! of the tests (`tests/common/mod.rs`), 20000 samples for each of eight seeds,
//! the best of five runs; and the same samples with no writes after the first
//! 2000, which is the pass alone.
//!
//!     taskset -c 2 cargo run --release --locked -p adlplug-opl3 --example timing
//!
//! Pinned to one processor, as above, the numbers vary less from run to run.

#[path = "../tests/common/mod.rs"]
mod common;

use std::hint::black_box;
use std::time::Instant;

use adlplug_cpu::Features;
use adlplug_opl3::Core;

const SAMPLES: u32 = 20000;
const RUNS: usize = 5;

/// The nanoseconds a sample of a run took, the best of [`RUNS`] runs: the
/// writes of the eight seeds, every one or only those of the first `until`
/// samples.
fn per_sample(make: &dyn Fn() -> Core, until: u32) -> f64 {
    let writes: Vec<Vec<common::Write>> = (1..=8).map(|seed| common::random_writes(seed, SAMPLES)).collect();
    let mut best = f64::INFINITY;
    for _ in 0..RUNS {
        let started = Instant::now();
        let mut sum = 0_i64;
        for (seed, writes) in (1..=8).zip(&writes) {
            let mut core = make();
            let mut next = writes.iter().filter(|write| write.at < until).peekable();
            for n in 0..SAMPLES {
                while let Some(write) = next.next_if(|write| write.at <= n) {
                    core.write(write.address, write.value);
                }
                if let Some((address, pan)) = common::soft_pan(n, seed) {
                    core.write_pan(address, pan);
                }
                let [a, b] = core.generate();
                sum += i64::from(a) + i64::from(b);
            }
        }
        black_box(sum);
        let seconds = started.elapsed().as_secs_f64();
        best = best.min(seconds * 1e9 / (8.0 * f64::from(SAMPLES)));
    }
    best
}

fn main() {
    println!("{:?}", Features::detect());
    println!("{:<28} {:>12} {:>14}", "level, tier", "with writes", "pass alone");
    let report = |name: &str, make: &dyn Fn() -> Core| {
        println!(
            "{name:<28} {:>9.1} ns {:>11.1} ns",
            per_sample(make, SAMPLES),
            per_sample(make, 2000)
        );
    };
    report("portable", &Core::portable);
    for level in Features::detect().levels() {
        let make = || Core::at(level).expect("a level the processor has");
        report(&format!("{level}, {}", make().tier()), &make);
    }
}
