// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! ADLplug-Next's emulator core of the YMF262 (OPL3), [`Core`]. It makes what
//! the low-level core drawn from a die shot of the chip (YMF262-LLE) makes as
//! libADLMIDI drives it, sample for sample, and makes it fast: the 36 slots are
//! worked out together, in loops written for the compiler to vectorize, or,
//! where the processor has the instructions, in a pass written by hand for them.
//!
//! libADLMIDI feeds that core a register write every 2 2/9 samples, and each
//! write takes effect in the middle of a pass over the slots, for the slots that
//! read their registers after it; the core here keeps the same queue, and
//! follows the chip in when each slot reads each register. What a slot works
//! out in a pass follows the code of YMF262-LLE and Nuked OPL3 (`ymf262.rs`).
//!
//! Time is counted in passes over the 36 slots, one a sample, and in slot times
//! within a pass. The chip works a slot out over several slot times; slot time
//! s is the one at which the chip works out slot s's phase and envelope.
//!
//! The slots are kept in lanes, in the order of the stages of modulation: a
//! slot in a stage takes its modulation, when it has one from another slot,
//! from the slot at the same position of the stage before. The stages of 12,
//! 12, 6 and 6 slots take 16, 16, 8 and 8 lanes, the lanes past a stage's slots
//! never heard, so that every loop over the lanes runs in whole vectors of 8 or
//! 16 lanes of 32 bits, and a stage reads the lanes of the stage before but
//! never its own. A loop over the lanes reads what a lane goes by before it
//! chooses anything, and chooses between names or constants (`ymf262.rs`), so
//! that nothing is read behind a condition: such a read (the right of an `&&`
//! or one arm of an `if`) is a masked load in a vectorized loop, and for bytes,
//! which no x86 instruction loads masked, a branch for each lane.
//!
//! Rust has no way to ask for a loop to be vectorized whatever the compiler's
//! cost model makes of it, as the C++ version's pragma asked: the loops that
//! look a table up for each lane, the operators' (`stage`) and `derive`'s, are
//! left scalar where the model counts the lookups dear, the operators' for
//! x86-64-v2, v3 and AArch64 and `derive`'s for x86-64-v2 and AArch64, as
//! LLVM 23 compiles them. Every other loop over the lanes is vectorized there.
//!
//! Around a pass written by hand, the code of a sample (`Core::generate_by_hand`)
//! is compiled for the tier of the pass's level as well (`x86_64.rs`): the
//! loops over the lanes that it runs and what makes the pass's inputs are
//! inlined into it (`#[inline(always)]`), so that they take the tier's
//! instructions, its vectors of 256 or 512 bits among them; the rest, the queue
//! and the registers as a write reaches them, is scalar code, which the compiler
//! inlines or calls as it sees fit.
#![forbid(unsafe_code)]

use std::cmp::Ordering;
use std::f64::consts::PI;
use std::fmt;

use adlplug_cpu::{Choice, Features, Level, Tier};

#[cfg(target_arch = "aarch64")]
use crate::aarch64::Hand;
#[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
use crate::pass::PassInputs;
use crate::pass::{LANES, Lanes};
#[cfg(target_arch = "x86_64")]
use crate::x86_64::Hand;
use crate::ymf262::{self, Counters, RhythmBits};

/// The slots of the chip.
const SLOTS: usize = 36;
/// The channels of the chip.
const CHANNELS: usize = 18;
/// The slot times of a pass, one for each slot.
const SLOT_TIMES: i32 = 36;

/// Where each stage begins, in lanes.
const STAGE_BEGIN: [usize; 4] = [0, 16, 32, 40];
/// How many lanes each stage takes.
const STAGE_WIDTH: [usize; 4] = [16, 16, 8, 8];

/// The writes the queue holds at the most.
const QUEUE_SIZE: usize = 2048;

/// The level whose pass is the portable one: x86-64's baseline, `v2`; none on
/// AArch64, whose baseline has a pass written by hand, and on an architecture
/// without levels.
const PORTABLE_LEVEL: Option<Level> = if cfg!(target_arch = "x86_64") {
    Some(Level::V2)
} else {
    None
};

/// The slot of a lane that has none.
const PAD: i32 = -1;

/// Lane by lane, the slot, or [`PAD`] for a lane that has none: the first
/// operator of every chain, the second, the third of a four-operator chain, and
/// its fourth.
#[rustfmt::skip]
static SLOT_OF: [i32; LANES] = [
    0, 1, 2, 18, 19, 20, 12, 13, 14, 30, 31, 32, PAD, PAD, PAD, PAD,
    3, 4, 5, 21, 22, 23, 15, 16, 17, 33, 34, 35, PAD, PAD, PAD, PAD,
    6, 7, 8, 24, 25, 26, PAD, PAD,
    9, 10, 11, 27, 28, 29, PAD, PAD,
];

/// The lane of the hi-hat's slot, 13.
const LANE_13: usize = 7;
/// The lane of the tom's slot, 14.
const LANE_14: usize = 8;
/// The lane of the snare's slot, 16.
const LANE_16: usize = 23;
/// The lane of the top cymbal's slot, 17.
const LANE_17: usize = 24;
/// The lane of slot 31, which shares its feedback store with slot 13.
const LANE_31: usize = 10;
/// The lane of slot 32, which shares its feedback store with slot 14.
const LANE_32: usize = 11;
/// The lanes of the slots that share a feedback store: 13 and 31, 14 and 32.
const SHARED: [(usize, usize); 2] = [(LANE_13, LANE_31), (LANE_14, LANE_32)];

/// The slot a register address below 0x20 names, from the low five bits, -1
/// for none.
#[rustfmt::skip]
static SLOT_OF_OFFSET: [i32; 32] = [
    0, 1, 2, 3, 4, 5, -1, -1, 6, 7, 8, 9, 10, 11, -1, -1,
    12, 13, 14, 15, 16, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
];

// Where a slot is: its bank, and its row of the bank's 18. Row r is in channel
// (r / 6) * 3 + r % 3 of the bank, as its first operator for r % 6 below 3 and
// as its second from 3. The rows below 12 make the bank's three chains of four,
// chain r % 3 of channels r % 3 and r % 3 + 3, as A, B, C and D by r / 3.

/// The bank of a slot.
const fn bank_of(slot: i32) -> i32 {
    slot / 18
}

/// The row of a slot in its bank.
const fn row_of(slot: i32) -> i32 {
    slot % 18
}

/// The channel of a slot.
const fn channel_of(slot: i32) -> i32 {
    bank_of(slot) * 9 + row_of(slot) / 6 * 3 + row_of(slot) % 3
}

/// Whether a slot is its channel's first operator.
const fn is_first_operator(slot: i32) -> bool {
    row_of(slot) % 6 < 3
}

/// The chain of four operators a slot can be in, -1 for none.
const fn chain_of(slot: i32) -> i32 {
    if row_of(slot) < 12 {
        bank_of(slot) * 3 + row_of(slot) % 3
    } else {
        -1
    }
}

/// A slot, lane, channel or chain, which is below 256, as an index, in a
/// constant: from its low byte, as a constant function cannot yet call std's
/// conversions.
const fn index(number: i32) -> usize {
    assert!(number >= 0 && number < 256, "an index of a byte");
    let mut bytes = [0; size_of::<usize>()];
    bytes[0] = number.to_le_bytes()[0];
    usize::from_le_bytes(bytes)
}

/// The lane of each slot.
static LANE_OF_SLOT: [usize; SLOTS] = {
    let mut t = [0; SLOTS];
    let mut lane = 0;
    while lane < LANES {
        if SLOT_OF[lane] != PAD {
            t[index(SLOT_OF[lane])] = lane;
        }
        lane += 1;
    }
    t
};

/// The lanes of each channel's two slots, the first operator's first.
static LANES_OF_CHANNEL: [[usize; 2]; CHANNELS] = {
    let mut t = [[0; 2]; CHANNELS];
    let mut slot = 0;
    while slot < 36 {
        t[index(channel_of(slot))][if is_first_operator(slot) { 0 } else { 1 }] = LANE_OF_SLOT[index(slot)];
        slot += 1;
    }
    t
};

/// The lanes of each chain's four slots: A, B, C and D.
static LANES_OF_CHAIN: [[usize; 4]; 6] = {
    let mut t = [[0; 4]; 6];
    let mut slot = 0;
    while slot < 36 {
        if chain_of(slot) >= 0 {
            t[index(chain_of(slot))][index(row_of(slot) / 3)] = LANE_OF_SLOT[index(slot)];
        }
        slot += 1;
    }
    t
};

/// The chip sums a sample of output A from slot 33 of one pass to slot 32 of
/// the next, and a sample of output B from slot 15 to slot 14: the slots from
/// these on are heard in the sample after the one of their pass.
const FIRST_LATE_ON_A: i32 = 33;
/// The first slot heard late on output B.
const FIRST_LATE_ON_B: i32 = 15;

