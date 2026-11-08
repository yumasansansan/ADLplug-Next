// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! What the parts of a sample take, for rust-speed's experiments: built with
//! the feature `experiment` alone, and no part of the core's interface beside
//! it. A core is measured as it stands, after the writes its caller gave it:
//! its pass over and over on its lanes, for a pass written by hand as for the
//! portable pass; what the slots read, the routing and what the passes go by
//! worked out over and over (`Core::rederive`), in the form of the core's tier;
//! for the passes for NEON and SVE, their forms without one of their parts
//! each, whose time the difference from the whole pass's tells; and the other
//! ways of writing the pass that the experiments try, which a core can take in
//! place of its own ([`Core::experiment_take`]) so that its samples can be
//! checked against those of a core of its level. A measurement is the
//! nanoseconds of one, the best of its rounds. The lanes go on from pass to
//! pass as the measurements leave them, so that a core measured makes no samples
//! worth hearing afterwards.

use std::hint::black_box;
use std::time::Instant;

use super::{Core, LANE_13, LANE_14, LANE_16, LANE_17, Pass};
#[cfg(target_arch = "aarch64")]
use crate::aarch64::{Hand, chain};
use crate::pass::PassInputs;
#[cfg(target_arch = "x86_64")]
use crate::x86_64::{Hand, chain};

impl Core {
    /// What each part of a sample takes, in nanoseconds, the best of `rounds`
    /// rounds of `iterations` each: `pass`, the pass alone; `writes`, the
    /// writes' handling alone; for the passes for NEON and SVE, `pass, no
    /// <part>`, the pass without that part; and `pass, <variant>`, each other
    /// way of writing the pass ([`Core::experiment_variants`]).
    #[must_use]
    pub fn experiment_parts(&mut self, iterations: u32, rounds: u32) -> Vec<(String, f64)> {
        let mut parts = vec![
            ("pass".to_owned(), best(rounds, || self.time_pass(None, iterations))),
            ("writes".to_owned(), best(rounds, || self.time_writes(iterations))),
        ];
        let forms = self.ablations().into_iter().chain(self.variants());
        for (name, hand) in forms.collect::<Vec<_>>() {
            let ns = best(rounds, || self.time_pass(Some(hand), iterations));
            parts.push((format!("pass, {name}"), ns));
        }
        parts
    }

    /// The names of the other ways of writing the core's pass that the
    /// experiments try: for the passes for NEON and SVE, and none for the
    /// others.
    #[must_use]
    pub fn experiment_variants(&self) -> Vec<&'static str> {
        self.variants().into_iter().map(|(name, _)| name).collect()
    }

    /// The core takes the variant of its pass of that name in place of its
    /// pass, with the same tier, and says whether it has one of that name.
    pub fn experiment_take(&mut self, variant: &str) -> bool {
        let Some((_, hand)) = self.variants().into_iter().find(|&(name, _)| name == variant) else {
            return false;
        };
        self.pass = Pass::Hand(hand);
        true
    }

    /// One part over and over, `iterations` times, for a profiler such as
    /// perf: `pass` or `writes`, as [`Core::experiment_parts`] names them.
    ///
    /// # Panics
    ///
    /// For any other part.
    pub fn experiment_run(&mut self, part: &str, iterations: u32) {
        match part {
            "pass" => {
                self.time_pass(None, iterations);
            }
            "writes" => {
                self.time_writes(iterations);
            }
            _ => panic!("no part {part}: pass or writes"),
        }
    }

    /// The processor's clock as it runs now, in gigahertz: the additions a
    /// nanosecond of a chain of them, each waiting for the one before, which
    /// take a cycle each on the processors of x86-64 and AArch64 of the
    /// runners; the best of five rounds of 200 million. With it the
    /// nanoseconds of the measurements are cycles.
    #[must_use]
    pub fn experiment_clock() -> f64 {
        const ADDITIONS: u32 = 200_000_000;
        let ns = best(5, || {
            let start = Instant::now();
            black_box(chain(black_box(u64::from(ADDITIONS / 8)), black_box(1)));
            start.elapsed().as_secs_f64() * 1e9
        });
        f64::from(ADDITIONS) / ns
    }

    /// The forms of the core's pass without one of their parts each.
    fn ablations(&self) -> Vec<(&'static str, Hand)> {
        match self.pass {
            Pass::Hand(hand) => hand.ablations(),
            Pass::Portable => Vec::new(),
        }
    }

    /// The other ways of writing the core's pass.
    fn variants(&self) -> Vec<(&'static str, Hand)> {
        match self.pass {
            Pass::Hand(hand) => hand.variants(),
            Pass::Portable => Vec::new(),
        }
    }

    /// What the pass written by hand goes by besides the lanes, as the core's
    /// pass takes it for the pass the counters stand at.
    fn inputs(&self) -> PassInputs {
        let rhythm = [
            self.rhythm_phase[LANE_13],
            self.rhythm_phase[LANE_16],
            self.rhythm_phase[LANE_17],
        ];
        let hold = [self.hold_history[LANE_13], self.hold_history[LANE_14]];
        PassInputs::new(&self.lanes.counters, self.lanes.noise, rhythm, hold, self.soft_pan)
    }

    /// The nanoseconds of a pass, run `iterations` times: by the form given, or
    /// by the core's own pass, written by hand or portable.
    fn time_pass(&mut self, form: Option<Hand>, iterations: u32) -> f64 {
        let start = Instant::now();
        let mut sum = 0_i64;
        match (self.pass, form) {
            (_, Some(hand)) | (Pass::Hand(hand), None) => {
                let inputs = self.inputs();
                for _ in 0..iterations {
                    let [a, b] = hand.run(&mut self.lanes, &inputs);
                    sum += i64::from(a) + i64::from(b);
                }
            }
            (Pass::Portable, None) => {
                for _ in 0..iterations {
                    self.envelope_and_phase();
                    self.rhythm_phases();
                    self.operators();
                    self.store_history();
                    let [a, b] = self.accumulate();
                    sum += i64::from(a) + i64::from(b);
                }
            }
        }
        black_box(sum);
        start.elapsed().as_secs_f64() * 1e9 / f64::from(iterations)
    }

    /// The nanoseconds of the writes' handling, run `iterations` times in the
    /// form of the core's tier. The lanes go out of sight after each, so that
    /// none is left out as the same as the one before.
    fn time_writes(&mut self, iterations: u32) -> f64 {
        let start = Instant::now();
        match self.pass {
            Pass::Hand(hand) => {
                for _ in 0..iterations {
                    hand.rederive(self);
                    black_box(&mut self.lanes);
                }
            }
            Pass::Portable => {
                for _ in 0..iterations {
                    self.rederive();
                    black_box(&mut self.lanes);
                }
            }
        }
        start.elapsed().as_secs_f64() * 1e9 / f64::from(iterations)
    }
}

/// The least of `rounds` measurements.
fn best(rounds: u32, mut measure: impl FnMut() -> f64) -> f64 {
    (0..rounds).map(|_| measure()).fold(f64::INFINITY, f64::min)
}
