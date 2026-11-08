// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! The processor the tests run on: it has the baseline the build is compiled
//! for, and the level chosen for it is the most preferred one it has. The
//! levels it has are printed, for the log of a run on a machine one does not
//! have at hand.

use adlplug_cpu::{Choice, Features, LEVELS};

#[test]
fn the_level_chosen_is_the_most_preferred_the_processor_has() {
    let features = Features::detect();
    let levels = features.levels();
    let choice = Choice::new(features);
    println!(
        "levels of this processor: {}; chosen: {choice}",
        levels.iter().map(ToString::to_string).collect::<Vec<_>>().join(", ")
    );
    assert_eq!(choice.level, levels.last().copied());
    for level in &levels {
        assert!(LEVELS.contains(level));
        assert!(features.has(*level));
    }
}

#[test]
#[cfg(target_arch = "x86_64")]
fn the_processor_has_the_baseline_the_build_is_for() {
    // The build is for x86-64-v2 (.cargo/config.toml); it would not have got
    // this far on a processor without it.
    assert!(Features::detect().has(adlplug_cpu::Level::V2));
}
