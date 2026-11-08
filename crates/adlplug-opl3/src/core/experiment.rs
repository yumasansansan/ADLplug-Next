// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! What the parts of a sample take, and other ways of writing the core's own
//! code, for rust-speed's experiments: built with the feature `experiment`
//! alone, and no part of the core's interface beside it. A core is measured as
//! it stands, after the writes its caller gave it: its pass over and over on
//! its lanes, for a pass written by hand as for the portable pass; whole
//! samples with no write to take, over and over, as [`Core::generate`] makes
//! them; and the parts of the writes' handling over and over (what the slots
//! read, the routing, derive), in the form of the core's tier
//! ([`Core::experiment_part`]). A measurement is the nanoseconds of one, the
//! best of its rounds. The lanes go on from pass to pass as the measurements
//! leave them, so that a core measured makes no samples worth hearing
//! afterwards.
//!
//! The other ways of writing the core's code, which a core takes in place of
//! its own ([`Core::experiment_take`]), so that its samples can be checked
//! against those of a core without them: on x86-64, `derive split` and
//! `derive lanes`, `Core::derive_split` and derive for the changed lanes at
//! every tier, which the core takes at x86-64-v4 and below it; and everywhere
//! `latch groups`, what the slots read worked out again for the groups of
//! registers alone that a write has changed ([`Core::relatch_groups`]), and
//! the routing only when one of those it reads has; and `derive groups`, that
//! with derive comparing what it reads of those groups alone
//! ([`Core::changed_lanes_by_groups`]) and working the soft panning out only
//! when a lane has changed, by the changed lanes at every tier. These do
//! nothing when they are asked again with no write being read, so they are
//! measured by whole samples with writes, as the profile program measures
//! them.

use std::hint::black_box;
use std::time::Instant;

use super::{
    CHAIN_FIRST_TIME, CHAIN_READ_TIME, CHANNEL_FIRST_TIME, Core, LANE_13, LANE_14, LANE_16, LANE_17, LANES, Pass,
    READS_CHIP, READS_FEEDBACK, READS_OWN, RHYTHM_AFTER, RHYTHM_BEFORE, RHYTHM_KEYS_TIME, SLOT_TIME, SLOT_TIMES, latch,
};
#[cfg(target_arch = "aarch64")]
use crate::aarch64::chain;
use crate::pass::PassInputs;
#[cfg(target_arch = "x86_64")]
use crate::x86_64::chain;

/// The registers of the slots, from 0x20, 0x40, 0x60, 0x80 and 0xe0, as the
/// slots read them (`l20` to `le0`).
const SLOTS: u8 = 1;
/// The channels' frequencies, blocks and keys, from 0xa0 and 0xb0 (`lfnum`,
/// `lblock`, `lkey`).
const FREQUENCIES: u8 = 2;
/// The channels' outputs, connections and feedbacks, from 0xc0 (`lpan`, `lcon`,
/// `lfirst_con`, `lsecond_con`, `lfb`).
const CHANNELS: u8 = 4;
/// The registers of the whole chip that the slots read, from 0x08 and 0xbd
/// (`lnts`, `ldrum_keys`, `ldam`, `ldvb`, `lrhythm` and the readings of the
/// rhythm before and after).
const CHIP: u8 = 8;
/// The register of the chains of four, 0x104 (`lfour_op`).
const CHAINS: u8 = 16;
/// Every group.
const ALL: u8 = SLOTS | FREQUENCIES | CHANNELS | CHIP | CHAINS;
/// The groups the routing reads: the connections, the chains and the rhythm.
const ROUTING: u8 = CHANNELS | CHIP | CHAINS;

/// The groups of registers that a write to an address may change, as
/// `Core::apply` decodes it, more of them rather than fewer: the registers
/// below 0x20, those of the whole chip, all of them, as the chip then writes
/// the last data written from 0x20 on once more, and a write to the register
/// of the chains spreads every channel.
const fn groups_of(address: u16) -> u8 {
    match address.to_le_bytes()[0] {
        0x00..=0x1f => ALL,
        0xbd => CHIP,
        0xa0..=0xbf => FREQUENCIES,
        0xc0..=0xcf => CHANNELS,
        0xd0..=0xdf => 0,
        _ => SLOTS,
    }
}

