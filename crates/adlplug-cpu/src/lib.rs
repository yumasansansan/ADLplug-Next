// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! The processor ADLplug-Next runs on: the extensions of its instruction set
//! that the passes of the emulator cores are written for, and the level of the
//! passes chosen from them.
//!
//! A [`Level`] is a set of extensions that one form of a pass is written for.
//! On x86-64 they are `v2`, the baseline the build is compiled for, which has
//! the portable form alone; `avx2`; `avx2-vnni`, AVX2 with the VEX form of VNNI;
//! and four of AVX-512: its foundation (F, DQ, BW and VL), with VNNI, with VBMI,
//! and with both. On AArch64 there is `neon`, its baseline, which has the
//! portable form alone until the passes in assembly for AArch64 come: they are
//! to be written for Apple's processors first and run on Windows and Linux on
//! AArch64 as well, and the levels of the extensions they take come with them.
//!
//! A processor has some of the levels, and not always every less preferred
//! one: Intel has made processors with AVX-512 VNNI and no VBMI (Cascade Lake,
//! Cooper Lake) and with VBMI and no VNNI (Cannon Lake). So the levels it has are
//! a set, and the level in effect is the most preferred of them. VBMI counts on
//! Intel's processors alone: the forms with it look their tables up by bytes,
//! which waits longer than a lookup by words on AMD's (Zen 4 and 5) and less on
//! Intel's, as the pass for AVX-512 measured it. `ADLPLUG_ISA=<level>` holds the
//! level to that one or a less preferred one, and `ADLPLUG_ISA=v2` to the
//! portable form ([`Request`]).
//!
//! The extensions are those std's `is_x86_feature_detected!` reports, which
//! asks the processor and the system: AVX-512 counts only where the system saves
//! its registers.
#![forbid(unsafe_code)]

use std::fmt;

/// A set of extensions of the instruction set that a form of a pass is written
/// for, named as `ADLPLUG_ISA` names it.
///
/// The levels of an architecture are declared from the least preferred to the
/// most, so that the order of the type is the order of preference.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum Level {
    /// `v2`: x86-64-v2, SSE4.2 and POPCNT, the baseline the build is compiled
    /// for. The portable form alone.
    V2,
    /// `avx2`: AVX2.
    Avx2,
    /// `avx2-vnni`: AVX2 and the VEX form of `vpdpwssd`: Alder Lake, Raptor
    /// Lake, Meteor Lake and after; Zen 5.
    Avx2Vnni,
    /// `avx512`: AVX-512 F, DQ, BW and VL, with the system saving their
    /// registers.
    Avx512,
    /// `avx512-vnni`: AVX-512 and its VNNI.
    Avx512Vnni,
    /// `avx512-vbmi`: AVX-512 and VBMI, on Intel's processors.
    Avx512Vbmi,
    /// `avx512-vbmi-vnni`: AVX-512, VBMI and VNNI, on Intel's processors.
    Avx512VbmiVnni,
    /// `neon`: AArch64's baseline. The portable form alone.
    Neon,
}

/// The levels of the architecture ADLplug-Next is built for, the least
/// preferred first.
#[cfg(target_arch = "x86_64")]
pub const LEVELS: &[Level] = &[
    Level::V2,
    Level::Avx2,
    Level::Avx2Vnni,
    Level::Avx512,
    Level::Avx512Vnni,
    Level::Avx512Vbmi,
    Level::Avx512VbmiVnni,
];
/// The levels of the architecture ADLplug-Next is built for, the least
/// preferred first.
#[cfg(target_arch = "aarch64")]
pub const LEVELS: &[Level] = &[Level::Neon];
/// The levels of the architecture ADLplug-Next is built for: none, on an
/// architecture whose passes are the portable form alone.
#[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64")))]
pub const LEVELS: &[Level] = &[];

/// Every level, of every architecture.
const ALL: [Level; 8] = [
    Level::V2,
    Level::Avx2,
    Level::Avx2Vnni,
    Level::Avx512,
    Level::Avx512Vnni,
    Level::Avx512Vbmi,
    Level::Avx512VbmiVnni,
    Level::Neon,
];

impl Level {
    /// The level's name, as `ADLPLUG_ISA` takes it.
    #[must_use]
    pub const fn name(self) -> &'static str {
        match self {
            Self::V2 => "v2",
            Self::Avx2 => "avx2",
            Self::Avx2Vnni => "avx2-vnni",
            Self::Avx512 => "avx512",
            Self::Avx512Vnni => "avx512-vnni",
            Self::Avx512Vbmi => "avx512-vbmi",
            Self::Avx512VbmiVnni => "avx512-vbmi-vnni",
            Self::Neon => "neon",
        }
    }

    /// The level of a name, of any architecture.
    #[must_use]
    pub fn from_name(name: &str) -> Option<Self> {
        ALL.into_iter().find(|level| level.name() == name)
    }

    /// Whether the level is one of the architecture ADLplug-Next is built for.
    #[must_use]
    pub fn is_native(self) -> bool {
        LEVELS.contains(&self)
    }
}

impl fmt::Display for Level {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(self.name())
    }
}

