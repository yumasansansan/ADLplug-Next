// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! The core makes the C++ version's core's samples, which are the chip's: for
//! the random writes of the C++ version's tests, at every level of the passes
//! this processor has, every 1000 samples hash as `data/cpp_core.txt` says the C++
//! core's did. The C++ core made the low-level core's samples (YMF262-LLE) for
//! all of the writes without soft panning, so these are the chip's as well.

mod common;

use adlplug_cpu::Features;
use adlplug_opl3::Core;

/// What `data/cpp_core.txt` holds for a seed.
#[derive(Debug)]
struct Expected {
    seed: u32,
    writes: usize,
    writes_hash: u64,
    plain: Vec<u64>,
    panned: Vec<u64>,
}

const SAMPLES: u32 = 20000;
const BLOCK: u32 = 1000;

const FNV_BASIS: u64 = 14_695_981_039_346_656_037;

fn fnv(mut hash: u64, bytes: &[u8]) -> u64 {
    for &byte in bytes {
        hash ^= u64::from(byte);
        hash = hash.wrapping_mul(1_099_511_628_211);
    }
    hash
}

fn hashes(words: &[&str]) -> Vec<u64> {
    words
        .iter()
        .map(|word| u64::from_str_radix(word, 16).expect("a hash in hexadecimal"))
        .collect()
}

/// The lines of `data/cpp_core.txt`, read by hand: its comments left out, and
/// three lines a seed.
fn expected() -> Vec<Expected> {
    let mut seeds: Vec<Expected> = Vec::new();
    for line in include_str!("data/cpp_core.txt").lines() {
        if line.starts_with('#') || line.trim().is_empty() {
            continue;
        }
        let words: Vec<&str> = line.split_whitespace().collect();
        let seed: u32 = words[1].parse().expect("a seed");
        match words[0] {
            "writes" => seeds.push(Expected {
                seed,
                writes: words[2].parse().expect("a count of writes"),
                writes_hash: hashes(&words[3..4])[0],
                plain: Vec::new(),
                panned: Vec::new(),
            }),
            "plain" | "panned" => {
                let last = seeds.last_mut().expect("the seed's writes come first");
                assert_eq!(last.seed, seed, "the lines of a seed are together");
                let blocks = hashes(&words[2..]);
                assert_eq!(blocks.len(), usize::try_from(SAMPLES / BLOCK).expect("a count"));
                if words[0] == "plain" {
                    last.plain = blocks;
                } else {
                    last.panned = blocks;
                }
            }
            other => panic!("data/cpp_core.txt has a line of kind {other}"),
        }
    }
    assert_eq!(seeds.len(), 8, "eight seeds");
    seeds
}

/// The hashes of every 1000 samples the core makes of a seed's writes, soft
/// panned or not.
fn block_hashes(core: &mut Core, seed: u32, panned: bool) -> Vec<u64> {
    let writes = common::random_writes(seed, SAMPLES);
    let mut next = writes.iter().peekable();
    let mut blocks = Vec::new();
    let mut hash = FNV_BASIS;
    for n in 0..SAMPLES {
        while let Some(write) = next.next_if(|write| write.at <= n) {
            core.write(write.address, write.value);
        }
        if panned && let Some((address, pan)) = common::soft_pan(n, seed) {
            core.write_pan(address, pan);
        }
        for value in core.generate() {
            hash = fnv(hash, &value.to_le_bytes());
        }
        if (n + 1) % BLOCK == 0 {
            blocks.push(hash);
            hash = FNV_BASIS;
        }
    }
    blocks
}

/// Checks a core of one level against the C++ core, seed by seed, and says at
/// which 1000 samples it first differs.
fn check(make: &dyn Fn() -> Core, what: &str) {
    for expected in expected() {
        for (panned, reference) in [(false, &expected.plain), (true, &expected.panned)] {
            let mut core = make();
            let ours = block_hashes(&mut core, expected.seed, panned);
            if let Some(block) = ours.iter().zip(reference.iter()).position(|(a, b)| a != b) {
                panic!(
                    "{what}, seed {}{}: samples {} to {} differ from the C++ core's",
                    expected.seed,
                    if panned { ", soft panned" } else { "" },
                    u32::try_from(block).expect("a block") * BLOCK,
                    (u32::try_from(block).expect("a block") + 1) * BLOCK - 1
                );
            }
        }
    }
}

#[test]
fn the_writes_are_those_of_the_cpp_tests() {
    for expected in expected() {
        let writes = common::random_writes(expected.seed, SAMPLES);
        let mut hash = FNV_BASIS;
        for write in &writes {
            hash = fnv(hash, &write.at.to_le_bytes());
            hash = fnv(hash, &write.address.to_le_bytes());
            hash = fnv(hash, &[write.value]);
        }
        assert_eq!(
            writes.len(),
            expected.writes,
            "seed {}: the number of writes",
            expected.seed
        );
        assert_eq!(hash, expected.writes_hash, "seed {}: the writes", expected.seed);
    }
}

#[test]
fn the_portable_pass_makes_the_cpp_cores_samples() {
    check(&Core::portable, "the portable pass");
}

#[test]
fn every_level_this_processor_has_makes_the_cpp_cores_samples() {
    for level in Features::detect().levels() {
        let make = || Core::at(level).expect("a level the processor has");
        check(&make, &format!("the pass at level {level}"));
    }
}
