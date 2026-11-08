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
//! its lanes, for a pass written by hand as for the portable pass; and what the
//! slots read, the routing and what the passes go by worked out over and over
//! (`Core::rederive`), in the form of the core's tier, with each way of writing
//! derive that works the whole of it out each time. A measurement is the
//! nanoseconds of one, the best of its rounds. The lanes go on from pass to
//! pass as the measurements leave them, so that a core measured makes no
//! samples worth hearing afterwards.
//!
//! The other ways of writing the core's code, which a core takes in place of
//! its own ([`Core::experiment_take`]), so that its samples can be checked
//! against those of a core without them: on x86-64, `derive plain` and `derive
//! split`, `Core::derive` and `Core::derive_split` whatever the CPU's vendor;
//! and everywhere `derive lanes`, derive for the lanes alone whose inputs have
//! changed since derive last read them ([`Core::derive_lanes`]). That one does
//! nothing when derive is asked again without a change, so it is measured by
//! whole samples with writes, as the profile program measures them, and not
//! by the writes' handling over and over.

use std::hint::black_box;
use std::time::Instant;

use super::{Core, DRUM_KEY_BIT, FIRST_LATE_ON_A, FIRST_LATE_ON_B, LANE_13, LANE_14, LANE_16, LANE_17, LANES, Pass};
#[cfg(target_arch = "aarch64")]
use crate::aarch64::chain;
use crate::pass::PassInputs;
#[cfg(target_arch = "x86_64")]
use crate::x86_64::chain;
use crate::ymf262;

/// The other ways of writing the core's own code that a core takes in place of
/// its own: none taken, the default.
#[derive(Debug, Clone, Default)]
pub(super) struct Experiment {
    /// `Core::derive_lanes` in place of `Core::derive`.
    pub(super) derive_lanes: bool,
    /// The next derive works every lane out, as the first one does.
    derive_all: bool,
    /// What derive last read for each lane.
    derived: Box<Inputs>,
}

/// What derive reads for each lane, besides the constants of the lanes: the
/// fields of the core that it reads by the lane.
#[derive(Debug, Clone)]
struct Inputs {
    /// `Core::l20` as derive last read it.
    l20: [u8; LANES],
    /// `Core::l40` as derive last read it.
    l40: [u8; LANES],
    /// `Core::l60` as derive last read it.
    l60: [u8; LANES],
    /// `Core::l80` as derive last read it.
    l80: [u8; LANES],
    /// `Core::le0` as derive last read it.
    le0: [u8; LANES],
    /// `Core::lfnum` as derive last read it.
    lfnum: [u16; LANES],
    /// `Core::lblock` as derive last read it.
    lblock: [u8; LANES],
    /// `Core::lkey` as derive last read it.
    lkey: [u8; LANES],
    /// `Core::lpan` as derive last read it.
    lpan: [u8; LANES],
    /// `Core::lfb` as derive last read it.
    lfb: [u8; LANES],
    /// `Core::lnts` as derive last read it.
    lnts: [u8; LANES],
    /// `Core::ldrum_keys` as derive last read it.
    ldrum_keys: [u8; LANES],
    /// `Core::ldam` as derive last read it.
    ldam: [u8; LANES],
    /// `Core::ldvb` as derive last read it.
    ldvb: [u8; LANES],
    /// `Core::lrhythm_keys` as derive last read it.
    lrhythm_keys: [u8; LANES],
    /// `Core::modulation` as derive last read it.
    modulation: [u8; LANES],
    /// `Core::weight` as derive last read it.
    weight: [u8; LANES],
}

impl Default for Inputs {
    fn default() -> Self {
        Self {
            l20: [0; LANES],
            l40: [0; LANES],
            l60: [0; LANES],
            l80: [0; LANES],
            le0: [0; LANES],
            lfnum: [0; LANES],
            lblock: [0; LANES],
            lkey: [0; LANES],
            lpan: [0; LANES],
            lfb: [0; LANES],
            lnts: [0; LANES],
            ldrum_keys: [0; LANES],
            ldam: [0; LANES],
            ldvb: [0; LANES],
            lrhythm_keys: [0; LANES],
            modulation: [0; LANES],
            weight: [0; LANES],
        }
    }
}

/// The variant of x86-64 that takes `Core::derive` whatever the vendor.
#[cfg(target_arch = "x86_64")]
const DERIVE_PLAIN: &str = "derive plain";
/// The variant of x86-64 that takes `Core::derive_split` whatever the vendor.
#[cfg(target_arch = "x86_64")]
const DERIVE_SPLIT: &str = "derive split";
/// The variant that takes `Core::derive_lanes`.
const DERIVE_LANES: &str = "derive lanes";