/// The other ways of writing the core's own code that a core takes in place of
/// its own, none taken by default, and what they keep.
#[derive(Debug, Clone, Default)]
pub(super) struct Experiment {
    /// `Core::relatch_groups` in place of `Core::relatch`, and the routing
    /// worked out only when a group it reads has changed.
    pub(super) latch_groups: bool,
    /// derive for the changed lanes, comparing what it reads of the groups
    /// written alone (`Core::changed_lanes_by_groups`), and the soft panning
    /// worked out only when a lane has changed.
    pub(super) derive_groups: bool,
    /// The groups of the registers written since the slots last read every
    /// write, whatever the variant, so that it holds them when it is taken.
    written: u8,
}

impl Experiment {
    /// Every group to be read afresh, as a reset has them.
    pub(super) const fn latch_all(&mut self) {
        self.written = ALL;
    }

    /// The slots have read every write.
    pub(super) const fn all_read(&mut self) {
        self.written = 0;
    }

    /// A write to an address reaches the chip's registers.
    pub(super) const fn written(&mut self, address: u16) {
        self.written |= groups_of(address);
    }

    /// Whether the variant `latch groups` leaves the routing as it is: no
    /// group that it reads has been written since the slots last read every
    /// write.
    pub(super) const fn routing_unchanged(&self) -> bool {
        self.latch_groups && (self.written & ROUTING) == 0
    }
}

/// A part of a sample that [`Core::experiment_part`] works out.
#[derive(Debug, Clone, Copy)]
pub(crate) enum Part {
    /// What the slots read, the routing and derive, as the writes' handling
    /// takes them: asked again and again, derive for the changed lanes finds
    /// none changed, where the split derive works every lane out.
    Writes,
    /// What the slots read.
    Relatch,
    /// The routing.
    Routing,
    /// derive for every lane, as after a reset.
    DeriveAll,
}

/// The variant of x86-64 that takes `Core::derive_split` at every tier.
#[cfg(target_arch = "x86_64")]
const DERIVE_SPLIT: &str = "derive split";
/// The variant of x86-64 that takes derive for the changed lanes at every tier.
#[cfg(target_arch = "x86_64")]
const DERIVE_LANES: &str = "derive lanes";
/// The variant that takes `Core::relatch_groups`.
const LATCH_GROUPS: &str = "latch groups";
/// The variant that takes `Core::relatch_groups` and
/// `Core::changed_lanes_by_groups`, with derive for the changed lanes at every
/// tier.
const DERIVE_GROUPS: &str = "derive groups";

