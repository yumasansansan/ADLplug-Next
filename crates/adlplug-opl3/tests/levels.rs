// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! The core at every level of the processor, as the C++ version's tests of its
//! core checked it (`tests/unit/opl3_core_tests.cc` of the C++ version):
//!
//! - the passes written by hand make the portable pass's samples, sample for
//!   sample, for random writes at random times, with a channel soft panned
//!   every 500 samples and every third time back to the centre, at each level
//!   the processor has, the others left out with a note
//!   (`opl3_core_hand_written_passes`);
//! - a core that nothing is written to makes nothing, and one given writes
//!   makes sound (`opl3_core_silence_and_sound`);
//! - soft panning at the centre is the chip's own output, and panning hard to
//!   one side silences the other (`opl3_core_soft_panning`).
//!
//! And a core takes the level it is made at, the most preferred the processor
//! has when none is named, and none that the processor has not; and the code
//! around its pass the tier of the level, or the most below it that the
//! processor has.

mod common;

use adlplug_cpu::{Choice, Features, LEVELS, Level, Tier};
use adlplug_opl3::Core;

/// A core's samples for writes, each written before the sample it is at, and
/// with the soft panning of the C++ version's test of the passes written by
/// hand for a seed, where one is given (written before the sample too).
fn render(core: &mut Core, writes: &[common::Write], samples: u32, panning: Option<u32>) -> Vec<[i32; 2]> {
    let mut out = Vec::with_capacity(usize::try_from(samples).expect("a count of samples"));
    let mut k = 0;
    for n in 0..samples {
        while let Some(write) = writes.get(k).filter(|write| write.at <= n) {
            core.write(write.address, write.value);
            k += 1;
        }
        if let Some((address, pan)) = panning.and_then(|seed| common::soft_pan(n, seed)) {
            core.write_pan(address, pan);
        }
        out.push(core.generate());
    }
    out
}

#[test]
fn every_level_makes_the_portable_samples() {
    const SAMPLES: u32 = 20_000;
    let levels = Features::detect().levels();
    for level in LEVELS.iter().filter(|level| !levels.contains(level)) {
        println!("note: this processor has not {level}, whose pass is left out");
    }
    for seed in 1..=8 {
        let writes = common::random_writes(seed, SAMPLES);
        let portable = render(&mut Core::portable(), &writes, SAMPLES, Some(seed));
        for &level in &levels {
            let mut core = Core::at(level).expect("a core at a level the processor has");
            assert_eq!(core.level(), Some(level));
            let samples = render(&mut core, &writes, SAMPLES, Some(seed));
            let differ = portable.iter().zip(&samples).filter(|(a, b)| a != b).count();
            assert_eq!(differ, 0, "{differ} samples of seed {seed} differ at {level}");
        }
    }
    println!(
        "levels whose passes make the portable pass's samples: {}",
        levels.iter().map(ToString::to_string).collect::<Vec<_>>().join(", ")
    );
}

#[test]
fn nothing_written_makes_silence_and_writes_make_sound() {
    let mut quiet = Core::new();
    let mut silent = true;
    for _ in 0..2000 {
        silent &= quiet.generate() == [0, 0];
    }
    assert!(silent);

    let out = render(&mut Core::new(), &common::random_writes(1, 4000), 4000, None);
    assert!(out.iter().any(|&frame| frame != [0, 0]));
}

#[test]
fn soft_panning_at_the_centre_is_the_chip_and_hard_to_a_side_silences_the_other() {
    let writes = common::random_writes(2, 4000);
    let mut plain = Core::new();
    let mut centred = Core::new();
    let mut left = Core::new();
    for ch in 0..18_u16 {
        let address = (ch / 9) * 0x100 + ch % 9;
        centred.write_pan(address, 64);
        left.write_pan(address, 0);
    }
    assert_eq!(
        render(&mut plain, &writes, 4000, None),
        render(&mut centred, &writes, 4000, None)
    );
    let l = render(&mut left, &writes, 4000, None);
    assert!(l.iter().any(|frame| frame[0] != 0));
    assert!(l.iter().all(|frame| frame[1] == 0));
}

#[test]
fn a_core_takes_the_level_it_is_made_at() {
    let core = Core::new();
    println!(
        "a core made without a level takes {}, with the code around its pass at {}",
        Choice { level: core.level() },
        core.tier()
    );
    assert_eq!(core.level(), Choice::new(Features::detect()).level);
    // The portable pass is x86-64's baseline's, and no level's elsewhere:
    // AArch64's baseline has a pass written by hand.
    let portable_level = if cfg!(target_arch = "x86_64") {
        Some(Level::V2)
    } else {
        None
    };
    assert_eq!(Core::portable().level(), portable_level);
    assert_eq!(Core::portable().tier(), Tier::Baseline);
    let features = Features::detect();
    let levels = features.levels();
    let all = [
        Level::V2,
        Level::Avx2,
        Level::Avx2Vnni,
        Level::Avx512,
        Level::Avx512Vnni,
        Level::Avx512Vbmi,
        Level::Avx512VbmiVnni,
        Level::Neon,
    ];
    for level in all {
        let core = Core::at(level);
        assert_eq!(core.is_some(), levels.contains(&level), "{level}");
        assert!(core.is_none_or(|core| core.level() == Some(level) && core.tier() == features.tier(level)));
    }
}