impl Core {
    /// What each part of a sample takes, in nanoseconds, the best of `rounds`
    /// rounds of `iterations` each: `pass`, the pass alone; `writes`, the
    /// writes' handling alone; and on x86-64 `writes, derive plain` and
    /// `writes, derive split`, the writes' handling with `Core::derive` and with
    /// `Core::derive_split`.
    #[must_use]
    pub fn experiment_parts(&mut self, iterations: u32, rounds: u32) -> Vec<(String, f64)> {
        #[cfg_attr(target_arch = "aarch64", expect(unused_mut, reason = "x86-64 adds its variants"))]
        let mut parts = vec![
            ("pass".to_owned(), best(rounds, || self.time_pass(iterations))),
            ("writes".to_owned(), best(rounds, || self.time_writes(iterations))),
        ];
        #[cfg(target_arch = "x86_64")]
        {
            let own = self.split_derive;
            for (name, split) in [(DERIVE_PLAIN, false), (DERIVE_SPLIT, true)] {
                self.split_derive = split;
                let ns = best(rounds, || self.time_writes(iterations));
                parts.push((format!("writes, {name}"), ns));
            }
            self.split_derive = own;
        }
        parts
    }

    /// The names of the other ways of writing the core's own code that the
    /// experiments try: on x86-64 the plain and the split derive, and
    /// everywhere derive for the lanes alone whose inputs changed.
    #[must_use]
    pub fn experiment_variants(&self) -> Vec<&'static str> {
        #[cfg(target_arch = "x86_64")]
        let names = vec![DERIVE_PLAIN, DERIVE_SPLIT, DERIVE_LANES];
        #[cfg(not(target_arch = "x86_64"))]
        let names = vec![DERIVE_LANES];
        names
    }

    /// The core takes the variant of that name in place of its own code, and
    /// says whether it has one of that name.
    pub fn experiment_take(&mut self, variant: &str) -> bool {
        match variant {
            #[cfg(target_arch = "x86_64")]
            DERIVE_PLAIN => self.split_derive = false,
            #[cfg(target_arch = "x86_64")]
            DERIVE_SPLIT => self.split_derive = true,
            DERIVE_LANES => {
                self.experiment.derive_lanes = true;
                self.experiment.derive_all = true;
            }
            _ => return false,
        }
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
                self.time_pass(iterations);
            }
            "writes" => {
                self.time_writes(iterations);
            }
            _ => panic!("no part {part}: pass or writes"),
        }
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

    /// derive for the lanes alone whose inputs have changed since derive last
    /// read them, or for every lane the first time: what the loop of
    /// `Core::derive` works out for a lane, for those lanes, the soft panning
    /// for all, and the increments marked when a lane changed. A write changes
    /// what one lane reads, or a channel's lanes, or by the registers of the
    /// whole chip every lane: over the writes of the tests' eight seeds, a
    /// derive found 1.2 of the 48 lanes changed on average, and none in half
    /// of the derives. What they read is checked in a loop of its own, without
    /// short-circuits, which vectorizes.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "it belongs in each tier's form of a sample, as derive does"
    )]
    pub(super) fn derive_lanes(&mut self) {
        let dirty = self.changed_lanes();
        let mut any = false;
        let lanes = &mut *self.lanes;
        let was = &mut *self.experiment.derived;
        for (lane, &drum_key_bit) in DRUM_KEY_BIT.iter().enumerate() {
            if !dirty[lane] {
                continue;
            }
            any = true;
            let s = lanes.lane_slot[lane];
            let r20 = u32::from(self.l20[lane]);
            let r40 = u32::from(self.l40[lane]);
            let r60 = u32::from(self.l60[lane]);
            let r80 = u32::from(self.l80[lane]);
            let fnum = u32::from(self.lfnum[lane]);
            let block = u32::from(self.lblock[lane]);
            let key = self.lkey[lane] != 0;
            let rhythm_keys = self.lrhythm_keys[lane] != 0;
            let dam = self.ldam[lane] != 0;
            let drum_keys = drum_key_bit & self.ldrum_keys[lane];
            let ksr = ymf262::key_scale(fnum, block, self.lnts[lane] != 0, ((r20 >> 4) & 1) != 0);
            let ar = r60 >> 4;
            let dr = r60 & 15;
            let rr = r80 & 15;
            let egt = (r20 >> 5) & 1;
            let by_state = [ar, dr, if egt != 0 { 0 } else { rr }, rr];
            let mut rates = 0;
            for (st, &rate) in by_state.iter().enumerate() {
                rates |= (ymf262::scaled_rate(rate, ksr) | if rate != 0 { 16 } else { 0 }) << (8 * st);
            }
            lanes.rates[lane] = rates;
            lanes.key_scale[lane] = ksr.cast_signed();
            lanes.key_scale_low[lane] = (ksr & 3).cast_signed();
            let sl = r80 >> 4;
            let sustain = sl.cast_signed();
            lanes.sustain[lane] = if sl == 15 { 31 } else { sustain };
            lanes.key_on[lane] = i32::from(key || (rhythm_keys && drum_keys != 0));
            lanes.level_offset[lane] = ymf262::level_offset(fnum, block, r40 >> 6, r40 & 63).cast_signed();
            let depth = if dam { 2 } else { 1 };
            lanes.tremolo_depth[lane] = if (r20 & 0x80) != 0 { depth } else { 0 };
            lanes.fnum_v[lane] = fnum;
            lanes.block_v[lane] = block;
            lanes.multiple_v[lane] = ymf262::MULTIPLE[usize::from(self.l20[lane] & 15)];
            lanes.vibrato[lane] = (r20 >> 6) & 1;
            lanes.vibrato_depth[lane] = u32::from(self.ldvb[lane]);
            let wave = ymf262::wave(u32::from(self.le0[lane]));
            lanes.phase_shift[lane] = wave.phase_shift;
            lanes.turn_bit[lane] = wave.turn_bit;
            lanes.mute_bit[lane] = wave.mute_bit;
            lanes.sign_bit[lane] = wave.sign_bit;
            lanes.square[lane] = wave.square;
            lanes.sawtooth[lane] = wave.sawtooth;
            let fb = i32::from(self.lfb[lane]);
            let by = i32::from(self.modulation[lane]);
            lanes.modulated_by[lane] = if by == 2 && fb == 0 { 0 } else { by };
            let shift = 9 - fb;
            lanes.feedback_shift[lane] = if fb != 0 { shift } else { 0 };
            let w = i32::from(self.weight[lane]);
            let pan = self.lpan[lane];
            let on_a = (pan & 1) != 0;
            let on_b = (pan & 2) != 0;
            lanes.a_now[lane] = if on_a && s < FIRST_LATE_ON_A { w } else { 0 };
            lanes.a_late[lane] = if on_a && s >= FIRST_LATE_ON_A { w } else { 0 };
            lanes.b_now[lane] = if on_b && s < FIRST_LATE_ON_B { w } else { 0 };
            lanes.b_late[lane] = if on_b && s >= FIRST_LATE_ON_B { w } else { 0 };
            was.l20[lane] = self.l20[lane];
            was.l40[lane] = self.l40[lane];
            was.l60[lane] = self.l60[lane];
            was.l80[lane] = self.l80[lane];
            was.le0[lane] = self.le0[lane];
            was.lfnum[lane] = self.lfnum[lane];
            was.lblock[lane] = self.lblock[lane];
            was.lkey[lane] = self.lkey[lane];
            was.lpan[lane] = self.lpan[lane];
            was.lfb[lane] = self.lfb[lane];
            was.lnts[lane] = self.lnts[lane];
            was.ldrum_keys[lane] = self.ldrum_keys[lane];
            was.ldam[lane] = self.ldam[lane];
            was.ldvb[lane] = self.ldvb[lane];
            was.lrhythm_keys[lane] = self.lrhythm_keys[lane];
            was.modulation[lane] = self.modulation[lane];
            was.weight[lane] = self.weight[lane];
        }
        self.derive_soft_pan();
        if any {
            self.lanes.increments_for = u8::MAX;
        }
    }

    /// The lanes whose inputs have changed since derive last read them, or
    /// every lane after the variant is taken.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "it belongs in each tier's form of a sample, as derive does"
    )]
    #[expect(
        clippy::needless_bitwise_bool,
        reason = "the comparisons without short-circuits are a loop that vectorizes"
    )]
    fn changed_lanes(&mut self) -> [bool; LANES] {
        let all = self.experiment.derive_all;
        self.experiment.derive_all = false;
        let mut dirty = [false; LANES];
        let was = &*self.experiment.derived;
        for (lane, dirty) in dirty.iter_mut().enumerate() {
            *dirty = all
                | (self.l20[lane] != was.l20[lane])
                | (self.l40[lane] != was.l40[lane])
                | (self.l60[lane] != was.l60[lane])
                | (self.l80[lane] != was.l80[lane])
                | (self.le0[lane] != was.le0[lane])
                | (self.lfnum[lane] != was.lfnum[lane])
                | (self.lblock[lane] != was.lblock[lane])
                | (self.lkey[lane] != was.lkey[lane])
                | (self.lpan[lane] != was.lpan[lane])
                | (self.lfb[lane] != was.lfb[lane])
                | (self.lnts[lane] != was.lnts[lane])
                | (self.ldrum_keys[lane] != was.ldrum_keys[lane])
                | (self.ldam[lane] != was.ldam[lane])
                | (self.ldvb[lane] != was.ldvb[lane])
                | (self.lrhythm_keys[lane] != was.lrhythm_keys[lane])
                | (self.modulation[lane] != was.modulation[lane])
                | (self.weight[lane] != was.weight[lane]);
        }
        dirty
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