impl Core {
    /// What each part of a sample takes, in nanoseconds, the best of `rounds`
    /// rounds of `iterations` each, by the names that [`Core::experiment_run`]
    /// takes: `pass`, the pass alone; `quiet`, a whole sample with no write to
    /// take; `writes`, the writes' handling with no lane changed for derive;
    /// `relatch`, `routing` and `derive-all`, its parts; and on x86-64
    /// `derive-all-split` and `derive-all-lanes`, derive for every lane by
    /// `Core::derive_split` and lane by lane.
    #[must_use]
    pub fn experiment_parts(&mut self, iterations: u32, rounds: u32) -> Vec<(String, f64)> {
        #[cfg_attr(
            target_arch = "aarch64",
            expect(unused_mut, reason = "x86-64 adds its forms of derive")
        )]
        let mut parts: Vec<(String, f64)> = ["pass", "quiet", "writes", "relatch", "routing", "derive-all"]
            .into_iter()
            .map(|part| (part.to_owned(), best(rounds, || self.time(part, iterations))))
            .collect();
        #[cfg(target_arch = "x86_64")]
        {
            let own = self.split_derive;
            for (name, split) in [("derive-all-split", true), ("derive-all-lanes", false)] {
                self.split_derive = split;
                let ns = best(rounds, || self.time("derive-all", iterations));
                parts.push((name.to_owned(), ns));
            }
            self.split_derive = own;
        }
        parts
    }

    /// The names of the other ways of writing the core's own code that the
    /// experiments try: on x86-64 the split derive and derive for the changed
    /// lanes at every tier, and everywhere `latch groups` and `derive groups`.
    #[must_use]
    pub fn experiment_variants(&self) -> Vec<&'static str> {
        #[cfg(target_arch = "x86_64")]
        let names = vec![DERIVE_SPLIT, DERIVE_LANES, LATCH_GROUPS, DERIVE_GROUPS];
        #[cfg(not(target_arch = "x86_64"))]
        let names = vec![LATCH_GROUPS, DERIVE_GROUPS];
        names
    }

    /// The core takes the variant of that name in place of its own code, and
    /// says whether it has one of that name.
    pub fn experiment_take(&mut self, variant: &str) -> bool {
        match variant {
            #[cfg(target_arch = "x86_64")]
            DERIVE_SPLIT => self.split_derive = true,
            #[cfg(target_arch = "x86_64")]
            DERIVE_LANES => self.split_derive = false,
            LATCH_GROUPS => self.experiment.latch_groups = true,
            DERIVE_GROUPS => {
                self.experiment.latch_groups = true;
                self.experiment.derive_groups = true;
                #[cfg(target_arch = "x86_64")]
                {
                    self.split_derive = false;
                }
            }
            _ => return false,
        }
        true
    }

    /// One part over and over, `iterations` times, for a profiler such as
    /// perf, by the names that [`Core::experiment_parts`] gives.
    ///
    /// # Panics
    ///
    /// For any other part.
    pub fn experiment_run(&mut self, part: &str, iterations: u32) {
        self.time(part, iterations);
    }

    /// The CPU's clock as it runs now, in gigahertz: the additions a nanosecond
    /// of a chain of them, each waiting for the one before, which take a cycle
    /// each on the CPUs of x86-64 and AArch64 of the runners; the best of five
    /// rounds of 200 million. With it the nanoseconds of the measurements are
    /// cycles.
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

    /// A part of a sample, as [`Part`] names them, in the form the core's code
    /// takes inline: each tier's form calls it (`Hand::part`).
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "each tier's form of it is compiled whole for that tier, as a sample's is"
    )]
    pub(crate) fn experiment_part(&mut self, part: Part) {
        match part {
            Part::Writes => {
                self.relatch();
                self.refresh_routing();
                self.derive();
            }
            Part::Relatch => self.relatch(),
            Part::Routing => self.refresh_routing(),
            Part::DeriveAll => {
                self.derive_all = true;
                self.derive();
            }
        }
    }

    /// What the slots read, worked out again for the groups of registers alone
    /// that a write has changed since the slots last read every write
    /// (`Experiment::written`), with what `Core::relatch` takes for each: the
    /// registers of a group no write has changed are what the slots read of
    /// them already. Most writes change one group, the registers of a slot or
    /// a channel's frequency, block and key, of three to eight lanes' reading.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "its loops over the lanes belong in each tier's form of a sample, as relatch's do"
    )]
    pub(super) fn relatch_groups(&mut self) {
        let written = self.experiment.written;
        let since = SLOT_TIMES * self.landed_age - self.landed_slot_time;
        if (written & SLOTS) != 0 {
            for (i, &s) in SLOT_TIME.iter().enumerate() {
                let own = since + s + READS_OWN >= 0;
                self.l20[i] = latch(own, self.r20[i], self.l20[i]);
                self.l40[i] = latch(own, self.r40[i], self.l40[i]);
                self.l60[i] = latch(own, self.r60[i], self.l60[i]);
                self.l80[i] = latch(own, self.r80[i], self.l80[i]);
                self.le0[i] = latch(own, self.re0[i], self.le0[i]);
            }
        }
        if (written & FREQUENCIES) != 0 {
            for (i, &s) in SLOT_TIME.iter().enumerate() {
                let own = since + s + READS_OWN >= 0;
                self.lfnum[i] = latch(own, self.ch_fnum[i], self.lfnum[i]);
                self.lblock[i] = latch(own, self.ch_block[i], self.lblock[i]);
                self.lkey[i] = latch(own, self.ch_key[i], self.lkey[i]);
            }
        }
        if (written & CHANNELS) != 0 {
            for i in 0..LANES {
                let s = SLOT_TIME[i];
                let own = since + s + READS_OWN >= 0;
                let channel_first = since + CHANNEL_FIRST_TIME[i] + READS_OWN >= 0;
                let chain_first = since + CHAIN_FIRST_TIME[i] + READS_OWN >= 0;
                self.lpan[i] = latch(own, self.ch_pan[i], self.lpan[i]);
                self.lcon[i] = latch(channel_first, self.ch_con[i], self.lcon[i]);
                self.lfirst_con[i] = latch(chain_first, self.first_con[i], self.lfirst_con[i]);
                self.lsecond_con[i] = latch(chain_first, self.second_con[i], self.lsecond_con[i]);
                self.lfb[i] = latch(since + s + READS_FEEDBACK >= 0, self.ch_fb[i], self.lfb[i]);
            }
        }
        if (written & CHIP) != 0 {
            for (i, &s) in SLOT_TIME.iter().enumerate() {
                let chip = since + s + READS_CHIP >= 0;
                self.lnts[i] = latch(chip, self.nts, self.lnts[i]);
                self.ldrum_keys[i] = latch(chip, self.drum_keys, self.ldrum_keys[i]);
                self.ldam[i] = latch(chip, self.dam, self.ldam[i]);
                self.ldvb[i] = latch(chip, self.dvb, self.ldvb[i]);
                self.lrhythm[i] = latch(chip, self.rhythm, self.lrhythm[i]);
                self.lrhythm_keys[i] = latch(since + RHYTHM_KEYS_TIME >= 0, self.rhythm, self.lrhythm_keys[i]);
                self.lrhythm_before[i] = latch(since + s + RHYTHM_BEFORE >= 0, self.rhythm, self.lrhythm_before[i]);
                self.lrhythm_after[i] = latch(since + s + RHYTHM_AFTER >= 0, self.rhythm, self.lrhythm_after[i]);
            }
        }
        if (written & CHAINS) != 0 {
            for (i, &t) in CHAIN_READ_TIME.iter().enumerate() {
                self.lfour_op[i] = latch(since + t >= 0, self.four_op, self.lfour_op[i]);
            }
        }
    }

    /// The lanes whose inputs may have changed since derive last read them, or
    /// every lane after a reset, by the groups of registers written since the
    /// slots last read every write (`Experiment::written`): what derive reads
    /// of a group no write has changed is what it read before, and the routing
    /// changes what it makes (`modulation`, `weight`) only when it has been
    /// worked out again, after a write to a group it reads. Each group's
    /// comparisons are a loop of their own, without short-circuits.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "it belongs in each tier's form of a sample, as derive does"
    )]
    #[expect(
        clippy::needless_bitwise_bool,
        reason = "the comparisons without short-circuits are loops that vectorize"
    )]
    pub(super) fn changed_lanes_by_groups(&mut self) -> [bool; LANES] {
        let all = self.derive_all;
        self.derive_all = false;
        if all {
            return [true; LANES];
        }
        let written = self.experiment.written;
        let was = &self.derived;
        let mut changed = [false; LANES];
        if (written & SLOTS) != 0 {
            for (lane, changed) in changed.iter_mut().enumerate() {
                *changed |= (self.l20[lane] != was.l20[lane])
                    | (self.l40[lane] != was.l40[lane])
                    | (self.l60[lane] != was.l60[lane])
                    | (self.l80[lane] != was.l80[lane])
                    | (self.le0[lane] != was.le0[lane]);
            }
        }
        if (written & FREQUENCIES) != 0 {
            for (lane, changed) in changed.iter_mut().enumerate() {
                *changed |= (self.lfnum[lane] != was.lfnum[lane])
                    | (self.lblock[lane] != was.lblock[lane])
                    | (self.lkey[lane] != was.lkey[lane]);
            }
        }
        if (written & CHANNELS) != 0 {
            for (lane, changed) in changed.iter_mut().enumerate() {
                *changed |= (self.lpan[lane] != was.lpan[lane]) | (self.lfb[lane] != was.lfb[lane]);
            }
        }
        if (written & CHIP) != 0 {
            for (lane, changed) in changed.iter_mut().enumerate() {
                *changed |= (self.lnts[lane] != was.lnts[lane])
                    | (self.ldrum_keys[lane] != was.ldrum_keys[lane])
                    | (self.ldam[lane] != was.ldam[lane])
                    | (self.ldvb[lane] != was.ldvb[lane])
                    | (self.lrhythm_keys[lane] != was.lrhythm_keys[lane]);
            }
        }
        if (written & ROUTING) != 0 {
            for (lane, changed) in changed.iter_mut().enumerate() {
                *changed |= (self.modulation[lane] != was.modulation[lane]) | (self.weight[lane] != was.weight[lane]);
            }
        }
        changed
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

    /// The nanoseconds of a part, run `iterations` times: `pass` by the core's
    /// own pass, written by hand or portable; `quiet` by `Core::generate`, once
    /// the writes have all been taken; the others in the form of the core's
    /// tier. The lanes go out of sight after each, so that none is left out as
    /// the same as the one before.
    ///
    /// # Panics
    ///
    /// For a part of another name.
    fn time(&mut self, part: &str, iterations: u32) -> f64 {
        let part = match part {
            "pass" => return self.time_pass(iterations),
            "quiet" => return self.time_quiet(iterations),
            "writes" => Part::Writes,
            "relatch" => Part::Relatch,
            "routing" => Part::Routing,
            "derive-all" => Part::DeriveAll,
            _ => panic!("no part {part}: pass, quiet, writes, relatch, routing or derive-all"),
        };
        let start = Instant::now();
        match self.pass {
            Pass::Hand(hand) => {
                for _ in 0..iterations {
                    hand.part(self, part);
                    black_box(&mut self.lanes);
                }
            }
            Pass::Portable => {
                for _ in 0..iterations {
                    self.experiment_part(part);
                    black_box(&mut self.lanes);
                }
            }
        }
        start.elapsed().as_secs_f64() * 1e9 / f64::from(iterations)
    }

    /// The nanoseconds of a pass, run `iterations` times: by the core's own
    /// pass, written by hand or portable.
    fn time_pass(&mut self, iterations: u32) -> f64 {
        let start = Instant::now();
        let mut sum = 0_i64;
        match self.pass {
            Pass::Hand(hand) => {
                let inputs = self.inputs();
                for _ in 0..iterations {
                    let [a, b] = hand.run(&mut self.lanes, &inputs);
                    sum += i64::from(a) + i64::from(b);
                }
            }
            Pass::Portable => {
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

    /// The nanoseconds of a whole sample with no write to take, run
    /// `iterations` times by [`Core::generate`]: the samples first go on
    /// until the queue is empty and the slots have read every write.
    fn time_quiet(&mut self, iterations: u32) -> f64 {
        while self.queue_count > 0 || self.landing_next.due || self.landed_age >= 0 {
            black_box(self.generate());
        }
        let start = Instant::now();
        let mut sum = 0_i64;
        for _ in 0..iterations {
            let [a, b] = self.generate();
            sum += i64::from(a) + i64::from(b);
        }
        black_box(sum);
        start.elapsed().as_secs_f64() * 1e9 / f64::from(iterations)
    }
}

/// The least of `rounds` measurements.
fn best(rounds: u32, mut measure: impl FnMut() -> f64) -> f64 {
    (0..rounds).map(|_| measure()).fold(f64::INFINITY, f64::min)
}