/// libADLMIDI's wrapper of the low-level core writes the address of a
/// register, 8 slot times later its data, and 72 slot times after that the next
/// address. Its sample begins 3 1/4 slot times before the end of the pass
/// [`Core::generate`] works out, and a data write reaches the chip's registers
/// 1 3/4 slot times after it is made: a data write at slot time k of the sample
/// takes effect at slot time k + 34 of that pass, or at k - 2 of the next.
const ADDRESS_TO_DATA: i32 = 8;
/// The slot times from a data write to the next address.
const DATA_TO_ADDRESS: i32 = 72;

/// When a slot reads what it goes by, in slot times from its own slot time: its
/// own registers and its channel's the slot time before, its channel's
/// feedback four before, and the registers of the whole chip at its own.
const READS_OWN: i32 = -1;
/// When a slot reads its channel's feedback.
const READS_FEEDBACK: i32 = -4;
/// When a slot reads the registers of the whole chip.
const READS_CHIP: i32 = 0;

/// The rhythm flag is read at slot time 12 for all six drums' keys, the slot
/// time before a slot for stopping its feedback or modulation, at its own for
/// its phase, and two after for whether and how loud it is heard (and whether
/// its value is stored for feedback).
const RHYTHM_KEYS_TIME: i32 = 12;
/// When a slot reads the rhythm flag for stopping its feedback or modulation.
const RHYTHM_BEFORE: i32 = -1;
/// When a slot reads the rhythm flag for whether and how loud it is heard.
const RHYTHM_AFTER: i32 = 2;

/// The slot time of a slot's chain's first slot, the slot's own out of a
/// chain.
const fn chain_first_time(slot: i32) -> i32 {
    if chain_of(slot) < 0 {
        return slot;
    }
    bank_of(slot) * 18 + row_of(slot) % 3
}

/// Each chain of four operators has its flag read once a pass, at slot time 5,
/// 6 or 7 for the first bank's chains and 23, 24 or 25 for the second's; a
/// slot out of a chain at its own.
const fn chain_time(slot: i32) -> i32 {
    if chain_of(slot) < 0 {
        return slot;
    }
    bank_of(slot) * 18 + 5 + row_of(slot) % 3
}

/// A table by lane of what an expression makes of the lane's slot, 0 for a
/// pad, in a constant.
macro_rules! by_lane {
    ($type:ty, |$s:ident| $value:expr) => {{
        let mut table: [$type; LANES] = [0; LANES];
        let mut lane = 0;
        while lane < LANES {
            let $s = SLOT_OF[lane];
            if $s != PAD {
                table[lane] = $value;
            }
            lane += 1;
        }
        table
    }};
}

// By lane: the slot time; that of the channel's first slot, and that of the
// chain's first slot (the slot's own out of a chain); the slot time the chain's
// flag is read at; the chain's bit in the register of the chains (0 out of
// one); whether the slot is its channel's first, and whether it is in the
// second channel of a chain; the drum whose phase it takes in rhythm mode; and
// the bit of 0xbd that also keys it in rhythm mode (slots 12 to 17: the bass
// drum's two, the hi-hat, the tom, the snare and the top cymbal).

/// The slot time.
static SLOT_TIME: [i32; LANES] = by_lane!(i32, |s| s);
/// The slot time of the channel's first slot.
static CHANNEL_FIRST_TIME: [i32; LANES] = by_lane!(i32, |s| if is_first_operator(s) { s } else { s - 3 });
/// The slot time of the chain's first slot.
static CHAIN_FIRST_TIME: [i32; LANES] = by_lane!(i32, |s| chain_first_time(s));
/// The slot time the chain's flag is read at.
static CHAIN_READ_TIME: [i32; LANES] = by_lane!(i32, |s| chain_time(s));
/// The chain's bit in the register of the chains.
static CHAIN_BIT: [u8; LANES] = by_lane!(u8, |s| if chain_of(s) < 0 { 0 } else { 1 << chain_of(s) });
/// Whether the slot is its channel's first.
static FIRST_OPERATOR: [i32; LANES] = by_lane!(i32, |s| if is_first_operator(s) { 1 } else { 0 });
/// Whether the slot is in the second channel of a chain.
static CHAIN_SECOND_CHANNEL: [i32; LANES] = by_lane!(i32, |s| if chain_of(s) >= 0 && row_of(s) >= 6 { 1 } else { 0 });
/// The drum whose phase the slot takes in rhythm mode.
static DRUM_PHASE: [u8; LANES] = by_lane!(u8, |s| drum_phase(s));
/// The bit of 0xbd that also keys the slot in rhythm mode.
static DRUM_KEY_BIT: [u8; LANES] = by_lane!(u8, |s| drum_key_bit(s));

/// The drum whose phase a slot takes in rhythm mode: 1 the hi-hat, 2 the
/// snare, 3 the top cymbal, 0 none.
const fn drum_phase(slot: i32) -> u8 {
    match slot {
        13 => 1,
        16 => 2,
        17 => 3,
        _ => 0,
    }
}

/// The bit of 0xbd that also keys a slot in rhythm mode, 0 for none.
const fn drum_key_bit(slot: i32) -> u8 {
    const KEY_BIT: [u8; 6] = [0x10, 0x01, 0x04, 0x10, 0x08, 0x02];
    if slot >= 12 && slot < 18 {
        KEY_BIT[index(slot - 12)]
    } else {
        0
    }
}

/// The wrapper's reset leaves the chip with its counters where 100 passes
/// after the reset puts them.
const PASSES_AFTER_RESET: i32 = 100;
/// And the noise 1784 steps past zero as the first pass after it begins.
const NOISE_STEPS_AFTER_RESET: i32 = 1784;

/// The counters as the reset leaves them.
static COUNTERS_AFTER_RESET: Counters = {
    let mut c = Counters::new();
    let mut i = 0;
    while i < PASSES_AFTER_RESET {
        c.advance();
        i += 1;
    }
    c
};

/// The noise as the reset leaves it.
static NOISE_AFTER_RESET: u32 = {
    let mut s = 0;
    let mut i = 0;
    while i < NOISE_STEPS_AFTER_RESET {
        s = ymf262::noise_step(s);
        i += 1;
    }
    s
};

/// A write on its way to the chip's registers: the slot time of its pass it
/// takes effect at.
#[derive(Clone, Copy, Default)]
struct Landing {
    /// It takes effect.
    due: bool,
    /// The register's address.
    address: u16,
    /// What is written.
    value: u8,
    /// The slot time of its pass it takes effect at.
    slot_time: i32,
}

/// A write in the queue.
#[derive(Clone, Copy, Default)]
struct Pending {
    /// The register's address.
    address: u16,
    /// What is written.
    value: u8,
    /// The address has been written, and the data is to follow.
    address_written: bool,
}

/// How a pass is worked out: by the portable pass, the loops in Rust, which
/// every build has, or by a pass written by hand in assembly (x86-64 and
/// AArch64).
#[derive(Clone, Copy)]
enum Pass {
    /// The portable pass.
    Portable,
    /// A pass written by hand.
    #[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
    Hand(Hand),
}

/// What derive reads for each lane, besides the constants of the lanes, as it
/// last read it: the fields of the core that it reads by the lane, of the same
/// names.
struct DerivedFrom {
    /// `Core::l20`.
    l20: [u8; LANES],
    /// `Core::l40`.
    l40: [u8; LANES],
    /// `Core::l60`.
    l60: [u8; LANES],
    /// `Core::l80`.
    l80: [u8; LANES],
    /// `Core::le0`.
    le0: [u8; LANES],
    /// `Core::lfnum`.
    lfnum: [u16; LANES],
    /// `Core::lblock`.
    lblock: [u8; LANES],
    /// `Core::lkey`.
    lkey: [u8; LANES],
    /// `Core::lpan`.
    lpan: [u8; LANES],
    /// `Core::lfb`.
    lfb: [u8; LANES],
    /// `Core::lnts`.
    lnts: [u8; LANES],
    /// `Core::ldrum_keys`.
    ldrum_keys: [u8; LANES],
    /// `Core::ldam`.
    ldam: [u8; LANES],
    /// `Core::ldvb`.
    ldvb: [u8; LANES],
    /// `Core::lrhythm_keys`.
    lrhythm_keys: [u8; LANES],
    /// `Core::modulation`.
    modulation: [u8; LANES],
    /// `Core::weight`.
    weight: [u8; LANES],
}

