// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! The processor the tests run on: it has what the build is compiled for, the
//! baseline and, for a build for x86-64-v3 or v4, that tier, and the level
//! chosen for it is the most preferred one it has. The levels and the tiers it
//! has are printed, for the log of a run on a machine one does not have at hand.

use adlplug_cpu::{Choice, Features, LEVELS, Tier};

#[test]
fn the_level_chosen_is_the_most_preferred_the_processor_has() {
    let features = Features::detect();
    let levels = features.levels();
    let choice = Choice::new(features);
    let tiers = [Tier::Baseline, Tier::V3, Tier::V4, Tier::Sve, Tier::Sve2]
        .into_iter()
        .filter(|&tier| features.has_tier(tier));
    println!(
        "levels of this processor: {}; chosen: {choice}; tiers: {}; SHA3: {}",
        levels.iter().map(ToString::to_string).collect::<Vec<_>>().join(", "),
        tiers.map(|tier| tier.to_string()).collect::<Vec<_>>().join(", "),
        features.sha3
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

#[test]
#[cfg(target_arch = "x86_64")]
fn the_processor_has_the_tier_the_build_is_compiled_for() {
    // A build for x86-64-v3 or v4 (ci/targets.sh) would not have got this far
    // on a processor without the tier's extensions, which the detection is to
    // find as the compiler names them.
    let features = Features::detect();
    let v3 = cfg!(all(
        target_feature = "avx",
        target_feature = "avx2",
        target_feature = "bmi1",
        target_feature = "bmi2",
        target_feature = "f16c",
        target_feature = "fma",
        target_feature = "lzcnt",
        target_feature = "movbe"
    ));
    let v4 = v3
        && cfg!(all(
            target_feature = "avx512f",
            target_feature = "avx512bw",
            target_feature = "avx512cd",
            target_feature = "avx512dq",
            target_feature = "avx512vl"
        ));
    assert!(!v3 || features.has_tier(Tier::V3));
    assert!(!v4 || features.has_tier(Tier::V4));
}
