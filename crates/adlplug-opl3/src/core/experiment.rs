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
//! against those of a core without them: `narrow latch`, what the slots read
//! worked out with the conditions of the slot times in bytes
//! ([`Core::relatch_narrow`]); `narrow routing`, the routing with what it
//! takes of each lane's slot in bytes ([`Core::refresh_routing_narrow`]);
//! `narrow`, both; and for a pass written by hand `frame ret`, the pass's form
//! that returns the two samples in the register of a return value in place of
//! writing them to memory (`Hand::returning`), and `all`, that and `narrow`.
//! The first three do nothing when they are asked again with no write being
//! read, so all are measured by whole samples with writes, as the profile
//! program measures them.

use std::hint::black_box;
use std::time::Instant;

use super::{
    ALL_GROUPS, CHAIN_BIT, CHAIN_FIRST_TIME, CHAIN_READ_TIME, CHANNEL_FIRST_TIME, Core, DRUM_PHASE, GROUP_CHAINS,
    GROUP_CHANNEL, GROUP_CHIP, GROUP_FREQUENCY, GROUP_SLOT, LANE_13, LANE_14, LANE_16, LANE_17, LANES, Pass,
    READS_CHIP, READS_FEEDBACK, READS_OWN, RHYTHM_AFTER, RHYTHM_BEFORE, RHYTHM_KEYS_TIME, SLOT_TIME, SLOT_TIMES, latch,
};
#[cfg(target_arch = "aarch64")]
use crate::aarch64::chain;
use crate::pass::PassInputs;
#[cfg(target_arch = "x86_64")]
use crate::x86_64::chain;

/// A table of slot times in bytes, which they all fit.
const fn narrow(table: &[i32; LANES]) -> [i8; LANES] {
    let mut bytes = [0; LANES];
    let mut lane = 0;
    while lane < LANES {
        let time = table[lane];
        assert!(time >= -128 && time <= 127, "a slot time of a byte");
        bytes[lane] = i8::from_le_bytes([time.to_le_bytes()[0]]);
        lane += 1;
    }
    bytes
}

/// `SLOT_TIME` in bytes.
static SLOT_TIME8: [i8; LANES] = narrow(&SLOT_TIME);
/// `CHANNEL_FIRST_TIME` in bytes.
static CHANNEL_FIRST_TIME8: [i8; LANES] = narrow(&CHANNEL_FIRST_TIME);
/// `CHAIN_FIRST_TIME` in bytes.
static CHAIN_FIRST_TIME8: [i8; LANES] = narrow(&CHAIN_FIRST_TIME);
/// `CHAIN_READ_TIME` in bytes.
static CHAIN_READ_TIME8: [i8; LANES] = narrow(&CHAIN_READ_TIME);

/// The least slot time of a byte that reads a write `since` slot times after
/// it with what is read `offset` slot times from the slot's turn: a slot whose
/// time is that or more has read it (`since + time + offset >= 0`), held to a
/// byte, which leaves every slot time on the same side.
fn threshold(since: i32, offset: i32) -> i8 {
    i8::try_from((-(since + offset)).clamp(-128, 127)).expect("a number held to a byte")
}

/// What the routing takes of each lane's slot, in bytes, for `narrow routing`:
/// filled from the lanes' constants as the variant is taken, so that the
/// compiler finds them in memory as it finds those (`Lanes::lane_slot` says
/// why).
#[derive(Debug, Clone)]
struct Constants {
    /// The slot is its channel's first.
    first: [u8; LANES],
    /// The slot is in the second channel of a chain of four.
    second_channel: [u8; LANES],
    /// The lane has a slot.
    heard: [u8; LANES],
    /// The slot is one of the drums', 12 to 17.
    drum: [u8; LANES],
    /// The slot is a drum alone, neither of the bass drum's.
    alone: [u8; LANES],
    /// How loud the drum is heard: 0 for the bass drum's first slot, else 2.
    drum_weight: [u8; LANES],
    /// The slot is the hi-hat's or the tom's, 13 or 14.
    holds: [u8; LANES],
    /// `CHAIN_BIT`.
    chain_bit: [u8; LANES],
    /// `DRUM_PHASE`.
    drum_phase: [u8; LANES],
}

impl Default for Constants {
    fn default() -> Self {
        Self {
            first: [0; LANES],
            second_channel: [0; LANES],
            heard: [0; LANES],
            drum: [0; LANES],
            alone: [0; LANES],
            drum_weight: [0; LANES],
            holds: [0; LANES],
            chain_bit: [0; LANES],
            drum_phase: [0; LANES],
        }
    }
}

