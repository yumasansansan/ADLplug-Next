// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

//! Assembles the passes written in assembly for the target's architecture
//! into a static library that the crate links: each `.S` of `asm/<arch>/` by
//! Clang, through its preprocessor and `asm/pass.h`, for the target's triple,
//! then the objects archived by `llvm-ar`. The LLVM the project builds with has
//! them: `clang`, `llvm-ar` and `llvm-nm` on `PATH`, or the programs
//! `ADLPLUG_CLANG`, `ADLPLUG_AR` and `ADLPLUG_NM` name. Only x86-64 has passes
//! in assembly, for ELF (Linux) and COFF (Windows); an architecture with none
//! gets no library.
//!
//! The passes are leaf functions that refer to nothing outside their own
//! file, so that an object with a symbol it does not define is a pass the
//! assembler took otherwise than written, its names declared and never
//! defined, which only the link would find. `llvm-nm` lists each object's
//! undefined symbols, and the build stops on any.

use std::path::{Path, PathBuf};
use std::process::Command;

fn main() {
    println!("cargo::rerun-if-changed=asm");
    println!("cargo::rerun-if-env-changed=ADLPLUG_CLANG");
    println!("cargo::rerun-if-env-changed=ADLPLUG_AR");
    println!("cargo::rerun-if-env-changed=ADLPLUG_NM");
    let architecture = std::env::var("CARGO_CFG_TARGET_ARCH").expect("Cargo names the target's architecture");
    let Ok(entries) = std::fs::read_dir(Path::new("asm").join(&architecture)) else {
        return;
    };
    let mut sources: Vec<PathBuf> = entries
        .filter_map(Result::ok)
        .map(|entry| entry.path())
        .filter(|path| path.extension().is_some_and(|extension| extension == "S"))
        .collect();
    if sources.is_empty() {
        return;
    }
    sources.sort();
    let target = std::env::var("TARGET").expect("Cargo names the target");
    let out = PathBuf::from(std::env::var_os("OUT_DIR").expect("Cargo names the output directory"));
    let clang = std::env::var("ADLPLUG_CLANG").unwrap_or_else(|_| "clang".to_owned());
    let archiver = std::env::var("ADLPLUG_AR").unwrap_or_else(|_| "llvm-ar".to_owned());
    let lister = std::env::var("ADLPLUG_NM").unwrap_or_else(|_| "llvm-nm".to_owned());
    let mut objects = Vec::with_capacity(sources.len());
    for source in &sources {
        println!("cargo::rerun-if-changed={}", source.display());
        let stem = source.file_stem().expect("a source's name");
        let object = out.join(stem).with_extension("o");
        run(Command::new(&clang)
            .arg(format!("--target={target}"))
            .args(["-c", "-Iasm", "-Werror"])
            .arg(source)
            .arg("-o")
            .arg(&object));
        let undefined = output(
            Command::new(&lister)
                .args(["--undefined-only", "--format=just-symbols"])
                .arg(&object),
        );
        assert!(
            undefined.trim().is_empty(),
            "{} declares symbols it does not define, as an assembler that took its lines otherwise than written \
             would leave them: {}",
            source.display(),
            undefined.split_whitespace().collect::<Vec<_>>().join(", ")
        );
        objects.push(object);
    }
    // The name MSVC's linker looks for, and the one of every other.
    let library = out.join(if target.ends_with("-msvc") {
        "adlplug_opl3_asm.lib"
    } else {
        "libadlplug_opl3_asm.a"
    });
    if library.exists() {
        std::fs::remove_file(&library).expect("the library of an earlier build removed");
    }
    run(Command::new(&archiver).arg("crs").arg(&library).args(&objects));
    println!("cargo::rustc-link-search=native={}", out.display());
    println!("cargo::rustc-link-lib=static=adlplug_opl3_asm");
}

/// Runs a program of the toolchain and returns what it writes, and stops the
/// build where it fails.
fn output(command: &mut Command) -> String {
    let output = command
        .output()
        .unwrap_or_else(|error| panic!("{command:?} could not run: {error}"));
    assert!(output.status.success(), "{command:?} failed: {}", output.status);
    String::from_utf8_lossy(&output.stdout).into_owned()
}

/// Runs a program of the toolchain, and stops the build where it fails.
fn run(command: &mut Command) {
    let status = command
        .status()
        .unwrap_or_else(|error| panic!("{command:?} could not run: {error}"));
    assert!(status.success(), "{command:?} failed: {status}");
}