impl DerivedFrom {
    /// Noughts, which the first derive after a reset does not go by.
    const fn new() -> Self {
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

/// A group of the registers that the slots read ([`Core::written`]): those of
/// the slots, from 0x20, 0x40, 0x60, 0x80 and 0xe0 (`l20` to `le0`).
const GROUP_SLOT: u8 = 1;
/// The channels' frequencies, blocks and keys, from 0xa0 and 0xb0 (`lfnum`,
/// `lblock`, `lkey`).
const GROUP_FREQUENCY: u8 = 2;
/// The channels' outputs, connections and feedbacks, from 0xc0 (`lpan`,
/// `lcon`, `lfirst_con`, `lsecond_con`, `lfb`).
const GROUP_CHANNEL: u8 = 4;
/// The registers of the whole chip that the slots read, from 0x08 and 0xbd
/// (`lnts`, `ldrum_keys`, `ldam`, `ldvb`, `lrhythm` and the readings of the
/// rhythm before and after).
const GROUP_CHIP: u8 = 8;
/// The register of the chains of four, 0x104 (`lfour_op`).
const GROUP_CHAINS: u8 = 16;
/// Every group.
const ALL_GROUPS: u8 = GROUP_SLOT | GROUP_FREQUENCY | GROUP_CHANNEL | GROUP_CHIP | GROUP_CHAINS;
/// The groups that the routing reads: the connections, the rhythm and the
/// chains.
const ROUTING_GROUPS: u8 = GROUP_CHANNEL | GROUP_CHIP | GROUP_CHAINS;

/// The groups of registers that a write to an address may change, as
/// [`Core::apply`] decodes it, more of them rather than fewer: a write below
/// 0x20, to the registers of the whole chip, may change any, as the chip then
/// writes the last data written from 0x20 on once more, and the register of
/// the chains spreads every channel; 0xd0 to 0xdf are none.
const fn groups_of(address: u16) -> u8 {
    match address.to_le_bytes()[0] {
        0x00..=0x1f => ALL_GROUPS,
        0xbd => GROUP_CHIP,
        0xa0..=0xbf => GROUP_FREQUENCY,
        0xc0..=0xcf => GROUP_CHANNEL,
        0xd0..=0xdf => 0,
        _ => GROUP_SLOT,
    }
}

/// ADLplug-Next OPL3, the project's own emulator core of the YMF262 (OPL3),
/// which makes what the low-level core drawn from a die shot of the chip
/// (YMF262-LLE) makes as libADLMIDI drives it, sample for sample.
///
/// A core works its passes over the slots out, one a sample, at a level of the
/// processor ([`Level`]): by a pass written by hand in assembly at the levels of
/// x86-64 from `avx2` on and at every level of AArch64, and by the portable
/// pass, loops in Rust, at `v2`, x86-64's baseline, and on an architecture
/// without levels; the code around a pass written by hand is taken at the tier
/// of its level, where the processor has it ([`Core::tier`]). All make the same
/// samples. A core keeps to the pass it is made with ([`Core::level`]).
///
/// What a core works with, its lanes and its queue of writes, is on the heap,
/// so that moving one moves a few kilobytes.
pub struct Core {
    /// By lane: what the passes work with and the state after them, at the
    /// offsets the passes in assembly read and write them at.
    lanes: Box<Lanes>,

    /// The first write of the queue.
    queue_head: usize,
    /// The writes the queue holds.
    queue_count: usize,
    /// The slot times until the queue may act again.
    queue_wait: i32,
    /// The last write to take effect, while some slots still hold what they
    /// read before it: the passes since, -1 for none.
    landed_age: i32,
    /// And its slot time.
    landed_slot_time: i32,
    /// The last write to the registers from 0x20 on: its bank as it was
    /// decoded.
    array_bank: usize,
    /// Its register.
    array_reg: u8,
    /// A write that takes effect in the next pass.
    landing_next: Landing,
    /// The queue of writes.
    queue: Box<[Pending; QUEUE_SIZE]>,

    // Registers as written: the slots' by lane, the channels' by channel.
    /// The frequency numbers.
    fnum: [u16; CHANNELS],
    /// The registers from 0x20.
    r20: [u8; LANES],
    /// The registers from 0x40.
    r40: [u8; LANES],
    /// The registers from 0x60.
    r60: [u8; LANES],
    /// The registers from 0x80.
    r80: [u8; LANES],
    /// The registers from 0xe0.
    re0: [u8; LANES],
    /// The blocks.
    block: [u8; CHANNELS],
    /// The keys.
    key: [u8; CHANNELS],
    /// The connections.
    con: [u8; CHANNELS],
    /// The feedbacks.
    fb: [u8; CHANNELS],
    /// The outputs a channel is heard on.
    pan: [u8; CHANNELS],
    /// The connection as a chain of four reads it.
    con_pair: [u8; CHANNELS],
    /// The OPL3's own mode.
    newm: u8,
    /// The register of the chains of four.
    four_op: u8,
    /// The note select.
    nts: u8,
    /// Rhythm mode.
    rhythm: u8,
    /// The drums' keys.
    drum_keys: u8,
    /// The tremolo's depth.
    dam: u8,
    /// The vibrato's depth.
    dvb: u8,
    /// The data of the last write to the registers from 0x20 on.
    array_value: u8,
    /// Some channel is soft panned.
    soft_pan: bool,
    /// The level whose pass the core takes.
    level: Option<Level>,
    /// That pass.
    pass: Pass,
    /// Whether what the passes go by is worked out for every lane at once by
    /// [`Core::derive_split`], at the tier x86-64-v4, where its loop is vector
    /// code of AVX-512's width: on GitHub's runners, the EPYC 9V45 and the Xeon
    /// Platinum 8370C, a whole sample with its writes took 0.4 to 1.5% less by
    /// it there than by derive for the changed lanes alone, and 2.9 to 3.2%
    /// less than by working every lane out in derive's loop, which takes
    /// branches and lookups. Set once, as the core is made, by its tier.
    #[cfg(target_arch = "x86_64")]
    split_derive: bool,
    /// What derive last read for each lane, by which it tells the lanes whose
    /// inputs have changed since ([`Core::changed_lanes`]).
    derived: DerivedFrom,
    /// The next derive works every lane out, as after a reset.
    derive_all: bool,
    /// The groups of registers written since the slots last read every write
    /// (`GROUP_SLOT` and the others), so that [`Core::relatch`], the routing
    /// and derive take those alone: the registers of a group that no write
    /// has changed since are what the slots read of them already.
    written: u8,

    // By lane: the channels' registers as they stand, for the slots that read
    // them (spread()), so that no loop over the lanes reads another lane's:
    // those of the slot's own channel, but in the second channel of a chain of
    // four the first's frequency, block and key; and in a chain, the first
    // channel's connection and the second's as a chain reads it.
    /// The frequency number.
    ch_fnum: [u16; LANES],
    /// The block.
    ch_block: [u8; LANES],
    /// The key.
    ch_key: [u8; LANES],
    /// The connection.
    ch_con: [u8; LANES],
    /// The feedback.
    ch_fb: [u8; LANES],
    /// The outputs.
    ch_pan: [u8; LANES],
    /// The first channel's connection, in a chain.
    first_con: [u8; LANES],
    /// The second channel's as a chain reads it.
    second_con: [u8; LANES],

    // By lane: the registers as the slot last read them. The chip reads a
    // slot's own registers a slot time before its turn, the feedback of its
    // channel four before, and the registers of the whole chip at its turn.
    /// The frequency number.
    lfnum: [u16; LANES],
    /// The register from 0x20.
    l20: [u8; LANES],
    /// The register from 0x40.
    l40: [u8; LANES],
    /// The register from 0x60.
    l60: [u8; LANES],
    /// The register from 0x80.
    l80: [u8; LANES],
    /// The register from 0xe0.
    le0: [u8; LANES],
    /// The block.
    lblock: [u8; LANES],
    /// The key.
    lkey: [u8; LANES],
    /// The outputs.
    lpan: [u8; LANES],
    /// The feedback.
    lfb: [u8; LANES],
    /// The channel's connection as the channel's first slot read it.
    lcon: [u8; LANES],
    /// In a chain of four, the first channel's connection as the chain's first
    /// slot read it.
    lfirst_con: [u8; LANES],
    /// And the second's as a chain reads it.
    lsecond_con: [u8; LANES],
    /// The note select.
    lnts: [u8; LANES],
    /// The drums' keys.
    ldrum_keys: [u8; LANES],
    /// The tremolo's depth.
    ldam: [u8; LANES],
    /// The vibrato's depth.
    ldvb: [u8; LANES],
    /// The register of the chains of four.
    lfour_op: [u8; LANES],
    // The rhythm flag, as read for the drums' keys, for stopping feedback and
    // modulation, for the drums' phases, and for what is heard.
    /// The rhythm flag, as read for the drums' keys.
    lrhythm_keys: [u8; LANES],
    /// For stopping feedback and modulation.
    lrhythm_before: [u8; LANES],
    /// For the drums' phases.
    lrhythm: [u8; LANES],
    /// For what is heard.
    lrhythm_after: [u8; LANES],

    // By lane: what the routing makes of them.
    /// What the operator is modulated by: 0 none, 1 the stage before, 2
    /// feedback.
    modulation: [u8; LANES],
    /// How loud it is heard: 0 not heard, 1, or 2 for a drum.
    weight: [u8; LANES],
    /// The phase of its own it takes: 0, or 1 hi-hat, 2 snare, 3 top cymbal.
    rhythm_phase: [u8; LANES],
    /// Its value is not stored for feedback.
    hold_history: [u8; LANES],
}

impl Core {
    /// The slots of the chip.
    pub const SLOTS: usize = SLOTS;
    /// The channels of the chip.
    pub const CHANNELS: usize = CHANNELS;

    /// A core at the most preferred level the processor has ([`Choice`]), or
    /// by the portable pass on an architecture without levels.
    #[must_use]
    pub fn new() -> Self {
        Choice::new(Features::detect())
            .level
            .and_then(Self::at)
            .unwrap_or_else(Self::portable)
    }

    /// A core at a level, where the processor has it: by the pass written by
    /// hand for the level, or by the portable pass at `v2`, the level whose
    /// pass it is. None for a level the processor has not, and for one of
    /// another architecture.
    #[must_use]
    pub fn at(level: Level) -> Option<Self> {
        if !level.is_native() || !Features::detect().has(level) {
            return None;
        }
        let pass = match level {
            Level::V2 => Pass::Portable,
            #[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
            _ => Pass::Hand(Hand::new(level)?),
            #[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64")))]
            _ => return None,
        };
        Some(Self::with(Some(level), pass))
    }

    /// A core by the portable pass: at `v2` on x86-64, the level whose pass it
    /// is, and at no level elsewhere: AArch64's baseline, `neon`, has a pass
    /// written by hand.
    #[must_use]
    pub fn portable() -> Self {
        Self::with(PORTABLE_LEVEL, Pass::Portable)
    }

    /// The level whose pass the core takes: for the portable pass `v2` on
    /// x86-64, and none elsewhere.
    #[must_use]
    pub const fn level(&self) -> Option<Level> {
        self.level
    }

    /// The tier of x86-64 that the code around the core's pass is compiled for
    /// ([`Tier`]): for a pass written by hand, its level's, or the most below
    /// it that the processor has; the baseline for the portable pass.
    #[must_use]
    pub const fn tier(&self) -> Tier {
        match self.pass {
            Pass::Portable => Tier::Baseline,
            #[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
            Pass::Hand(hand) => hand.tier(),
        }
    }

    /// A core of noughts that takes a pass, reset.
    fn with(level: Option<Level>, pass: Pass) -> Self {
        let mut core = Self {
            lanes: Box::new(Lanes::new()),
            queue_head: 0,
            queue_count: 0,
            queue_wait: 0,
            landed_age: -1,
            landed_slot_time: 0,
            array_bank: 0,
            array_reg: 0,
            landing_next: Landing::default(),
            queue: Box::new([Pending::default(); QUEUE_SIZE]),
            fnum: [0; CHANNELS],
            r20: [0; LANES],
            r40: [0; LANES],
            r60: [0; LANES],
            r80: [0; LANES],
            re0: [0; LANES],
            block: [0; CHANNELS],
            key: [0; CHANNELS],
            con: [0; CHANNELS],
            fb: [0; CHANNELS],
            pan: [0; CHANNELS],
            con_pair: [0; CHANNELS],
            newm: 0,
            four_op: 0,
            nts: 0,
            rhythm: 0,
            drum_keys: 0,
            dam: 0,
            dvb: 0,
            array_value: 0,
            soft_pan: false,
            level,
            pass,
            #[cfg(target_arch = "x86_64")]
            split_derive: false,
            derived: DerivedFrom::new(),
            derive_all: true,
            written: ALL_GROUPS,
            ch_fnum: [0; LANES],
            ch_block: [0; LANES],
            ch_key: [0; LANES],
            ch_con: [0; LANES],
            ch_fb: [0; LANES],
            ch_pan: [0; LANES],
            first_con: [0; LANES],
            second_con: [0; LANES],
            lfnum: [0; LANES],
            l20: [0; LANES],
            l40: [0; LANES],
            l60: [0; LANES],
            l80: [0; LANES],
            le0: [0; LANES],
            lblock: [0; LANES],
            lkey: [0; LANES],
            lpan: [0; LANES],
            lfb: [0; LANES],
            lcon: [0; LANES],
            lfirst_con: [0; LANES],
            lsecond_con: [0; LANES],
            lnts: [0; LANES],
            ldrum_keys: [0; LANES],
            ldam: [0; LANES],
            ldvb: [0; LANES],
            lfour_op: [0; LANES],
            lrhythm_keys: [0; LANES],
            lrhythm_before: [0; LANES],
            lrhythm: [0; LANES],
            lrhythm_after: [0; LANES],
            modulation: [0; LANES],
            weight: [0; LANES],
            rhythm_phase: [0; LANES],
            hold_history: [0; LANES],
        };
        #[cfg(target_arch = "x86_64")]
        {
            core.split_derive = core.tier() == Tier::V4;
        }
        core.reset();
        core
    }

    /// The chip as libADLMIDI's wrapper of the low-level core leaves it when it
    /// resets it.
    pub fn reset(&mut self) {
        self.queue_head = 0;
        self.queue_count = 0;
        self.queue_wait = 0;
        self.landing_next = Landing::default();
        self.r20 = [0; LANES];
        self.r40 = [0; LANES];
        self.r60 = [0; LANES];
        self.r80 = [0; LANES];
        self.re0 = [0; LANES];
        self.fnum = [0; CHANNELS];
        self.block = [0; CHANNELS];
        self.key = [0; CHANNELS];
        self.con = [0; CHANNELS];
        self.con_pair = [0; CHANNELS];
        self.fb = [0; CHANNELS];
        self.pan = [3; CHANNELS];
        self.newm = 0;
        self.four_op = 0;
        self.nts = 0;
        self.rhythm = 0;
        self.drum_keys = 0;
        self.dam = 0;
        self.dvb = 0;
        self.array_bank = 0;
        self.array_reg = 0;
        self.array_value = 0;
        for channel in 0..CHANNELS {
            self.spread(channel);
        }
        let lanes = &mut *self.lanes;
        // The lanes' constants that the routing and derive() compare (the
        // lanes say why they are kept there).
        lanes.lane_slot = SLOT_OF;
        lanes.lane_first = FIRST_OPERATOR;
        lanes.lane_second_channel = CHAIN_SECOND_CHANNEL;
        lanes.phase = [0; LANES];
        lanes.level = [0x1ff; LANES];
        lanes.state = [ymf262::RELEASE; LANES];
        lanes.level16 = [sixteen(0x1ff); LANES];
        lanes.state16 = [sixteen(ymf262::RELEASE); LANES];
        lanes.out = [0; LANES];
        lanes.out_1 = [0; LANES];
        lanes.out_2 = [0; LANES];
        lanes.shared_history = [[0; 4]; 2];
        lanes.eg_out = [0x1ff; LANES];
        lanes.phase_out = [0; LANES];
        lanes.gain_a = [65536; LANES];
        lanes.gain_b = [65536; LANES];
        lanes.counters = COUNTERS_AFTER_RESET;
        lanes.noise = NOISE_AFTER_RESET;
        lanes.rhythm_bits = RhythmBits::default();
        lanes.a_carry = 0;
        lanes.b_carry = 0;
        lanes.a_carry_soft = 0;
        lanes.b_carry_soft = 0;
        self.soft_pan = false;
        self.landed_age = 0;
        self.landed_slot_time = -4; // everything read afresh
        self.written = ALL_GROUPS;
        self.relatch();
        self.refresh_routing();
        self.derive_all = true;
        self.derive();
        self.landed_age = -1;
        self.written = 0;
    }

    /// Queues a register write: the address of the register, the second bank's
    /// from 0x100, and what is written. The writes take effect one after
    /// another, a write every 2 2/9 samples, each at the slot time it reaches
    /// the chip. A queue of 2048 writes is full, and a write to it has the
    /// oldest take effect at once.
    pub fn write(&mut self, address: u16, value: u8) {
        if self.queue_count == QUEUE_SIZE {
            // Full: the oldest write takes effect at once.
            let p = self.queue[self.queue_head];
            self.apply(p.address, p.value);
            self.landed_age = 0;
            self.landed_slot_time = -4;
            self.queue_head = (self.queue_head + 1) % QUEUE_SIZE;
            self.queue_count -= 1;
        }
        let end = (self.queue_head + self.queue_count) % QUEUE_SIZE;
        self.queue[end] = Pending {
            address,
            value,
            address_written: false,
        };
        self.queue_count += 1;
    }

    /// Soft panning of a channel, named by the address of its 0xc0 register or
    /// that less 0xc0: 0 left only, 64 both at full, 127 right only. It takes
    /// effect at once.
    pub fn write_pan(&mut self, address: u16, value: u8) {
        let c = usize::from(address & 0x0f);
        if c > 8 {
            return;
        }
        let channel = usize::from((address >> 8) & 1) * 9 + c;
        let v = i32::from(value & 0x7f);
        // The side the channel is panned towards stays at full; the other falls
        // away along a quarter of a sine.
        let mut a = 1.0;
        let mut b = 1.0;
        match v.cmp(&64) {
            Ordering::Less => b = (PI / 2.0 * f64::from(v) / 64.0).sin(),
            Ordering::Greater => a = (PI / 2.0 * f64::from(127 - v) / 63.0).sin(),
            Ordering::Equal => {}
        }
        let lanes = &mut *self.lanes;
        for lane in LANES_OF_CHANNEL[channel] {
            lanes.gain_a[lane] = gain(a);
            lanes.gain_b[lane] = gain(b);
        }
        let mut panned = 0;
        for i in 0..LANES {
            let full_a = lanes.gain_a[i] == 65536;
            let full_b = lanes.gain_b[i] == 65536;
            panned += i32::from(!(full_a && full_b));
        }
        self.soft_pan = panned != 0;
        self.derive_soft_pan();
    }

    /// One sample at the chip's rate: output A, then output B.
    pub fn generate(&mut self) -> [i32; 2] {
        match self.pass {
            Pass::Portable => {
                self.take_writes();
                self.envelope_and_phase();
                self.rhythm_phases();
                self.operators();
                self.store_history();
                let frame = self.accumulate();
                self.advance();
                frame
            }
            #[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
            Pass::Hand(hand) => hand.generate(self),
        }
    }

    /// One sample by a pass written by hand, as [`Core::generate`] makes it:
    /// `x86_64.rs` and `aarch64.rs` compile it for the tier of the pass, as well
    /// as for the baseline.
    #[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "each tier's form of a sample is compiled whole for that tier, its loops vector code of the tier's width"
    )]
    pub(crate) fn generate_by_hand(&mut self, hand: Hand) -> [i32; 2] {
        self.take_writes();
        let frame = self.hand_written_pass(hand);
        self.advance();
        frame
    }