/// The extensions that the levels need, as the processor and the system report
/// them. The fields of the other architecture stay false.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
#[expect(
    clippy::struct_excessive_bools,
    reason = "one flag for each extension that the processor reports, as std's macros report them"
)]
pub struct Features {
    /// SSE4.2.
    pub sse4_2: bool,
    /// POPCNT.
    pub popcnt: bool,
    /// AVX2.
    pub avx2: bool,
    /// AVX-VNNI, the VEX form of VNNI's instructions.
    pub avx_vnni: bool,
    /// AVX-512 F, with the system saving its registers.
    pub avx512f: bool,
    /// AVX-512 DQ.
    pub avx512dq: bool,
    /// AVX-512 BW.
    pub avx512bw: bool,
    /// AVX-512 VL.
    pub avx512vl: bool,
    /// AVX-512 VNNI.
    pub avx512_vnni: bool,
    /// AVX-512 VBMI.
    pub avx512_vbmi: bool,
    /// Whether the processor is one of Intel's: what CPUID's first leaf names
    /// as its maker is `GenuineIntel`.
    pub intel: bool,
    /// NEON (Advanced SIMD).
    pub neon: bool,
}

impl Features {
    /// The extensions of the processor this runs on.
    #[must_use]
    pub fn detect() -> Self {
        #[cfg(target_arch = "x86_64")]
        {
            use std::arch::is_x86_feature_detected;
            Self {
                sse4_2: is_x86_feature_detected!("sse4.2"),
                popcnt: is_x86_feature_detected!("popcnt"),
                avx2: is_x86_feature_detected!("avx2"),
                avx_vnni: is_x86_feature_detected!("avxvnni"),
                avx512f: is_x86_feature_detected!("avx512f"),
                avx512dq: is_x86_feature_detected!("avx512dq"),
                avx512bw: is_x86_feature_detected!("avx512bw"),
                avx512vl: is_x86_feature_detected!("avx512vl"),
                avx512_vnni: is_x86_feature_detected!("avx512vnni"),
                avx512_vbmi: is_x86_feature_detected!("avx512vbmi"),
                intel: is_intel(),
                ..Self::default()
            }
        }
        #[cfg(target_arch = "aarch64")]
        {
            Self {
                neon: std::arch::is_aarch64_feature_detected!("neon"),
                ..Self::default()
            }
        }
        #[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64")))]
        {
            Self::default()
        }
    }

    /// Whether the extensions include every one that the level needs.
    #[must_use]
    pub const fn has(self, level: Level) -> bool {
        let avx512 = self.avx2 && self.avx512f && self.avx512dq && self.avx512bw && self.avx512vl;
        let vbmi = self.avx512_vbmi && self.intel;
        match level {
            Level::V2 => self.sse4_2 && self.popcnt,
            Level::Avx2 => self.avx2,
            Level::Avx2Vnni => self.avx2 && self.avx_vnni,
            Level::Avx512 => avx512,
            Level::Avx512Vnni => avx512 && self.avx512_vnni,
            Level::Avx512Vbmi => avx512 && vbmi,
            Level::Avx512VbmiVnni => avx512 && vbmi && self.avx512_vnni,
            Level::Neon => self.neon,
        }
    }

    /// The levels of the architecture that the extensions include, the least
    /// preferred first: those the tests run every pass at.
    #[must_use]
    pub fn levels(self) -> Vec<Level> {
        LEVELS.iter().copied().filter(|&level| self.has(level)).collect()
    }
}

/// Whether CPUID's first leaf names Intel as the processor's maker: the twelve
/// bytes of EBX, EDX and ECX, in that order, read `GenuineIntel`.
#[cfg(target_arch = "x86_64")]
fn is_intel() -> bool {
    let leaf = std::arch::x86_64::__cpuid(0);
    let mut maker = [0_u8; 12];
    maker[..4].copy_from_slice(&leaf.ebx.to_le_bytes());
    maker[4..8].copy_from_slice(&leaf.edx.to_le_bytes());
    maker[8..].copy_from_slice(&leaf.ecx.to_le_bytes());
    &maker == b"GenuineIntel"
}

/// What the environment asks of the passes: a limit to their level
/// (`ADLPLUG_ISA`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct Request {
    /// The most preferred level that may be taken; any, where there is none.
    pub limit: Option<Level>,
}

/// A request that the environment makes and that cannot be met as it is.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    /// `ADLPLUG_ISA` names no level.
    UnknownLevel(String),
    /// `ADLPLUG_ISA` names a level of another architecture.
    OtherArchitecture(Level),
}

impl fmt::Display for Error {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        let names = LEVELS.iter().map(|level| level.name()).collect::<Vec<_>>().join(", ");
        match self {
            Self::UnknownLevel(name) => {
                write!(formatter, "ADLPLUG_ISA={name} names no level; the levels are {names}")
            }
            Self::OtherArchitecture(level) => write!(
                formatter,
                "ADLPLUG_ISA={level} is a level of another architecture; the levels of this one are {names}"
            ),
        }
    }
}

impl std::error::Error for Error {}