/// The other ways of writing the core's own code that a core takes in place of
/// its own, none taken by default, and what they keep.
#[derive(Debug, Clone, Default)]
pub(super) struct Experiment {
    /// `Core::relatch_narrow` in place of the core's relatch.
    pub(super) narrow_latch: bool,
    /// `Core::refresh_routing_narrow` in place of the core's routing.
    pub(super) narrow_routing: bool,
    /// What the narrow routing takes of each lane's slot: in the core, not on
    /// the heap, so that the compiler sees that the routing's stores do not
    /// reach it (from a box the loop would need checks of overlap at run time,
    /// more than LLVM makes, and is not vectorized).
    constants: Constants,
}

/// A part of a sample that [`Core::experiment_part`] works out.
#[derive(Debug, Clone, Copy)]
pub(crate) enum Part {
    /// What the slots read of the groups given, as after writes to them: by
    /// the core's relatch, or with `narrow` by `Core::relatch_narrow`.
    Relatch {
        /// The groups written.
        groups: u8,
        /// By the narrow form.
        narrow: bool,
    },
    /// The routing, as after a write to a group it reads: by the core's, or
    /// with `narrow` by `Core::refresh_routing_narrow`.
    Routing {
        /// By the narrow form.
        narrow: bool,
    },
    /// derive for every lane, as after a reset.
    DeriveAll,
}

/// The variant that takes `Core::relatch_narrow`.
const NARROW_LATCH: &str = "narrow latch";
/// The variant that takes `Core::refresh_routing_narrow`.
const NARROW_ROUTING: &str = "narrow routing";
/// The variant that takes both.
const NARROW: &str = "narrow";
/// The variant that takes the pass's form that returns the two samples.
const FRAME_RET: &str = "frame ret";
/// The variant that takes all of them.
const ALL: &str = "all";

impl Core {
    /// What each part of a sample takes, in nanoseconds, the best of `rounds`
    /// rounds of `iterations` each, by the names that [`Core::experiment_run`]
    /// takes: `pass`, the pass alone; `quiet`, a whole sample with no write to
    /// take; `relatch-all` and `relatch-slot`, what the slots read of every
    /// group and of the slots' registers alone; `routing`; each of those three
    /// by the narrow forms besides (`-narrow`); and `derive-all`, derive for
    /// every lane.
    #[must_use]
    pub fn experiment_parts(&mut self, iterations: u32, rounds: u32) -> Vec<(String, f64)> {
        [
            "pass",
            "quiet",
            "relatch-all",
            "relatch-all-narrow",
            "relatch-slot",
            "relatch-slot-narrow",
            "routing",
            "routing-narrow",
            "derive-all",
        ]
        .into_iter()
        .map(|part| (part.to_owned(), best(rounds, || self.time(part, iterations))))
        .collect()
    }