    /// The writes, over the sample this call stands for: what the slots read of
    /// the last write that they had not read yet, the write that takes effect
    /// in this pass, if any, and what they read of it; then what the routing
    /// and the passes make of what they have read.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "its loops over the lanes belong in each tier's form of a sample, where they are vector code of the tier's width"
    )]
    fn take_writes(&mut self) {
        // The write that takes effect in this pass: one the last sample sent
        // into it, or one this sample sends so early that it still reaches it
        // (the writes are 80 slot times apart, so never both).
        let sent_before = self.landing_next;
        let (now, next) = self.schedule_writes();
        self.landing_next = next;
        let due = if now.due { now } else { sent_before };

        // What the slots read of the last write that they had not read yet.
        let reading = self.landed_age >= 0;
        if reading {
            self.relatch();
            if SLOT_TIMES * self.landed_age + READS_FEEDBACK >= self.landed_slot_time {
                self.landed_age = -1;
            }
        }
        if due.due {
            self.land(due);
            self.relatch();
        }
        // The routing and what the passes go by come of what the slots have
        // read and of nothing that a write changes, so once for both readings.
        if reading || due.due {
            self.refresh_routing();
            self.derive();
        }
        // Every write read: the registers are what the slots read of them.
        if self.landed_age < 0 {
            self.written = 0;
        }
        self.refresh_increments();
    }

    /// The lanes' phase increments in this pass, worked out again when what
    /// they come of has changed since they were: the vibrato's position, which
    /// moves on every 1024 samples, or what the passes go by, which only writes
    /// change ([`Core::derive`] marks them). Every pass takes them from the
    /// lanes, which the C++ version's passes worked out in each pass.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "its loop over the lanes belongs in each tier's form of a sample, where it is vector code of the tier's width"
    )]
    fn refresh_increments(&mut self) {
        let lanes = &mut *self.lanes;
        let position = lanes.counters.vibrato_position;
        if lanes.increments_for == position {
            return;
        }
        lanes.increments_for = position;
        let vibrato = ymf262::vibrato(position);
        for i in 0..LANES {
            lanes.increment.0[i] = ymf262::phase_increment(
                lanes.fnum_v[i],
                lanes.block_v[i],
                lanes.multiple_v[i],
                lanes.vibrato[i] != 0,
                lanes.vibrato_depth[i] != 0,
                &vibrato,
            );
        }
    }

    /// The counters and the noise, on to the next pass, and the age of the last
    /// write.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "it belongs in each tier's form of a sample with the rest of the sample"
    )]
    fn advance(&mut self) {
        self.lanes.counters.advance();
        self.lanes.noise = ymf262::noise_pass(self.lanes.noise);
        if self.landed_age >= 0 {
            self.landed_age += 1;
        }
    }

    /// The queue, over the sample this call stands for: the write, if any,
    /// whose data reaches the chip in the pass worked out now, and the one
    /// whose data reaches it in the next.
    fn schedule_writes(&mut self) -> (Landing, Landing) {
        let mut now = Landing::default();
        let mut next = Landing::default();
        let mut t = self.queue_wait;
        while self.queue_count > 0 && t < SLOT_TIMES {
            let p = &mut self.queue[self.queue_head];
            if !p.address_written {
                p.address_written = true;
                t += ADDRESS_TO_DATA;
                continue;
            }
            let landing = Landing {
                due: true,
                address: p.address,
                value: p.value,
                slot_time: if t < 2 { t + 34 } else { t - 2 },
            };
            if t < 2 {
                now = landing;
            } else {
                next = landing;
            }
            self.queue_head = (self.queue_head + 1) % QUEUE_SIZE;
            self.queue_count -= 1;
            t += DATA_TO_ADDRESS;
        }
        self.queue_wait = if t > SLOT_TIMES { t - SLOT_TIMES } else { 0 };
        (now, next)
    }

    /// A write that takes effect, at its slot time.
    fn land(&mut self, landing: Landing) {
        self.apply(landing.address, landing.value);
        self.landed_age = 0;
        self.landed_slot_time = landing.slot_time;
    }

    /// A write as it reaches the chip's registers.
    fn apply(&mut self, address: u16, value: u8) {
        self.written |= groups_of(address);
        let [reg, high] = address.to_le_bytes();
        // The second bank is there only in the OPL3's own mode, but for the
        // register that turns that mode on; before it, its addresses are the
        // first bank's.
        let bank = if self.newm != 0 { usize::from(high & 1) } else { 0 };
        if reg < 0x20 {
            if reg == 0x05 && (high & 1) != 0 {
                self.newm = value & 1;
            } else if bank == 1 && reg == 0x04 {
                self.four_op = value & 0x3f;
                for channel in 0..CHANNELS {
                    self.spread(channel);
                }
            } else if bank == 0 && reg == 0x08 {
                self.nts = (value >> 6) & 1;
            }
            // The chip then writes the last data written to the registers from
            // 0x20 on once more, to the same register, decoded afresh.
            self.apply_array(self.array_bank, self.array_reg, self.array_value);
            return;
        }
        self.array_bank = bank;
        self.array_reg = reg;
        self.array_value = value;
        if reg == 0xbd && bank == 0 {
            self.dam = (value >> 7) & 1;
            self.dvb = (value >> 6) & 1;
            self.rhythm = (value >> 5) & 1;
            self.drum_keys = value & 0x1f;
        }
        self.apply_array(bank, reg, value);
    }

    /// A write to the registers of the operators and the channels.
    fn apply_array(&mut self, bank: usize, reg: u8, value: u8) {
        if (0xa0..0xd0).contains(&reg) {
            // The chip decodes a channel number of 9 as 7 and 10 as 8, and
            // ignores those above; but for the feedback, and the connection as
            // a chain of four reads it, of 0xc0 it takes 12, 13 and 14 as the
            // first bank's channels 3, 4 and 5, whichever bank they are written
            // to.
            let mut c = usize::from(reg & 0x0f);
            if (reg & 0xf0) == 0xc0 && (12..=14).contains(&c) {
                let first_bank = c - 9;
                self.con_pair[first_bank] = value & 1;
                self.fb[first_bank] = (value >> 1) & 7;
                self.spread(first_bank);
                return;
            }
            if c == 9 || c == 10 {
                c -= 2;
            } else if c > 8 {
                return;
            }
            let ch = bank * 9 + c;
            match reg & 0xf0 {
                0xa0 => self.fnum[ch] = (self.fnum[ch] & 0x300) | u16::from(value),
                0xb0 => {
                    self.fnum[ch] = (self.fnum[ch] & 0xff) | (u16::from(value & 3) << 8);
                    self.block[ch] = (value >> 2) & 7;
                    self.key[ch] = (value >> 5) & 1;
                }
                _ => {
                    // 0xc0
                    self.con[ch] = value & 1;
                    self.con_pair[ch] = value & 1;
                    self.fb[ch] = (value >> 1) & 7;
                    self.pan[ch] = if self.newm != 0 { (value >> 4) & 15 } else { 3 };
                }
            }
            self.spread(ch);
            if c < 3 {
                self.spread(ch + 3);
            }
            return;
        }
        let Ok(s) = usize::try_from(SLOT_OF_OFFSET[usize::from(reg & 0x1f)]) else {
            return;
        };
        if (reg & 0x1f) > 0x15 {
            return;
        }
        let lane = LANE_OF_SLOT[bank * 18 + s];
        match reg & 0xe0 {
            0x20 => self.r20[lane] = value,
            0x40 => self.r40[lane] = value,
            0x60 => self.r60[lane] = value,
            0x80 => self.r80[lane] = value,
            0xe0 => self.re0[lane] = value & if self.newm != 0 { 7 } else { 3 },
            _ => {}
        }
    }

    /// A channel's registers, into the lanes of the slots that read them: its
    /// own two slots', which in the second channel of a chain of four go by the
    /// first channel's frequency, block and key; and in a chain, the chain's
    /// four, which route by the first channel's connection and by the second's
    /// as a chain reads it. A write to the first channel of a chain spreads the
    /// second as well, and a write to the register of the chains spreads every
    /// channel.
    fn spread(&mut self, channel: usize) {
        let c = channel % 9;
        let chain = channel / 9 * 3 + c % 3;
        let paired = (3..6).contains(&c) && ((self.four_op >> chain) & 1) != 0;
        let by = if paired { channel - 3 } else { channel }; // whose frequency, block and key
        for lane in LANES_OF_CHANNEL[channel] {
            self.ch_fnum[lane] = self.fnum[by];
            self.ch_block[lane] = self.block[by];
            self.ch_key[lane] = self.key[by];
            self.ch_con[lane] = self.con[channel];
            self.ch_fb[lane] = self.fb[channel];
            self.ch_pan[lane] = self.pan[channel];
        }
        if c < 3 {
            for lane in LANES_OF_CHAIN[chain] {
                self.first_con[lane] = self.con[channel];
            }
        } else if c < 6 {
            for lane in LANES_OF_CHAIN[chain] {
                self.second_con[lane] = self.con_pair[channel];
            }
        }
    }

    /// What each slot reads for the pass worked out now: the registers as they
    /// stand if it reads them after the last write took effect, or else what it
    /// read before, for the groups of registers written since the slots last
    /// read every write alone ([`Core::written`]), each in a loop of its own.
    /// The routing and what the passes go by follow from it
    /// (`refresh_routing()`, `derive()`). Most writes change one group, the
    /// registers of a slot or a channel's frequency, block and key, of three
    /// to eight of the 22 readings of every lane; on GitHub's runners a
    /// reading of every group took 14 to 24 ns at x86-64-v4, 41 to 77 at v3,
    /// 74 to 111 at the baseline and 54 to 63 on the Neoverse N2 and the
    /// Cobalt 100.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "its loops over the lanes belong in each tier's form of a sample, where they are vector code of the tier's width"
    )]
    fn relatch(&mut self) {
        let written = self.written;
        // Slot times from the write to this pass.
        let since = SLOT_TIMES * self.landed_age - self.landed_slot_time;
        if (written & GROUP_SLOT) != 0 {
            for (i, &s) in SLOT_TIME.iter().enumerate() {
                let own = since + s + READS_OWN >= 0;
                self.l20[i] = latch(own, self.r20[i], self.l20[i]);
                self.l40[i] = latch(own, self.r40[i], self.l40[i]);
                self.l60[i] = latch(own, self.r60[i], self.l60[i]);
                self.l80[i] = latch(own, self.r80[i], self.l80[i]);
                self.le0[i] = latch(own, self.re0[i], self.le0[i]);
            }
        }
        if (written & GROUP_FREQUENCY) != 0 {
            for (i, &s) in SLOT_TIME.iter().enumerate() {
                let own = since + s + READS_OWN >= 0;
                self.lfnum[i] = latch(own, self.ch_fnum[i], self.lfnum[i]);
                self.lblock[i] = latch(own, self.ch_block[i], self.lblock[i]);
                self.lkey[i] = latch(own, self.ch_key[i], self.lkey[i]);
            }
        }
        if (written & GROUP_CHANNEL) != 0 {
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
        if (written & GROUP_CHIP) != 0 {
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
        if (written & GROUP_CHAINS) != 0 {
            for (i, &t) in CHAIN_READ_TIME.iter().enumerate() {
                self.lfour_op[i] = latch(since + t >= 0, self.four_op, self.lfour_op[i]);
            }
        }
    }

    /// What the routing makes of what the slots read: what each is modulated
    /// by, how loud it is heard, the drum's phase it takes, and whether its
    /// value is stored for feedback. It reads the connections, the rhythm and
    /// the chains alone, so it is left as it is after a write to none of those
    /// ([`Core::written`]).
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "its loop over the lanes belongs in each tier's form of a sample, where it is vector code of the tier's width"
    )]
    fn refresh_routing(&mut self) {
        if (self.written & ROUTING_GROUPS) == 0 {
            return;
        }
        let lanes = &*self.lanes;
        for i in 0..LANES {
            let s = lanes.lane_slot[i];
            let first = lanes.lane_first[i] != 0;
            let second_channel = lanes.lane_second_channel[i] != 0;
            let chain = (self.lfour_op[i] & CHAIN_BIT[i]) != 0;
            let con = self.lcon[i] != 0;
            let first_con = self.lfirst_con[i] != 0;
            let second_con = self.lsecond_con[i] != 0;
            let rhythm_early = self.lrhythm_before[i] != 0;
            let rhythm = self.lrhythm[i] != 0;
            let rhythm_late = self.lrhythm_after[i] != 0;
            let phase = DRUM_PHASE[i];
            let heard = s >= 0;
            // Two operators: what the channel's first slot read of its
            // connection decides both whether that slot is heard and whether it
            // modulates the second.
            let second_modulation = u8::from(!con);
            let mut modulation = if first { 2 } else { second_modulation };
            let mut weight = u8::from(!first || con);
            // Four operators: the chain A B C D of channels f and f + 3. B is
            // heard and C goes unmodulated when the chain adds A and B to C and
            // D (the first channel's connection 0, the second's 1, both as A
            // read them); C is heard and D goes unmodulated, as for two
            // operators, when the second channel's connection as C read it is
            // 1, but not then. A goes as for two operators, and so does B's
            // modulation, and D is heard.
            let two_pairs = !first_con && second_con;
            let c_heard = !two_pairs && con;
            let is_b = chain && !second_channel && !first;
            let is_c = chain && second_channel && first;
            let is_d = chain && second_channel && !first;
            let b_weight = u8::from(two_pairs);
            let c_weight = u8::from(c_heard);
            let c_modulation = u8::from(!two_pairs);
            let d_modulation = u8::from(!c_heard);
            weight = if is_b { b_weight } else { weight };
            weight = if is_c { c_weight } else { weight };
            modulation = if is_c { c_modulation } else { modulation };
            modulation = if is_d { d_modulation } else { modulation };
            // Rhythm: channels 6, 7 and 8 of the first bank, slots 12 to 17.
            // The bass drum is the pair of channel 6 as it is, heard from its
            // carrier alone; the other four drums are a slot each, with neither
            // feedback nor modulation. A drum is heard twice as loud, and the
            // values of the hi-hat's and the tom's slots are not stored for
            // feedback.
            let drum = (12..18).contains(&s);
            let alone = drum && s != 12 && s != 15;
            let drum_weight = if s == 12 { 0 } else { 2 };
            modulation = if alone && rhythm_early { 0 } else { modulation };
            weight = if drum && rhythm_late { drum_weight } else { weight };
            weight = if heard { weight } else { 0 };
            self.modulation[i] = modulation;
            self.weight[i] = weight;
            self.hold_history[i] = u8::from(rhythm_late && (s == 13 || s == 14));
            self.rhythm_phase[i] = if rhythm { phase } else { 0 };
        }
    }

    /// What the passes go by, from what the slots read and the routing: for
    /// the lanes alone whose inputs have changed since derive last read them
    /// ([`Core::changed_lanes`]), every lane after a reset, and at x86-64-v4
    /// for every lane at once ([`Core::split_derive`]). A write changes what
    /// one lane reads, or a channel's lanes, or by the registers of the whole
    /// chip every lane: over the writes of the tests' eight seeds, a derive
    /// found 1.2 of the 48 lanes changed on average, and none in half of the
    /// derives. On GitHub's runners a whole sample with its writes took 10 to
    /// 27% less by the changed lanes alone than by working every lane out on
    /// AArch64 (the Neoverse N2, the Cobalt 100 and the Apple M1), and 2.6 to
    /// 6.6% less at x86-64's baseline and v3 (AMD's EPYC 7763, 9V74 and 9V45,
    /// and Intel's Xeon Platinum 8370C). When a lane has changed, the soft
    /// panning is worked out for every lane and the phase increments are
    /// marked; when none has, both stand as they are.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "it belongs in each tier's form of a sample, where its comparisons of the lanes are vector code of the tier's width"
    )]
    fn derive(&mut self) {
        #[cfg(target_arch = "x86_64")]
        if self.split_derive {
            self.derive_split();
            return;
        }
        let changed = self.changed_lanes();
        let mut any = false;
        let lanes = &mut *self.lanes;
        let was = &mut self.derived;
        for (lane, &drum_key_bit) in DRUM_KEY_BIT.iter().enumerate() {
            if !changed[lane] {
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
        if any {
            self.derive_soft_pan();
            self.lanes.increments_for = u8::MAX;
        }
    }

    /// The lanes whose inputs have changed since derive last read them
    /// ([`DerivedFrom`]), or every lane after a reset: compared for the groups
    /// of registers written since the slots last read every write alone
    /// ([`Core::written`]), since what derive reads of a group that no write
    /// has changed is what it read before, and what the routing makes
    /// (`modulation`, `weight`) only when it has been worked out again. Each
    /// group's comparisons are a loop of their own, without short-circuits,
    /// which is vector code.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "it belongs in each tier's form of a sample, where its loops are vector code of the tier's width"
    )]
    #[expect(
        clippy::needless_bitwise_bool,
        reason = "the comparisons without short-circuits are loops that vectorize"
    )]
    fn changed_lanes(&mut self) -> [bool; LANES] {
        let all = self.derive_all;
        self.derive_all = false;
        if all {
            return [true; LANES];
        }
        let written = self.written;
        let was = &self.derived;
        let mut changed = [false; LANES];
        if (written & GROUP_SLOT) != 0 {
            for (lane, changed) in changed.iter_mut().enumerate() {
                *changed |= (self.l20[lane] != was.l20[lane])
                    | (self.l40[lane] != was.l40[lane])
                    | (self.l60[lane] != was.l60[lane])
                    | (self.l80[lane] != was.l80[lane])
                    | (self.le0[lane] != was.le0[lane]);
            }
        }
        if (written & GROUP_FREQUENCY) != 0 {
            for (lane, changed) in changed.iter_mut().enumerate() {
                *changed |= (self.lfnum[lane] != was.lfnum[lane])
                    | (self.lblock[lane] != was.lblock[lane])
                    | (self.lkey[lane] != was.lkey[lane]);
            }
        }
        if (written & GROUP_CHANNEL) != 0 {
            for (lane, changed) in changed.iter_mut().enumerate() {
                *changed |= (self.lpan[lane] != was.lpan[lane]) | (self.lfb[lane] != was.lfb[lane]);
            }
        }
        if (written & GROUP_CHIP) != 0 {
            for (lane, changed) in changed.iter_mut().enumerate() {
                *changed |= (self.lnts[lane] != was.lnts[lane])
                    | (self.ldrum_keys[lane] != was.ldrum_keys[lane])
                    | (self.ldam[lane] != was.ldam[lane])
                    | (self.ldvb[lane] != was.ldvb[lane])
                    | (self.lrhythm_keys[lane] != was.lrhythm_keys[lane]);
            }
        }
        if (written & ROUTING_GROUPS) != 0 {
            for (lane, changed) in changed.iter_mut().enumerate() {
                *changed |= (self.modulation[lane] != was.modulation[lane]) | (self.weight[lane] != was.weight[lane]);
            }
        }
        changed
    }

    /// [`Core::derive`] for every lane at once, at x86-64-v4
    /// ([`Core::split_derive`]): with the one table that no expression takes
    /// the place of, the key scale level's ROM, looked up lane by lane first,
    /// and the rest worked out in a loop of its own, without lookups and
    /// without short-circuits. The multiples and
    /// the shifts of the key scale level are expressions
    /// (`ymf262::multiple_of`, `ymf262::ksl_shift_of`), and the conditions are
    /// `&` and `|` in place of `&&` and `||`, which say the same of booleans:
    /// of the values the loop stores, those that `&&` picked made LLVM branch
    /// around the stores, and a loop with stores under branches vectorizes
    /// only where the tier has stores under masks.
    #[cfg(target_arch = "x86_64")]
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "its loops over the lanes belong in each tier's form of a sample, where they are vector code of the tier's width"
    )]
    #[expect(
        clippy::needless_bitwise_bool,
        reason = "conditions without short-circuits are what this form is for: with them the loop stores without branches"
    )]
    fn derive_split(&mut self) {
        let mut ksl_rom = [0_u32; LANES];
        for (rom, &fnum) in ksl_rom.iter_mut().zip(&self.lfnum) {
            *rom = ymf262::ksl_rom_of(u32::from(fnum));
        }
        let lanes = &mut *self.lanes;
        for (lane, &drum_key_bit) in DRUM_KEY_BIT.iter().enumerate() {
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
            lanes.key_on[lane] = i32::from(key | (rhythm_keys & (drum_keys != 0)));
            lanes.level_offset[lane] =
                ymf262::level_offset_from(ksl_rom[lane], block, r40 >> 6, r40 & 63).cast_signed();
            let depth = if dam { 2 } else { 1 };
            lanes.tremolo_depth[lane] = if (r20 & 0x80) != 0 { depth } else { 0 };
            lanes.fnum_v[lane] = fnum;
            lanes.block_v[lane] = block;
            lanes.multiple_v[lane] = ymf262::multiple_of(r20 & 15);
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
            lanes.modulated_by[lane] = if (by == 2) & (fb == 0) { 0 } else { by };
            let shift = 9 - fb;
            lanes.feedback_shift[lane] = if fb != 0 { shift } else { 0 };
            let w = i32::from(self.weight[lane]);
            let pan = self.lpan[lane];
            let on_a = (pan & 1) != 0;
            let on_b = (pan & 2) != 0;
            lanes.a_now[lane] = if on_a & (s < FIRST_LATE_ON_A) { w } else { 0 };
            lanes.a_late[lane] = if on_a & (s >= FIRST_LATE_ON_A) { w } else { 0 };
            lanes.b_now[lane] = if on_b & (s < FIRST_LATE_ON_B) { w } else { 0 };
            lanes.b_late[lane] = if on_b & (s >= FIRST_LATE_ON_B) { w } else { 0 };
        }
        self.derive_soft_pan();
        self.lanes.increments_for = u8::MAX;
    }

    /// How loud each slot is on each output with soft panning.
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "its loop over the lanes belongs in each tier's form of a sample, where it is vector code of the tier's width"
    )]
    fn derive_soft_pan(&mut self) {
        let lanes = &mut *self.lanes;
        for i in 0..LANES {
            lanes.soft_a_now[i] = lanes.a_now[i] * lanes.gain_a[i];
            lanes.soft_a_late[i] = lanes.a_late[i] * lanes.gain_a[i];
            lanes.soft_b_now[i] = lanes.b_now[i] * lanes.gain_b[i];
            lanes.soft_b_late[i] = lanes.b_late[i] * lanes.gain_b[i];
        }
    }

    // The parts of the portable pass, each called once from generate().

    /// The envelope and the phase of every lane, for this pass, in two loops,
    /// so that neither holds more than a vector's registers can.
    #[inline]
    fn envelope_and_phase(&mut self) {
        let lanes = &mut *self.lanes;
        let timing = lanes.counters.envelope_timing();
        let tremolo_level = lanes.counters.tremolo_level();
        let tremolo_shallow = tremolo_level >> 4;
        let tremolo_deep = tremolo_level >> 2;
        for i in 0..LANES {
            let slot = ymf262::Envelope {
                key: lanes.key_on[i],
                rates: lanes.rates[i],
                ksr: lanes.key_scale[i],
                ksr_low: lanes.key_scale_low[i],
                sustain_level: lanes.sustain[i],
            };
            let e = ymf262::envelope(lanes.state[i], lanes.level[i], &slot, &timing);
            let depth = lanes.tremolo_depth[i];
            let shallow = if depth == 1 { tremolo_shallow } else { 0 };
            let tremolo = if depth == 2 { tremolo_deep } else { shallow };
            lanes.eg_out[i] = ymf262::attenuation(lanes.level[i], lanes.level_offset[i] + tremolo);
            lanes.level[i] = e.level;
            lanes.state[i] = e.state;
            lanes.restart[i] = i32::from(e.restart);
        }
        for i in 0..LANES {
            lanes.phase_out[i] = ymf262::phase_out(lanes.phase[i]);
            lanes.phase[i] = ymf262::next_phase(lanes.phase[i], lanes.increment.0[i], lanes.restart[i] != 0);
        }
    }

    /// Rhythm: the hi-hat, the snare and the top cymbal take phases of their
    /// own. The hi-hat's bits are taken every pass, the top cymbal's only in
    /// rhythm mode.
    #[inline]
    fn rhythm_phases(&mut self) {
        let lanes = &mut *self.lanes;
        lanes.rhythm_bits.take_hi_hat(lanes.phase_out[LANE_13]);
        if self.rhythm_phase[LANE_13] != 0 {
            lanes.phase_out[LANE_13] = lanes.rhythm_bits.hi_hat(ymf262::hi_hat_noise(lanes.noise));
        }
        if self.rhythm_phase[LANE_16] != 0 {
            lanes.phase_out[LANE_16] = lanes.rhythm_bits.snare(ymf262::snare_noise(lanes.noise));
        }
        if self.rhythm_phase[LANE_17] != 0 {
            lanes.rhythm_bits.take_top_cymbal(lanes.phase_out[LANE_17]);
            lanes.phase_out[LANE_17] = lanes.rhythm_bits.top_cymbal();
        }
    }

    /// The operators of one stage, over its lanes.
    #[inline]
    fn stage<const BEGIN: usize, const BEFORE: usize, const WIDTH: usize>(&mut self) {
        let lanes = &mut *self.lanes;
        for j in 0..WIDTH {
            let i = BEGIN + j;
            let source = BEFORE + j;
            let modulation = ymf262::modulation(
                lanes.out[source],
                lanes.out_1[i] + lanes.out_2[i],
                lanes.feedback_shift[i],
                lanes.modulated_by[i],
            );
            let wave = ymf262::Wave {
                phase_shift: lanes.phase_shift[i],
                turn_bit: lanes.turn_bit[i],
                mute_bit: lanes.mute_bit[i],
                sign_bit: lanes.sign_bit[i],
                square: lanes.square[i],
                sawtooth: lanes.sawtooth[i],
            };
            lanes.out[i] = ymf262::operator_output(lanes.phase_out[i] + modulation, &wave, lanes.eg_out[i]);
        }
    }

    /// The feedback history of the slots that share a queue, then the operators
    /// stage by stage.
    #[inline]
    fn operators(&mut self) {
        // The feedback history of slots 13 and 31 and of 14 and 32, from their
        // queues: slot m reads the second and the fourth; slot m + 18 reads them
        // after m's value has gone in, or else the same two.
        let lanes = &mut *self.lanes;
        for (f, &(low, high)) in lanes.shared_history.iter().zip(&SHARED) {
            lanes.out_1[low] = f[1];
            lanes.out_2[low] = f[3];
            let stored = self.hold_history[low] == 0;
            lanes.out_1[high] = if stored { f[0] } else { f[1] };
            lanes.out_2[high] = if stored { f[2] } else { f[3] };
        }
        self.stage::<{ STAGE_BEGIN[0] }, { STAGE_BEGIN[0] }, { STAGE_WIDTH[0] }>();
        self.stage::<{ STAGE_BEGIN[1] }, { STAGE_BEGIN[0] }, { STAGE_WIDTH[1] }>();
        self.stage::<{ STAGE_BEGIN[2] }, { STAGE_BEGIN[1] }, { STAGE_WIDTH[2] }>();
        self.stage::<{ STAGE_BEGIN[3] }, { STAGE_BEGIN[2] }, { STAGE_WIDTH[3] }>();
    }

    /// This pass's values into the history of the last two, and into the
    /// shared queues: slot m's but when it is held out, then slot m + 18's.
    #[inline]
    fn store_history(&mut self) {
        let lanes = &mut *self.lanes;
        for i in 0..LANES {
            lanes.out_2[i] = lanes.out_1[i];
            lanes.out_1[i] = lanes.out[i];
        }
        for (f, &(low, high)) in lanes.shared_history.iter_mut().zip(&SHARED) {
            if self.hold_history[low] == 0 {
                *f = [lanes.out[low], f[0], f[1], f[2]];
            }
            *f = [lanes.out[high], f[0], f[1], f[2]];
        }
    }

    /// The sums of the two outputs: what the slots heard now make, with what
    /// those heard late made in the last pass, in nineteen bits, as the chip
    /// adds, then sixteen.
    #[inline]
    fn accumulate(&mut self) -> [i32; 2] {
        let lanes = &mut *self.lanes;
        if !self.soft_pan {
            let mut a_now = 0;
            let mut a_late = 0;
            let mut b_now = 0;
            let mut b_late = 0;
            for i in 0..LANES {
                let v = lanes.out[i];
                a_now += v * lanes.a_now[i];
                a_late += v * lanes.a_late[i];
                b_now += v * lanes.b_now[i];
                b_late += v * lanes.b_late[i];
            }
            let frame = [clip(a_now + lanes.a_carry), clip(b_now + lanes.b_carry)];
            lanes.a_carry = a_late;
            lanes.b_carry = b_late;
            lanes.a_carry_soft = i64::from(a_late) * 65536;
            lanes.b_carry_soft = i64::from(b_late) * 65536;
            return frame;
        }
        // Soft panning: the gains in sixteenths of sixteen bits, the sums
        // exact, and one rounding (half away from zero) at the end.
        let mut a_now = 0;
        let mut a_late = 0;
        let mut b_now = 0;
        let mut b_late = 0;
        for i in 0..LANES {
            let v = i64::from(lanes.out[i]);
            a_now += v * i64::from(lanes.soft_a_now[i]);
            a_late += v * i64::from(lanes.soft_a_late[i]);
            b_now += v * i64::from(lanes.soft_b_now[i]);
            b_late += v * i64::from(lanes.soft_b_late[i]);
        }
        let frame = [
            clip(round16(a_now + lanes.a_carry_soft)),
            clip(round16(b_now + lanes.b_carry_soft)),
        ];
        lanes.a_carry_soft = a_late;
        lanes.b_carry_soft = b_late;
        lanes.a_carry = round16(a_late);
        lanes.b_carry = round16(b_late);
        frame
    }

    /// The pass written by hand, for AVX2 (`pass_avx2.S`) without AVX-VNNI or
    /// with it, for AVX-512 (`pass_avx512.S`), or for NEON, SHA3, SVE or SVE2
    /// (`pass_neon.S`),
    /// given what it goes by besides the lanes: what the counters and the noise
    /// give for this pass, and what the routing says of the drums and of the
    /// shared feedback history.
    #[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))]
    #[inline(always)]
    #[expect(
        clippy::inline_always,
        reason = "the pass's inputs are made in each tier's form of a sample, and stored as wide as the pass loads them"
    )]
    fn hand_written_pass(&mut self, hand: Hand) -> [i32; 2] {
        let rhythm = [
            self.rhythm_phase[LANE_13],
            self.rhythm_phase[LANE_16],
            self.rhythm_phase[LANE_17],
        ];
        let hold = [self.hold_history[LANE_13], self.hold_history[LANE_14]];
        let inputs = PassInputs::new(&self.lanes.counters, self.lanes.noise, rhythm, hold, self.soft_pan);
        hand.run(&mut self.lanes, &inputs)
    }
}

