<!--
SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later

This file is part of ADLplug-Next. The SPDX lines name its copyright holder and
its license, in the machine-readable form of the REUSE specification: the GNU
General Public License, version 3 or any later version
(LICENSES/GPL-3.0-or-later.txt).
-->

# ADLplug-Next in Rust

English | [日本語](README.ja.md)

This branch is ADLplug-Next written again from the beginning in Rust. It cannot
be used yet: the plugins that work are those of the C++ version, on the main
branch, which moves to a branch named 2.2 when this one takes main's place.

What it is to be:

- the two plugins, ADLplug-Next (OPL3) and OPNplug-Next (OPN2), with emulator
  cores of their own (the parts that recreate the chips in software) that come
  out bit for bit as the emulations drawn from the chips' circuits do, and
  nothing of the libraries the C++ version was built on;
- compatible with the C++ version: the same plugin and parameter IDs, so that a
  host's project opens with this version in place of that one, and the projects
  and banks it saved read as they were;
- Safe Rust (the part of Rust whose memory safety the compiler guarantees)
  wherever it can be; where it cannot -- the calls into the operating systems,
  and the parts written in assembly -- each unsafe operation stands alone and
  says why it is sound;
- nothing from crates.io (Rust's package registry), with what the operating
  systems offer declared by hand;
- for x86-64 processors from x86-64-v2 on, taking AVX2 and AVX-512 where the
  processor has them, and for AArch64: Apple's processors, and Windows and Linux
  on AArch64.

## Building and testing

You need Rust, the latest stable release (rust-toolchain.toml names it), and
LLVM: Clang, LLD and the LLVM tools (llvm-ar, llvm-nm) on PATH. Then:

```
cargo test --workspace --locked
```

`ci/rust.sh` runs what the CI runs: the layout check (rustfmt), clippy with
every warning an error, and the tests in the dev and the release profile.

## License

ADLplug-Next is under the GNU General Public License, version 3 or any later
version (LICENSES/GPL-3.0-or-later.txt). The images, icons, fonts and manuals
under resources/ and docs/ come with licenses of their own, which REUSE.toml
names.
