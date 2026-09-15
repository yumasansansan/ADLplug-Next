<!--
SPDX-FileCopyrightText: 2018-2021 Jean Pierre Cimalando
SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
SPDX-License-Identifier: BSL-1.0 AND GPL-3.0-or-later

This file comes from ADLplug and was modified for ADLplug-Next. ADLplug gave
it no notice of its own; it was under ADLplug's license, the Boost Software
License 1.0 (LICENSES/BSL-1.0.txt). The SPDX lines name the copyright holders
and licenses in the machine-readable form of the REUSE specification:
ADLplug's part is under the Boost Software License 1.0, and ADLplug-Next's
changes are under the GNU General Public License, version 3 or any later
version (LICENSES/GPL-3.0-or-later.txt).
-->

# ADLplug-Next
Synthesizer plugins that recreate the Yamaha FM sound chips of the 1980s and 1990s (VST3, LV2, AU and AAX; they also run standalone)

English | [日本語](README.ja.md)

![screenshot](docs/screen.png)

## Introduction

This software package provides FM synthesizer plugins, ADLplug-Next and OPNplug-Next, that recreate the [OPL3](https://en.wikipedia.org/wiki/Yamaha_YMF262) and [OPN2](https://en.wikipedia.org/wiki/Yamaha_YM2612) sound chips. It is a modified version of [ADLplug](https://github.com/jpcima/ADLplug), which Jean Pierre Cimalando developed.  
The emulations and the drivers are provided by [libADLMIDI](https://github.com/Wohlstand/libADLMIDI) and [libOPNMIDI](https://github.com/Wohlstand/libOPNMIDI).

- [x] control of multiple emulated (recreated in software) YMF262/YM2612 chips
- [x] high-fidelity emulation, with a choice between good fidelity at high speed and excellent fidelity at low speed
- [x] synthesis of melodic and percussive instruments
- [x] extensible polyphony
- [x] bundled collection of instruments
- [x] support for dynamic parameterization and automation
- [x] rigorous implementation of the MIDI standard
- [x] multi-channel operation with General MIDI compatibility
- [x] ability to synthesize entire MIDI files out of the box

ADLplug-Next: DyTect ([yumasansansan](https://github.com/yumasansansan) on GitHub)  
Upstream ADLplug (the original that ADLplug-Next modifies): [Jean Pierre Cimalando](https://github.com/jpcima), author; contributors [Olivier Humbert](https://github.com/trebmuh), [Christopher Arndt](https://github.com/SpotlightKid), [Bruce Sutherland](https://github.com/bsutherland), [David Runge](https://github.com/dvzrv), [Jérémy Frey](https://github.com/jfrey-xx)

DyTect is the artist and engineer name of Yuma Kakei, who is yumasansansan on
GitHub. Yuma Kakei is the real name, which the KDE project knows too, and
which the copyright lines of ADLplug-Next's code carry.

## Development builds

[![CI](https://github.com/yumasansansan/ADLplug-Next/actions/workflows/ci.yml/badge.svg)](https://github.com/yumasansansan/ADLplug-Next/actions/workflows/ci.yml)

Every push and pull request to the repository is built by GitHub Actions
(GitHub's service that builds and tests automatically) on Windows, Linux and
macOS, in Debug and Release. For x86-64 (64-bit Intel and AMD CPUs), two
builds are made: one that runs on every x86-64 CPU (baseline), and one for
CPUs with AVX2.

A push to the `main` branch also packages the Release builds, and builds the
rpm packages and the builds that leave some emulator cores (the programs that
recreate the sound chips) out. When every job has passed, the
[Nightly](https://github.com/yumasansansan/ADLplug-Next/releases/tag/nightly)
pre-release (the latest development version, ahead of a proper release) is
replaced. Archives for each system are made, and packages for Ubuntu and for
RHEL, AlmaLinux and openSUSE (see [Installing](#installing)).

Until the first release, the versions are 1.99.N, where N counts the commits
of the `main` branch since the last commit of upstream ADLplug. The plugins
show the time of the commit, in UTC, and its hash as well:
`1.99.N+YYYYMMDD.HHMM.git<hash>`. An odd minor number marks a development
version.

## Installing

Each archive of the
[Nightly](https://github.com/yumasansansan/ADLplug-Next/releases/tag/nightly)
pre-release (zip, tar.xz) has both plugins, ADLplug-Next and OPNplug-Next.
The builds with `avx2`, `amd64v3` or `x86_64_v3` in their names need a CPU
with AVX2 (x86-64-v3); as of 2026, most CPUs from about the last ten years
have it. On a CPU without it, a DAW or other host (the software that loads
plugins) crashes, even while it scans for plugins.

- Windows 11 or later (x86-64): extract the zip archive. The plugins and the
  standalone programs need the
  [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)
  for x64. Copy each plugin, a folder whose name ends in `.vst3` or the like,
  to:
  - `.vst3`: `C:\Program Files\Common Files\VST3`
  - `.lv2`: `C:\Program Files\Common Files\LV2` or `%APPDATA%\LV2`
  - `.aaxplugin`: `C:\Program Files\Common Files\Avid\Audio\Plug-Ins`
- macOS 26 or later (Apple Silicon): extract the zip archive. The files are
  not notarized (checked for malware by Apple), so macOS may stop them from
  loading. In Terminal, run `xattr -dr com.apple.quarantine <extracted folder>`
  to remove the quarantine attribute that the download put on them (the mark
  of a file from the internet). Copy the plugins to:
  - `.vst3`: `~/Library/Audio/Plug-Ins/VST3`
  - `.component` (AU): `~/Library/Audio/Plug-Ins/Components`
  - `.lv2`: `~/Library/Audio/Plug-Ins/LV2`
  - `.aaxplugin`: `/Library/Application Support/Avid/Audio/Plug-Ins`
- Ubuntu 26.04 or later: install the deb packages, for example
  `sudo apt install ./adlplug-next_*_amd64.deb`. The `amd64v3` packages are
  the AVX2 builds; install them only on a CPU with AVX2.
- RHEL 10 or later, AlmaLinux 10 or later, and openSUSE: install the rpm
  packages, for example `sudo dnf install ./adlplug-next-*.x86_64.rpm` or
  `sudo zypper install ./adlplug-next-*.x86_64.rpm`. The `x86_64_v3`
  packages are the AVX2 builds; install them only on a CPU with AVX2.
- Other Linux systems (x86-64): extract the tar.xz archive, and copy the
  `.vst3` folders to `~/.vst3` and the `.lv2` folders to `~/.lv2`.

On Linux, the user interface is shown with X11 (through Xwayland in a Wayland
session). The AAX plugins have no PACE signature, so only Pro Tools Developer
loads them. Each archive and package has the licenses of the plugins, and
`<plugin name>-banks.txt` (such as `ADLplug-Next-banks.txt`) with the terms of
their instrument banks (sets of instruments).

## Coming from ADLplug

ADLplug-Next and OPNplug-Next are plugins separate from ADLplug and OPNplug.
They have different names, maker and identifiers, so both can be installed.
- Their VST3 plugins stay compatible with the VST2 and VST3 plugins of ADLplug
  and OPNplug: in a DAW or other host that supports this, a project that used
  those opens with ADLplug-Next or OPNplug-Next as it is.
- In AU hosts (Logic Pro and others) and LV2 hosts, they are different
  plugins.
- When ADLplug-Next or OPNplug-Next has no settings of its own yet, it reads
  those of ADLplug or OPNplug (the keyboard layout and the last directory of
  instruments), and saves to its own.

## Useful links

- User Manual of ADLplug : [English :us:](http://jpcima.sdf1.org/software/documentation/ADLplug/manual/en/manual.html) [French :fr:](http://jpcima.sdf1.org/software/documentation/ADLplug/manual/fr/manual.html)
- Packages of upstream ADLplug:
  - LibraZiK-2 : [ADLplug :fr:](https://librazik.tuxfamily.org/doc2/logiciels/adlplug) [OPNplug :fr:](https://librazik.tuxfamily.org/doc2/logiciels/opnplug)
  - Fedora Copr : [ycollet/linuxmao](https://copr.fedorainfracloud.org/coprs/ycollet/linuxmao/)
  - Arch Linux AUR : [adlplug-git](https://aur.archlinux.org/packages/adlplug-git/) and [opnplug-git](https://aur.archlinux.org/packages/opnplug-git/)
- Bank editor software (to edit instrument banks) : [OPL3](https://github.com/Wohlstand/OPL3BankEditor) and [OPN2](https://github.com/Wohlstand/OPN2BankEditor)

## FM core characteristics

An emulator core (a core below) is a program that recreates a sound chip in
software. For the same chip, the cores differ in how accurate they sound and
how heavy they are to run, and the plugins switch between them in their
emulator menu.

ADLplug-Next builds every emulator core that libADLMIDI and libOPNMIDI
provide, and every core but the ones the plugins need can be left out with a
build option (see [Build instructions](#build-instructions)). The names are
those of the plugins' emulator menu, and the notes summarise what the
libraries and the cores document about themselves.

*Speed* is how many times faster than real time a core played nine notes on
one chip at 44.1 kHz, built with `-O3`, on an Intel Core i7-1360P: at 100×,
one second of sound takes 0.01 seconds to make. An instance (one plugin loaded
in a DAW) runs two chips by default. With *full panning*, a sound can sit
anywhere between left and right; without it, only on the left, in the centre
or on the right. The OPL2 is mono, with no left or right at all.

The low-level (LLE) cores recreate a chip's circuits as read from photos of
its inside (die shots). They are the most faithful, and so heavy that most
computers cannot play them in real time: use them when you render (write the
sound to a file) in a DAW.

**ADLplug-Next**

| Core                             | Chip           | Notes                                                                                                  | Speed | Full panning | Build option                  |
|----------------------------------|----------------|--------------------------------------------------------------------------------------------------------|------:|--------------|-------------------------------|
| DOSBox 0.74-r4111 OPL3 (default) | OPL3 (YMF262)  | Accurate and fast, per libADLMIDI                                                                      |  346× | yes          | `USE_DOSBOX_EMULATOR`, needed |
| Nuked OPL3 (v 1.8)               | OPL3 (YMF262)  | Very accurate, and needs more CPU power, per libADLMIDI                                                |   57× | yes          | `USE_NUKED_EMULATOR`          |
| Nuked OPL3 Fast (by tgies)       | OPL3 (YMF262)  | A faster derivative (fork) of Nuked OPL3 with bit-identical output; replaces Nuked OPL3 1.7.4          |   73× | yes          | `USE_NUKED_EMULATOR`          |
| YMFM OPL3                        | OPL3 (YMF262)  | Aims to be indistinguishable by ear rather than exact to the bit, at a reasonable speed, per ymfm      |  101× | no           | `USE_YMFM_EMULATOR`           |
| Opal OPL3                        | OPL3 (YMF262)  | Inaccurate, per libADLMIDI; written for Reality Adlib Tracker tunes, and has no percussion mode        |  109× | yes          | `USE_OPAL_EMULATOR`           |
| Java 1.0.6 OPL3                  | OPL3 (YMF262)  | Partly accurate, per libADLMIDI                                                                        |   76× | yes          | `USE_JAVA_EMULATOR`           |
| YMF262-LLE OPL3                  | OPL3 (YMF262)  | Low-level; too heavy for ordinary CPUs, per libADLMIDI                                                 |  1.7× | no           | `USE_NUKED_OPL3_LLE_EMULATOR` |
| DOSBox 0.74-r4111 OPL2           | OPL2 (YM3812)  | DOSBox's core run as an OPL2                                                                           |  515× | mono         | `USE_DOSBOX_EMULATOR`         |
| MAME OPL2                        | OPL2 (YM3812)  | MAME's YM3812 core                                                                                     |  214× | mono         | `USE_MAME_EMULATOR`           |
| YMFM OPL2                        | OPL2 (YM3812)  | As YMFM OPL3                                                                                           |  184× | mono         | `USE_YMFM_EMULATOR`           |
| Nuked OPL2 Lite                  | OPL2 (YM3812)  | By Nuke.YKT, version 0.9 beta                                                                          |   88× | mono         | `USE_NUKED_EMULATOR`          |
| YM3812-LLE OPL2                  | OPL2 (YM3812)  | As YMF262-LLE                                                                                          |  3.5× | mono         | `USE_NUKED_OPL2_LLE_EMULATOR` |
| ESFMu                            | ESFM (ESS)     | ESS's extended OPL3 clone, emulated on the basis of Nuked OPL3; libADLMIDI plays it as an OPL3         |  9.7× | yes          | `USE_ESFMU_EMULATOR`          |
| Nuked CQM                        | CQM (Creative) | Creative's OPL3 clone chip, by Nuke.YKT, version 0.9 beta                                              |   22× | no           | `USE_NUKED_EMULATOR`          |

**OPNplug-Next**

| Core                     | Chip           | Notes                                                                                                   | Speed | Full panning | Build option                  |
|--------------------------|----------------|---------------------------------------------------------------------------------------------------------|------:|--------------|-------------------------------|
| MAME YM2612 (default)    | OPN2 (YM2612)  | Accurate, and fast even on slow devices, per libOPNMIDI                                                 |  247× | yes          | `USE_MAME_EMULATOR`, needed   |
| Nuked OPN2 (2612)        | OPN2 (YM2612)  | Very accurate, and needs a very powerful CPU, per libOPNMIDI                                            |   21× | yes          | `USE_NUKED_EMULATOR`          |
| Nuked OPN2 (3438)        | OPN2C (YM3438) | The same core as a YM3438                                                                               |   21× | yes          | `USE_NUKED_EMULATOR`          |
| GENS/GS II OPN2          | OPN2 (YM2612)  | The fastest, but very outdated and inaccurate, per libOPNMIDI                                           |  321× | yes          | `USE_GENS_EMULATOR`           |
| YMFM OPN2                | OPN2 (YM2612)  | As YMFM OPL3                                                                                            |  107× | yes          | `USE_YMFM_EMULATOR`           |
| YM2612-LLE OPN2          | OPN2 (YM2612)  | Low-level; very accurate, and so heavy that slow machines can only render with it, per libOPNMIDI       |  1.4× | no           | `USE_NUKED_OPN2_LLE_EMULATOR` |
| YM3438-LLE OPN2          | OPN2C (YM3438) | As YM2612-LLE                                                                                           |  2.3× | no           | `USE_NUKED_OPN2_LLE_EMULATOR` |
| YMF276-LLE OPN2          | OPN2L (YMF276) | As YM2612-LLE                                                                                           |  2.0× | no           | `USE_NUKED_OPN2_LLE_EMULATOR` |
| MAME YM2608              | OPNA (YM2608)  | Accurate, and fast even on slow devices, per libOPNMIDI                                                 |  175× | yes          | `USE_MAME_2608_EMULATOR`      |
| Neko Project II Kai OPNA | OPNA (YM2608)  | Partly accurate, and fast on slow devices, per libOPNMIDI; its SSG-EG is an experimental libOPNMIDI addition |  325× | yes          | `USE_NP2_EMULATOR`            |
| YMFM OPNA                | OPNA (YM2608)  | As YMFM OPL3                                                                                            |   66× | no           | `USE_YMFM_EMULATOR`           |
| YM2608-LLE OPNA          | OPNA (YM2608)  | As YM2612-LLE                                                                                           |  0.3× | no           | `USE_NUKED_OPNA_LLE_EMULATOR` |

## Build instructions

This section is about building the plugins from their source code. To use the
plugins that are distributed, see [Installing](#installing).

ADLplug-Next supports Windows 11 or later (x86-64), Ubuntu 26.04 or later
(x86-64) and macOS 26 or later (Apple Silicon). On Linux the user interface
runs on X11, which means XWayland in a Wayland session: JUCE (the framework
the plugins are made with) has no Wayland backend.

Install the required dependencies (the software the build needs):
- CMake 3.29 or newer, and Ninja
- LLVM 19 or newer, for C23 / C++23: Clang, LLD, `llvm-ar`, `llvm-ranlib`,
  on Windows `llvm-rc`, and for profile-guided optimisation `llvm-profdata`
  and the profile runtime of compiler-rt (`libclang-rt-<version>-dev`), all
  of one version. The configuration checks this before anything is built.
  GCC, GNU binutils and MSVC's `cl.exe` and `link.exe` are not supported. The
  CI (the automated builds on GitHub Actions) builds with LLVM 23, which is
  recommended.
- Windows: the MSVC libraries and the Windows SDK, which come with Visual
  Studio (Desktop development with C++); Visual Studio 2026 is recommended
- Linux: development packages for ALSA, FreeType, Fontconfig and X11; on
  Ubuntu, `libasound2-dev libfontconfig1-dev libfreetype-dev libx11-dev
  libxcomposite-dev libxcursor-dev libxext-dev libxi-dev libxinerama-dev
  libxrandr-dev libxrender-dev`
- macOS: Xcode, for the SDK, and LLVM's Clang and LLD, not Apple's that come
  with Xcode. The plugins use the libc++ of macOS, so compile with the SDK's
  libc++ headers, not the ones that come with LLVM (`ci/setup.sh` shows how
  the CI sets this up)

### Compiling

Run these commands to build ADLplug-Next:

```
git clone --recursive https://github.com/yumasansansan/ADLplug-Next.git
cd ADLplug-Next
cmake --preset adl-release     # opn-release to build OPNplug-Next; *-relwithdebinfo and *-debug are for debugging
cmake --build --preset adl-release     # likewise
```

The presets in `CMakePresets.json` (ready-made build settings) work on
Windows, Linux and macOS alike. They select the LLVM toolchain, such as Clang
and LLD (`CMAKE_LINKER_TYPE=LLD`), and Ninja. Their versions are not fixed,
but LLVM 23 is recommended. Pass the options below to `cmake` to customise
the build.

An option chooses whether ADLplug-Next or OPNplug-Next is built:
- to build the OPL3 variant (ADLplug-Next), set the option `ADLplug_CHIP` to
  `OPL3`;
- to build the OPN2 variant (OPNplug-Next), set the option `ADLplug_CHIP` to
  `OPN2`.

| Build option                    | Default                                | Description                                                                                   |
| ------------------------------- | -------------------------------------- | --------------------------------------------------------------------------------------------- |
| -DADLplug_VST3=ON/OFF           | ON                                     | Build a VST3 plugin                                                                           |
| -DADLplug_LV2=ON/OFF            | ON                                     | Build a LV2 plugin                                                                            |
| -DADLplug_AU=ON/OFF             | ON on macOS, OFF elsewhere             | Build an Audio Unit (for Logic Pro; macOS only)                                               |
| -DADLplug_AAX=ON/OFF            | ON on Windows and macOS, OFF elsewhere | Build an AAX plugin (needs PACE signing to load in Pro Tools)                                 |
| -DADLplug_Standalone=ON/OFF     | ON                                     | Build a standalone program                                                                    |
| -DADLplug_ASIO=ON/OFF           | ON on Windows, OFF elsewhere           | Enable ASIO in the standalone (Windows only)                                                  |
| -DADLplug_CHIP=OPL3/OPN2        | OPL3 (the opn-* presets set OPN2)      | Choose ADLplug-Next (OPL3) or OPNplug-Next (OPN2)                                             |
| -DADLplug_GREYZONE_BANKS=ON/OFF | OFF                                    | Include the banks of the grey zone (see below)                                                |
| -DADLplug_ARCH=baseline/avx2    | baseline                               | x86-64 instruction set: baseline (every x86-64 CPU) or avx2 (CPUs with AVX2)                  |
| -DADLplug_PGO=ON/OFF            | ON                                     | Profile-guided optimisation of Release builds (see below)                                     |
| -DADLplug_ASSERTIONS=ON/OFF     | OFF                                    | Enable assertions (internal consistency checks) in any build type (Debug, Release and others) |
| -DADLplug_WERROR=ON/OFF         | OFF (the presets set ON)               | Treat warnings in ADLplug-Next's own code as errors                                           |
| -DADLplug_BUILD_TOOLS=ON/OFF    | OFF                                    | Build developer tools (a tool that loads the VST3 plugin and writes out its sound)            |
| -DADLplug_BUILD_TESTS=ON/OFF    | OFF                                    | Build the tests and register them with CTest (CMake's test runner)                            |
| -DADLplug_INSTALL_VST3DIR=<dir> | lib/vst3                               | Install directory of the VST3 plugin (Linux)                                                  |
| -DADLplug_INSTALL_LV2DIR=<dir>  | lib/lv2                                | Install directory of the LV2 plugin (Linux)                                                   |

With `ADLplug_PGO`, a Release build trains (collects a profile) before it
compiles. In `pgo/instrumented` under the build directory, it builds a VST3
plugin and the offline renderer with code that collects the profile (data on
which code runs most), renders with every emulator core of the build, and
compiles every target (the plugins and the rest that the build makes) with
that profile; the profile is made again only when the plugin changes. It
needs `llvm-profdata` and the profile runtime of LLVM's compiler-rt (in the
`libclang-rt-<version>-dev` package of apt.llvm.org), and a machine that can
run the plugin it builds: to build the AVX2 variant on a CPU without AVX2, or
to build faster, turn it off.

Every emulator core is built by default. To leave cores out, turn off options
in the Build option column of the tables under
[FM core characteristics](#fm-core-characteristics), for example
`-DUSE_OPAL_EMULATOR=OFF`. Some options, such as `USE_NUKED_EMULATOR`, leave
several cores out at once, and some names exist in both libADLMIDI and
libOPNMIDI; such a name affects only the plugin being built (libADLMIDI for
ADLplug-Next). `USE_DOSBOX_EMULATOR` (ADLplug-Next) and `USE_MAME_EMULATOR`
(OPNplug-Next) cannot be turned off, because the plugins measure their
instruments on those cores (the configuration stops with an error).

A project saved with a core that a build leaves out plays on that build's
default core for the same chip. The choice of the core stays saved, so a
build with the core plays it again.

### Instrument banks

The instrument banks in the plugins (sets of instruments) are made when the
plugins are built, by `tools/bankgen`, from the files of the submodules (the
outside repositories that the project includes) in `thirdparty/`:
- ADLplug-Next: the banks of libADLMIDI, from their WOPLX files (libADLMIDI's
  bank format), as libADLMIDI's `banks-no-grey.ini` lists them, with each
  file once.
- OPNplug-Next: the banks that `resources/opn2/banks.ini` lists, which are the
  WOPN banks of libOPNMIDI and the GYB and GEMS banks among the examples of
  OPN2 Bank Editor. The GYB and GEMS banks are converted as OPN2 Bank Editor
  reads them.

libADLMIDI has a grey zone of banks that were made without an explicit
permission of their authors, whose legal status is unclear, and has
substitutes for them. Unless `-DADLplug_GREYZONE_BANKS=ON` is given, the
plugins are built without the grey zone. With it, ADLplug-Next takes
libADLMIDI's `banks.ini` instead, and OPNplug-Next also the banks that its
list (`resources/opn2/banks.ini`) marks as grey zone, for which no license or
permission of their authors is known. The binaries on Releases are built
without the grey zone.

Each bank includes what its sources say of it, such as its authors and its
terms of use. The plugins show it under "Bank information..." in the menu that
loads banks, and a build writes it for all the banks to
`banks/<plugin name>-banks.txt` in the build directory.

### Testing

To run the tests, configure (set up the build with CMake) with the tests and
the developer tools, build, and run CTest:

```
cmake --preset adl-debug -DADLplug_BUILD_TESTS=ON -DADLplug_BUILD_TOOLS=ON
cmake --build --preset adl-debug
ctest --preset adl-debug
```

- The unit tests (`tests/unit`) check ADLplug-Next's own shared code and the
  banks in the plugins. `bankgen.greyzone` generates the banks with the grey
  zone as well, which checks what only those banks use, such as the
  conversion of GYB and GEMS banks.
- The render tests load the VST3 plugin into `tools/render`, have it play a
  fixed MIDI sequence, and compare the output with
  `tests/render/references.txt`. They also check that:
  - opening the editor or restoring the saved state does not change the output
  - the plugin keeps its parameters and state when it is prepared again
  - the VST3 plugin keeps the parameter IDs of upstream ADLplug
  - in Release builds, every emulator core plays
  - in a build that leaves cores out, asking for one of them plays another
    core in its place
- When [pluginval](https://github.com/Tracktion/pluginval) or
  [lv2lint](https://git.open-music-kontrollers.ch/~hp/lv2lint) (plugin
  validators) is on the `PATH` at configure time, it validates the VST3 or
  LV2 plugin as well.

The editor tests open windows, so on Linux they need an X11 display with a
window manager (the software that manages windows); a desktop session runs
them on Xwayland. Without a window manager, lv2lint stops at an X error from
the editor.

### Installing a build

On Linux, `cmake --install` installs what a build made, under the directory
that `--prefix` gives:

```
sudo cmake --install build/adl-release --prefix /usr/local
```

The plugins go into `lib/vst3` and `lib/lv2` under that directory
(`/usr/local/lib/vst3` and `/usr/local/lib/lv2` above). On systems that keep
their libraries in `lib64`, such as RHEL and openSUSE, the LV2 hosts look in
`lib64/lv2`: configure with `-DADLplug_INSTALL_LV2DIR=lib64/lv2` there. On
Windows and macOS, copy the plugins that the build made in
`build/<preset>/ADLplug_artefacts/Release` as [Installing](#installing)
describes.

### Change Log

**dev**

- Fixed state reloading under certain plugin hosts
- Linked to pthread on platforms where relevant
- Fixed the user interface using 100% CPU on Windows

**1.0.2**

- English translation of the user manual by Bruce Sutherland
- modified KSL editing behavior to make it linear with regards to attenuation
- added a build option to link with system-wide libfmt
- made the resource system compatible with unsigned-char targets

**1.0.1**

- updated XG bank by Wohlstand for OPN with new percussion instruments
- partial rework of the state handling mechanism
- initial version of Audio Unit; does not pass the validation yet

**1.0.0**

- added the Java OPL3 emulator by Robson Cozendey
- added the Opal OPL3 emulator from Reality Adlib Tracker
- fixed missing percussion in case the key is released very quickly
- allowed to play the full drum set on virtual keyboard
- prevented selection of percussive instruments on melodic channels and vice-versa
- permitted changing programs using the scroll wheel over the combo box
- limited the scroll wheel step to 1 for discrete controls
- displayed the exact value for knob and slider controls
- supported the rhythm-mode channels for bank files which use it
- performed more efficient channel management in case of many sustained notes
- fixed a rare fatal error in case hold pedal is used and channel pressure is high
- implemented a custom resource system for faster rebuilds
- changed the user interface in minor ways

**1.0.0.beta.5**

- added a new chip: YM2608 (OPNA) using Neko Project II Kai emulation
- added a new emulator: MAME YM2608
- allowed to choose a chip rate which matches either OPN2 or OPNA instruments
- allowed saving and restoring the program selection, part selection and bank name
- added Non session management capabilities `optional-gui`, `switch`
- hidden a large number of parameters to improve performance under hosts
- fixed incorrect handling of OPN levels on the graphical interface
- permitted a VST2 build using VeSTige as a replacement of Steinberg SDK
- added a CLI flag `--version` in the JACK standalone
- added a window icon in the JACK standalone
- built the macOS standalone as an app bundle

**1.0.0.beta.4**

- add a control for master volume
- support loading SBI instruments
- support the Non session manager
- fixed a case when the state loading fallback would fail because of a bad initialization sequence
- fixed the editor state after closing and reopening under certain hosts
- allow to reload a saved bank which has no melodic banks or no percussive banks
- add an ability to delete entire banks
- memorize the instrument directory between uses
- update the bank collection for OPN2

**1.0.0.beta.3**

- added the ability to add, delete and rename banks and programs
- support extended key maps with unicode characters
- fixed a crash at startup when the state is restored before setting up the synthesizer
- added soft panning support for OPN2
- fixed a case where parameters would not be synchronized after receiving MIDI program change

**1.0.0.beta.2**

- added the CLI flag `-a` for auto-connection to system outputs in the JACK-only standalone
- added the freedesktop shortcuts and icons
- support for keyboard mappings other than QWERTY
- support setting the keyboard's octave
- highlighted the keys played via MIDI input
- made the program selection follow MIDI program change events
- allowed to install into the GNU standard installation directories
- we have been selected for the [Open Source Music FM Synthesizer Challenge](https://fmchallenge.osamc.de/fmsynths/)! :tada:

**1.0.0.beta.1**

- support of OPN2 synthesis in a distinct plugin
- fixed the plugin state which would be saved incomplete
- fixed the extension of OPN2 bank files in the file chooser

**1.0.0.alpha.3**

- compensation of MIDI latency at high buffer sizes
- fixed a mismanagement of the 4-op channel map
- fixed cases of bad channel allocations following a long idle period
- improved internal timing precision
- gained an ability to save and restore the current state
- added a large collection of embedded banks
- enhanced the UI in various ways

## License

ADLplug-Next as a whole is distributed under the **GNU General Public License v3**
(GPLv3; see `LICENSE`). It combines code under several different free
software licenses, and the strongest of those governs the whole.

Code written for ADLplug-Next — new files, and the changes made to files that
came from ADLplug — is licensed under **GPLv3-or-later** (GPLv3 or any later
version). Its copyright holder is Yuma Kakei, yumasansansan on GitHub.

The parts developed for the original ADLplug remain available from their
authors under the **Boost Software License 1.0** (see `LICENSES/BSL-1.0.txt`),
which is what the Boost notices in `sources/` refer to. The Boost Software
License is compatible with the GPL, so those parts may be redistributed as
part of ADLplug-Next with their notices intact.

Every file names its copyright holders and its license in the machine-readable
form of the [REUSE specification](https://reuse.software/spec-3.3/): in a
comment at its top, or, for the files that cannot hold one, such as images and
fonts, in `REUSE.toml`. A file that came from ADLplug keeps its original
notice, and its header tells ADLplug's part from what ADLplug-Next changed.
The license texts are in `LICENSES/`, and the CI checks every file with
`reuse lint`.

The binaries (the built plugins and programs) are distributed under GPLv3,
not GPLv3-or-later, because:
- the bundled ASIO and AAX SDKs are available under GPLv3, but not
  GPLv3-or-later;
- JUCE is used under the **AGPLv3** option of its dual licence (two licenses
  to choose from).

GPLv3 §13 explicitly permits combining a GPLv3 work with an AGPLv3 work; the
AGPLv3's clause on interaction over a network then applies to the
combination. For an audio plugin this has no practical effect, but it is why
the binaries cannot be described as "GPLv3 only".

| Files                                      | License                                                    |
| ------------------------------------------ | ---------------------------------------------------------- |
| `sources/`, `tools/` — ADLplug-Next code   | GNU GPL v3 or later                                        |
| `sources/` — original ADLplug parts        | Boost Software License 1.0, except as noted below          |
| `thirdparty/JUCE`                          | GNU AGPL v3 (or commercial)                                |
| `thirdparty/JUCE` — bundled ASIO SDK       | Steinberg ASIO License **or GNU GPL v3**                   |
| `thirdparty/JUCE` — bundled AAX SDK        | Avid AAX SDK License **or GNU GPL v3**                     |
| `thirdparty/JUCE` — bundled VST3 SDK       | MIT                                                        |
| `thirdparty/libADLMIDI`                    | GNU LGPL v2.1+, GNU GPL v2+, GNU GPL v3+, MIT, BSD, Boost  |
| `thirdparty/libOPNMIDI`                    | GNU LGPL v2.1+, GNU GPL v2+, GNU GPL v3+, MIT              |
| `thirdparty/simpleini`                     | MIT                                                        |
| `thirdparty/OPN2BankEditor`                | GNU GPL v3+ (only example banks are taken from it)         |
| `sources/opl3/adl/measurer`                | GNU GPL v3+                                                |
| `sources/opn2/adl/measurer`                | GNU GPL v3+                                                |
| `sources/opl3/ui/components/opl3_waves.cc` | GNU LGPL v2.1+                                             |
| `resources/opn2/LICENSE-DMXOPN2.txt`       | MIT (the license of the DMXOPN2 bank)                      |
| Instrument banks, generated by the build   | Their own terms; see [Instrument banks](#instrument-banks) |
| `resources/ui/fonts`                       | SIL Open Font License 1.1 (Liberation, renamed)            |
| `resources/ui/noto-emoji`                  | Apache License 2.0                                         |
| `resources/ui/cores/ESFMu.png`             | GNU LGPL v2.1+ (ESFMu's logo, from its repository)         |
| `docs/manual`                              | Free Art License 1.3 or CC BY-SA 4.0                       |

ASIO is a trademark and software of Steinberg Media Technologies GmbH.
AAX is a trademark of Avid Technology, Inc.