impl Request {
    /// The request of `ADLPLUG_ISA`, of which an empty value is the same as
    /// none.
    ///
    /// # Errors
    ///
    /// Where `ADLPLUG_ISA` names no level of this architecture.
    pub fn from_environment() -> Result<Self, Error> {
        let isa = std::env::var("ADLPLUG_ISA").ok();
        Self::parse(isa.as_deref())
    }

    /// The request of a value of `ADLPLUG_ISA`.
    ///
    /// # Errors
    ///
    /// As [`Request::from_environment`].
    pub fn parse(isa: Option<&str>) -> Result<Self, Error> {
        let limit = match isa.map(str::trim).filter(|name| !name.is_empty()) {
            None => None,
            Some(name) => {
                let level = Level::from_name(name).ok_or_else(|| Error::UnknownLevel(name.to_owned()))?;
                if !level.is_native() {
                    return Err(Error::OtherArchitecture(level));
                }
                Some(level)
            }
        };
        Ok(Self { limit })
    }
}

/// The level of the passes in effect: the most preferred level that the
/// processor has and the request allows; none on an architecture without
/// levels, where the passes are the portable form alone.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Choice {
    /// The level the passes are taken at.
    pub level: Option<Level>,
}

impl Choice {
    /// The choice for a processor's extensions and a request.
    #[must_use]
    pub fn new(features: Features, request: Request) -> Self {
        let level = LEVELS
            .iter()
            .copied()
            .rfind(|&level| features.has(level) && request.limit.is_none_or(|limit| level <= limit));
        Self { level }
    }
}

impl fmt::Display for Choice {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.level {
            Some(level) => write!(formatter, "{level}"),
            None => formatter.write_str("portable"),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn names_go_back_to_their_levels() {
        for level in ALL {
            assert_eq!(Level::from_name(level.name()), Some(level));
        }
        assert_eq!(Level::from_name("avx512vnni"), None);
        assert_eq!(Level::from_name("AVX2"), None);
    }

    #[test]
    fn the_order_of_the_type_is_the_order_of_preference() {
        let mut sorted = LEVELS.to_vec();
        sorted.sort_unstable();
        assert_eq!(sorted, LEVELS);
    }

    #[test]
    fn a_request_names_a_level_of_this_architecture_or_fails() {
        assert_eq!(Request::parse(None), Ok(Request::default()));
        assert_eq!(Request::parse(Some(" ")), Ok(Request::default()));
        assert_eq!(
            Request::parse(Some("sse9")),
            Err(Error::UnknownLevel("sse9".to_owned()))
        );
        for &level in LEVELS {
            assert_eq!(Request::parse(Some(level.name())), Ok(Request { limit: Some(level) }));
        }
        for level in ALL.into_iter().filter(|level| !level.is_native()) {
            assert_eq!(Request::parse(Some(level.name())), Err(Error::OtherArchitecture(level)));
        }
    }

    /// The extensions of a processor with every one of x86-64's, Intel's.
    #[cfg(target_arch = "x86_64")]
    fn everything() -> Features {
        Features {
            sse4_2: true,
            popcnt: true,
            avx2: true,
            avx_vnni: true,
            avx512f: true,
            avx512dq: true,
            avx512bw: true,
            avx512vl: true,
            avx512_vnni: true,
            avx512_vbmi: true,
            intel: true,
            neon: false,
        }
    }

    #[test]
    #[cfg(target_arch = "x86_64")]
    fn the_most_preferred_level_the_processor_has_and_the_request_allows() {
        let all = everything();
        assert_eq!(Choice::new(all, Request::default()).level, Some(Level::Avx512VbmiVnni));
        for &limit in LEVELS {
            assert_eq!(Choice::new(all, Request { limit: Some(limit) }).level, Some(limit));
        }

        // VBMI on AMD's processors does not count, and VNNI without VBMI does.
        let amd = Features { intel: false, ..all };
        assert_eq!(Choice::new(amd, Request::default()).level, Some(Level::Avx512Vnni));
        assert!(!amd.has(Level::Avx512Vbmi));

        // VBMI without VNNI, as Cannon Lake has it.
        let cannon_lake = Features {
            avx512_vnni: false,
            avx_vnni: false,
            ..all
        };
        assert_eq!(
            Choice::new(cannon_lake, Request::default()).level,
            Some(Level::Avx512Vbmi)
        );

        // AVX-VNNI without AVX-512, as Alder Lake has it.
        let alder_lake = Features { avx512f: false, ..all };
        assert_eq!(Choice::new(alder_lake, Request::default()).level, Some(Level::Avx2Vnni));
        assert_eq!(alder_lake.levels(), [Level::V2, Level::Avx2, Level::Avx2Vnni]);

        // Nothing but the baseline, and less than it.
        let baseline = Features {
            sse4_2: true,
            popcnt: true,
            ..Features::default()
        };
        assert_eq!(Choice::new(baseline, Request::default()).level, Some(Level::V2));
        assert_eq!(Choice::new(Features::default(), Request::default()).level, None);
        assert_eq!(
            Choice::new(Features::default(), Request::default()).to_string(),
            "portable"
        );
    }
}