    /// The names of the other ways of writing the core's own code that the
    /// experiments try: `narrow latch`, `narrow routing` and `narrow`, and for
    /// a pass written by hand `frame ret` and `all`.
    #[must_use]
    pub fn experiment_variants(&self) -> Vec<&'static str> {
        match self.pass {
            Pass::Hand(_) => vec![NARROW_LATCH, NARROW_ROUTING, NARROW, FRAME_RET, ALL],
            Pass::Portable => vec![NARROW_LATCH, NARROW_ROUTING, NARROW],
        }
    }

    /// The core takes the variant of that name in place of its own code, and
    /// says whether it has one of that name.
    pub fn experiment_take(&mut self, variant: &str) -> bool {
        let (latch, routing, ret) = match variant {
            NARROW_LATCH => (true, false, false),
            NARROW_ROUTING => (false, true, false),
            NARROW => (true, true, false),
            FRAME_RET => (false, false, true),
            ALL => (true, true, true),
            _ => return false,
        };
        if ret {
            let Pass::Hand(hand) = self.pass else {
                return false;
            };
            self.pass = Pass::Hand(hand.returning());
        }
        if routing {
            self.fill_constants();
        }
        self.experiment.narrow_latch |= latch;
        self.experiment.narrow_routing |= routing;
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
    /// takes inline: each tier's form calls it (`Hand::part`). The groups
    /// written are nought again after it, as after every write has been read.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "each tier's form of it is compiled whole for that tier, as a sample's is"
    )]
    pub(crate) fn experiment_part(&mut self, part: Part) {
        match part {
            Part::Relatch { groups, narrow } => {
                self.written = groups;
                if narrow {
                    self.relatch_narrow();
                } else {
                    self.relatch();
                }
            }
            Part::Routing { narrow } => {
                self.written = ALL_GROUPS;
                if narrow {
                    self.refresh_routing_narrow();
                } else {
                    self.refresh_routing();
                }
            }
            Part::DeriveAll => {
                self.derive_all = true;
                self.derive();
            }
        }
        self.written = 0;
    }

    /// The core's relatch, by groups as it reads them, with the conditions of
    /// the slot times in bytes: each a comparison of the lane's slot time in a
    /// byte (`SLOT_TIME8` and the others) with the least that has read the
    /// write ([`threshold`]), in place of a sum of 32 bits compared with
    /// nought, so that the loops are vector code of bytes, as the readings
    /// are, at every tier.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "its loops over the lanes belong in each tier's form of a sample, as relatch's do"
    )]
    pub(super) fn relatch_narrow(&mut self) {
        let written = self.written;
        let since = SLOT_TIMES * self.landed_age - self.landed_slot_time;
        let own = threshold(since, READS_OWN);
        if (written & GROUP_SLOT) != 0 {
            for (i, &s) in SLOT_TIME8.iter().enumerate() {
                let reads = s >= own;
                self.l20[i] = latch(reads, self.r20[i], self.l20[i]);
                self.l40[i] = latch(reads, self.r40[i], self.l40[i]);
                self.l60[i] = latch(reads, self.r60[i], self.l60[i]);
                self.l80[i] = latch(reads, self.r80[i], self.l80[i]);
                self.le0[i] = latch(reads, self.re0[i], self.le0[i]);
            }
        }
        if (written & GROUP_FREQUENCY) != 0 {
            for (i, &s) in SLOT_TIME8.iter().enumerate() {
                let reads = s >= own;
                self.lfnum[i] = latch(reads, self.ch_fnum[i], self.lfnum[i]);
                self.lblock[i] = latch(reads, self.ch_block[i], self.lblock[i]);
                self.lkey[i] = latch(reads, self.ch_key[i], self.lkey[i]);
            }
        }
        if (written & GROUP_CHANNEL) != 0 {
            let feedback = threshold(since, READS_FEEDBACK);
            for i in 0..LANES {
                let s = SLOT_TIME8[i];
                let channel_first = CHANNEL_FIRST_TIME8[i] >= own;
                let chain_first = CHAIN_FIRST_TIME8[i] >= own;
                self.lpan[i] = latch(s >= own, self.ch_pan[i], self.lpan[i]);
                self.lcon[i] = latch(channel_first, self.ch_con[i], self.lcon[i]);
                self.lfirst_con[i] = latch(chain_first, self.first_con[i], self.lfirst_con[i]);
                self.lsecond_con[i] = latch(chain_first, self.second_con[i], self.lsecond_con[i]);
                self.lfb[i] = latch(s >= feedback, self.ch_fb[i], self.lfb[i]);
            }
        }
        if (written & GROUP_CHIP) != 0 {
            let chip = threshold(since, READS_CHIP);
            let before = threshold(since, RHYTHM_BEFORE);
            let after = threshold(since, RHYTHM_AFTER);
            let keys = since + RHYTHM_KEYS_TIME >= 0;
            for (i, &s) in SLOT_TIME8.iter().enumerate() {
                let reads = s >= chip;
                self.lnts[i] = latch(reads, self.nts, self.lnts[i]);
                self.ldrum_keys[i] = latch(reads, self.drum_keys, self.ldrum_keys[i]);
                self.ldam[i] = latch(reads, self.dam, self.ldam[i]);
                self.ldvb[i] = latch(reads, self.dvb, self.ldvb[i]);
                self.lrhythm[i] = latch(reads, self.rhythm, self.lrhythm[i]);
                self.lrhythm_keys[i] = latch(keys, self.rhythm, self.lrhythm_keys[i]);
                self.lrhythm_before[i] = latch(s >= before, self.rhythm, self.lrhythm_before[i]);
                self.lrhythm_after[i] = latch(s >= after, self.rhythm, self.lrhythm_after[i]);
            }
        }
        if (written & GROUP_CHAINS) != 0 {
            let reads = threshold(since, 0);
            for (i, &t) in CHAIN_READ_TIME8.iter().enumerate() {
                self.lfour_op[i] = latch(t >= reads, self.four_op, self.lfour_op[i]);
            }
        }
    }

    /// What the narrow routing takes of each lane's slot, from the lanes'
    /// constants.
    fn fill_constants(&mut self) {
        let lanes = &*self.lanes;
        let c = &mut self.experiment.constants;
        for i in 0..LANES {
            let s = lanes.lane_slot[i];
            let drum = (12..18).contains(&s);
            c.first[i] = u8::from(lanes.lane_first[i] != 0);
            c.second_channel[i] = u8::from(lanes.lane_second_channel[i] != 0);
            c.heard[i] = u8::from(s >= 0);
            c.drum[i] = u8::from(drum);
            c.alone[i] = u8::from(drum && s != 12 && s != 15);
            c.drum_weight[i] = if s == 12 { 0 } else { 2 };
            c.holds[i] = u8::from(s == 13 || s == 14);
            c.chain_bit[i] = CHAIN_BIT[i];
            c.drum_phase[i] = DRUM_PHASE[i];
        }
    }

    /// The core's routing, with what it takes of each lane's slot in bytes
    /// (`Constants`) in place of the slot's number in 32 bits, so that the loop
    /// is vector code of bytes, as what it reads and makes are, at every tier:
    /// everything it reads loaded first, and its conditions `&` and `|` in
    /// place of `&&` and `||`, which say the same of booleans. With `&&` LLVM
    /// branched around the loads of the bytes it picked, and the loop was not
    /// vectorized at all, the routing three times as long as the core's.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "its loop over the lanes belongs in each tier's form of a sample, as the routing's does"
    )]
    #[expect(
        clippy::needless_bitwise_bool,
        reason = "conditions without short-circuits are what keep the loop free of branches, and vector code"
    )]
    pub(super) fn refresh_routing_narrow(&mut self) {
        let c = &self.experiment.constants;
        for i in 0..LANES {
            let first = c.first[i] != 0;
            let second_channel = c.second_channel[i] != 0;
            let heard = c.heard[i] != 0;
            let drum = c.drum[i] != 0;
            let alone = c.alone[i] != 0;
            let drum_weight = c.drum_weight[i];
            let holds = c.holds[i] != 0;
            let chain = (self.lfour_op[i] & c.chain_bit[i]) != 0;
            let phase = c.drum_phase[i];
            let con = self.lcon[i] != 0;
            let first_con = self.lfirst_con[i] != 0;
            let second_con = self.lsecond_con[i] != 0;
            let rhythm_early = self.lrhythm_before[i] != 0;
            let rhythm = self.lrhythm[i] != 0;
            let rhythm_late = self.lrhythm_after[i] != 0;
            let mut modulation = if first { 2 } else { u8::from(!con) };
            let mut weight = u8::from(!first | con);
            let two_pairs = !first_con & second_con;
            let c_heard = !two_pairs & con;
            let is_b = chain & !second_channel & !first;
            let is_c = chain & second_channel & first;
            let is_d = chain & second_channel & !first;
            weight = if is_b { u8::from(two_pairs) } else { weight };
            weight = if is_c { u8::from(c_heard) } else { weight };
            modulation = if is_c { u8::from(!two_pairs) } else { modulation };
            modulation = if is_d { u8::from(!c_heard) } else { modulation };
            modulation = if alone & rhythm_early { 0 } else { modulation };
            weight = if drum & rhythm_late { drum_weight } else { weight };
            weight = if heard { weight } else { 0 };
            self.modulation[i] = modulation;
            self.weight[i] = weight;
            self.hold_history[i] = u8::from(rhythm_late & holds);
            self.rhythm_phase[i] = if rhythm { phase } else { 0 };
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
            "relatch-all" => Part::Relatch {
                groups: ALL_GROUPS,
                narrow: false,
            },
            "relatch-all-narrow" => Part::Relatch {
                groups: ALL_GROUPS,
                narrow: true,
            },
            "relatch-slot" => Part::Relatch {
                groups: GROUP_SLOT,
                narrow: false,
            },
            "relatch-slot-narrow" => Part::Relatch {
                groups: GROUP_SLOT,
                narrow: true,
            },
            "routing" => Part::Routing { narrow: false },
            "routing-narrow" => {
                self.fill_constants();
                Part::Routing { narrow: true }
            }
            "derive-all" => Part::DeriveAll,
            _ => panic!(
                "no part {part}: pass, quiet, relatch-all, relatch-slot, routing (each of those three -narrow \
                 besides) or derive-all"
            ),
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