impl Default for Core {
    /// [`Core::new`].
    fn default() -> Self {
        Self::new()
    }
}

impl fmt::Debug for Core {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter
            .debug_struct("Core")
            .field("level", &self.level)
            .finish_non_exhaustive()
    }
}

/// What a lane holds after a reading: the register as it stands if the slot
/// reads it after the write, or else what it held. Both are loaded, and the
/// value chosen, rather than the one place to load from: by
/// `select_unpredictable`, which takes the two values as they are, where an
/// `if` lets the compiler move each read into its arm and then read from the
/// place it chooses, which in a loop over the lanes is a gather from two
/// arrays, and keeps the loop from being vectorized.
#[inline(always)]
#[expect(
    clippy::inline_always,
    reason = "a lane's reading belongs in the loop over the lanes, in each tier's form of a sample"
)]
fn latch<T>(reads: bool, now: T, held: T) -> T {
    std::hint::select_unpredictable(reads, now, held)
}

/// A state or a level of an envelope in 16 bits, as the passes written by hand
/// keep them.
fn sixteen(value: i32) -> i16 {
    i16::try_from(value).expect("a state or a level of nine bits")
}

/// A gain of soft panning, from 0 to 1, in sixteenths of sixteen bits: times
/// 65536 and rounded half away from nought, as `lround` rounds, then read from
/// the double with 2^52 added, whose low bits then hold the whole number as it
/// is.
fn gain(fraction: f64) -> i32 {
    let rounded = (fraction * 65536.0).round();
    let bits = (rounded + 4_503_599_627_370_496.0).to_bits() & 0x000f_ffff_ffff_ffff;
    i32::try_from(bits).expect("a gain from 0 to 65536")
}

/// A sum in nineteen bits, as the chip adds, then held to sixteen.
#[inline]
fn clip(x: i32) -> i32 {
    ((x.cast_unsigned() << 13).cast_signed() >> 13).clamp(-32768, 32767)
}

/// A sum in sixteenths of sixteen bits, shifted down by 16 and rounded half
/// away from nought. The sums of the 36 slots stay within 2^36, so it fits 32
/// bits.
#[inline]
fn round16(x: i64) -> i32 {
    let rounded = if x >= 0 {
        (x + 32768) >> 16
    } else {
        -((32768 - x) >> 16)
    };
    i32::try_from(rounded).expect("a sum of the slots within 2^20")
}
