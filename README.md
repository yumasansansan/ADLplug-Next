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

# ADLplug
Synthesizer plugin for ADLMIDI and OPNMIDI (VST/LV2)

![screenshot](docs/screen.png)

## Introduction

This software package provides FM synthesizer plugins, based on [OPL3](https://en.wikipedia.org/wiki/Yamaha_YMF262) and [OPN2](https://en.wikipedia.org/wiki/Yamaha_YM2612) sound chip emulations.  
The emulations and the drivers are provided by [libADLMIDI](https://github.com/Wohlstand/libADLMIDI) and [libOPNMIDI](https://github.com/Wohlstand/libOPNMIDI).

- [x] control of multiple YMF262/YM2612 emulated chips
- [x] high fidelity emulation, with choice of compromise level (good fidelity/fast, excellent fidelity/slow)
- [x] synthesis of melodic and percussive instruments
- [x] extensible polyphony
- [x] bundled collection of instruments
- [x] support for dynamic parameterization and automation
- [x] rigorous implementation of the MIDI standard
- [x] multi-channel operation with General MIDI compatibility
- [x] ability to synthesize entire MIDI files out of the box

Author: [Jean Pierre Cimalando](https://github.com/jpcima)  
Contributors: [Olivier Humbert](https://github.com/trebmuh), [Christopher Arndt](https://github.com/SpotlightKid), [Bruce Sutherland](https://github.com/bsutherland), [David Runge](https://github.com/dvzrv), [Jérémy Frey](https://github.com/jfrey-xx)

## Development builds

[![CI](https://github.com/yumasansansan/ADLplug-Next/actions/workflows/ci.yml/badge.svg)](https://github.com/yumasansansan/ADLplug-Next/actions/workflows/ci.yml)

Every push and pull request is built by GitHub Actions on Windows, Linux and
macOS, in Debug and Release; x86-64 builds are made both for the baseline
instruction set and for AVX2.

## Useful links

- User Manual : [English :us:](http://jpcima.sdf1.org/software/documentation/ADLplug/manual/en/manual.html) [French :fr:](http://jpcima.sdf1.org/software/documentation/ADLplug/manual/fr/manual.html)
- LibraZiK-2 : [ADLplug :fr:](https://librazik.tuxfamily.org/doc2/logiciels/adlplug) [OPNplug :fr:](https://librazik.tuxfamily.org/doc2/logiciels/opnplug)
- Fedora Copr : [ycollet/linuxmao](https://copr.fedorainfracloud.org/coprs/ycollet/linuxmao/)
- Arch Linux AUR : [adlplug-git](https://aur.archlinux.org/packages/adlplug-git/) and [opnplug-git](https://aur.archlinux.org/packages/opnplug-git/)
- Bank editor software : [OPL3](https://github.com/Wohlstand/OPL3BankEditor) and [OPN2](https://github.com/Wohlstand/OPN2BankEditor)

## FM core characteristics

ADLplug builds every emulator core that libADLMIDI and libOPNMIDI provide, and
each one can be left out with a build option (see
[Build instructions](#build-instructions)). The names are those of the
plugins' emulator menu, and the notes summarise what the libraries and the
cores document about themselves.

*Speed* is how many times faster than real time a core played nine notes on
one chip at 44.1 kHz, built with `-O3`, on an Intel Core i7-1360P. An instance
runs two chips by default. With *full panning*, a voice can sit anywhere
between left and right; without it, a voice is on the left, in the centre or
on the right, and the OPL2 has no stereo at all.

The low-level (LLE) cores reproduce a chip's circuits as read from its die
shots. They are the most faithful, and too slow to play in real time on most
computers: use them to render.

**ADLplug**

| Core                             | Chip           | Notes                                                                                                  | Speed | Full panning | Build option                  |
|----------------------------------|----------------|--------------------------------------------------------------------------------------------------------|------:|--------------|-------------------------------|
| DOSBox 0.74-r4111 OPL3 (default) | OPL3 (YMF262)  | Accurate and fast, per libADLMIDI                                                                      |  346× | yes          | `USE_DOSBOX_EMULATOR`, needed |
| Nuked OPL3 (v 1.8)               | OPL3 (YMF262)  | Very accurate, and needs more CPU power, per libADLMIDI                                                |   57× | yes          | `USE_NUKED_EMULATOR`          |
| Nuked OPL3 Fast (by tgies)       | OPL3 (YMF262)  | An optimised fork of Nuked OPL3 whose output is identical to it bit for bit; replaces Nuked OPL3 1.7.4 |   73× | yes          | `USE_NUKED_EMULATOR`          |
| YMFM OPL3                        | OPL3 (YMF262)  | Aims to be indistinguishable by ear rather than exact to the bit, at a reasonable speed, per ymfm      |  101× | no           | `USE_YMFM_EMULATOR`           |
| Opal OPL3                        | OPL3 (YMF262)  | Inaccurate, per libADLMIDI; written for Reality Adlib Tracker tunes, and has no percussion mode        |  109× | yes          | `USE_OPAL_EMULATOR`           |
| Java 1.0.6 OPL3                  | OPL3 (YMF262)  | Partly accurate, per libADLMIDI                                                                        |   76× | yes          | `USE_JAVA_EMULATOR`           |
| YMF262-LLE OPL3                  | OPL3 (YMF262)  | Low-level; too heavy for ordinary processors, per libADLMIDI                                           |  1.7× | no           | `USE_NUKED_OPL3_LLE_EMULATOR` |
| DOSBox 0.74-r4111 OPL2           | OPL2 (YM3812)  | DOSBox's core run as an OPL2                                                                           |  515× | mono         | `USE_DOSBOX_EMULATOR`         |
| MAME OPL2                        | OPL2 (YM3812)  | MAME's YM3812 core                                                                                     |  214× | mono         | `USE_MAME_EMULATOR`           |
| YMFM OPL2                        | OPL2 (YM3812)  | As YMFM OPL3                                                                                           |  184× | mono         | `USE_YMFM_EMULATOR`           |
| Nuked OPL2 Lite                  | OPL2 (YM3812)  | By Nuke.YKT, version 0.9 beta                                                                          |   88× | mono         | `USE_NUKED_EMULATOR`          |
| YM3812-LLE OPL2                  | OPL2 (YM3812)  | As YMF262-LLE                                                                                          |  3.5× | mono         | `USE_NUKED_OPL2_LLE_EMULATOR` |
| ESFMu                            | ESFM (ESS)     | ESS's extended OPL3 clone, emulated on the basis of Nuked OPL3; libADLMIDI plays it as an OPL3         |  9.7× | yes          | `USE_ESFMU_EMULATOR`          |
| Nuked CQM                        | CQM (Creative) | Creative's OPL3 clone chip, by Nuke.YKT, version 0.9 beta                                              |   22× | no           | `USE_NUKED_EMULATOR`          |

**OPNplug**

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

ADLplug-Next supports Windows 11 or later (x86-64), Ubuntu 26.04 or later
(x86-64) and macOS 26 or later (Apple Silicon). On Linux the user interface
runs on X11, which means XWayland in a Wayland session: JUCE has no Wayland
backend.

Install required dependencies:
- CMake 3.25 or newer, and Ninja
- Clang and LLD 19 or newer, for C23 / C++23 (GCC is not supported; the CI
  builds with LLVM 23)
- Windows: the MSVC libraries and the Windows SDK, from Visual Studio or its
  Build Tools
- Linux: development packages for ALSA, FreeType, Fontconfig and X11; on
  Ubuntu, `libasound2-dev libfontconfig1-dev libfreetype-dev libx11-dev
  libxcomposite-dev libxcursor-dev libxext-dev libxi-dev libxinerama-dev
  libxrandr-dev libxrender-dev`
- macOS: Xcode for the SDK, and LLVM's own Clang and LLD. Compile against the
  SDK's libc++ headers, not the ones that come with the toolchain, because
  the plugins load the system's libc++ (`ci/setup.sh` shows how the CI does
  it)

### Compiling

```
git clone --recursive https://github.com/yumasansansan/ADLplug-Next.git
cd ADLplug-Next
cmake --preset adl-release     # or opn-release; *-relwithdebinfo, *-debug for development
cmake --build --preset adl-release
```

The presets in `CMakePresets.json` work on all three systems. They select
Clang, LLD and Ninja without fixing their versions, and treat warnings in
ADLplug's own code as errors. To configure by hand instead, pass the options
below to `cmake` directly.

This package is able to build several plugins from a single source:
- to build the OPL3 variant, define the option `ADLplug_CHIP` to `OPL3`;
- to build the OPN2 variant, define the option `ADLplug_CHIP` to `OPN2`.

| Build option                    | Default                                | Description                                                      |
| ------------------------------- | -------------------------------------- | ---------------------------------------------------------------- |
| -DADLplug_VST3=ON/OFF           | ON                                     | Build a VST3 plugin                                              |
| -DADLplug_LV2=ON/OFF            | ON                                     | Build a LV2 plugin                                               |
| -DADLplug_AU=ON/OFF             | ON on macOS, OFF elsewhere             | Build an Audio Unit (macOS only)                                 |
| -DADLplug_AAX=ON/OFF            | ON on Windows and macOS, OFF elsewhere | Build an AAX plugin (needs PACE signing to load in Pro Tools)    |
| -DADLplug_Standalone=ON/OFF     | ON                                     | Build a standalone program                                       |
| -DADLplug_ASIO=ON/OFF           | ON on Windows, OFF elsewhere           | Enable ASIO in the standalone (Windows; uses JUCE's bundled SDK) |
| -DADLplug_CHIP=OPL3/OPN2        | OPL3 (the opn-* presets set OPN2)      | Build a variant for the given chip type                          |
| -DADLplug_GREYZONE_BANKS=ON/OFF | OFF                                    | Include the banks of the grey zone (see below)                   |
| -DADLplug_ARCH=baseline/avx2    | baseline                               | x86-64 instruction set: baseline or AVX2 (x86-64-v3)             |
| -DADLplug_LTO=ON/OFF            | ON                                     | Link-time optimisation (ThinLTO) in Release builds               |
| -DADLplug_ASSERTIONS=ON/OFF     | OFF                                    | Force building with assertions regardless of build type          |
| -DADLplug_WERROR=ON/OFF         | OFF (the presets set ON)               | Treat warnings in ADLplug's own code as errors                   |
| -DADLplug_BUILD_TOOLS=ON/OFF    | OFF                                    | Build developer tools (offline VST3 renderer)                    |
| -DADLplug_BUILD_TESTS=ON/OFF    | OFF                                    | Build the tests and register them with CTest                     |

Every emulator core is built by default. Each has an option of the library it
comes from, `-DUSE_<core>_EMULATOR=ON/OFF`, named in the tables under
[FM core characteristics](#fm-core-characteristics). Some names, such as
`USE_NUKED_EMULATOR`, exist in both libraries; only the library of the chip
being built is affected. `USE_DOSBOX_EMULATOR` (ADLplug) and
`USE_MAME_EMULATOR` (OPNplug) have to stay on, because the plugins measure
their instruments on those cores. A project saved with a core that a build
leaves out plays on that build's default core for the same chip, and keeps
its choice: a build with the core plays it again.

### Instrument banks

The banks that the plugins offer are generated when they are built, by
`tools/bankgen`, from the submodules in `thirdparty/`:
- ADLplug takes the banks built into libADLMIDI, from their WOPLX files, as
  libADLMIDI's `banks-no-grey.ini` lists them, with each file once.
- OPNplug takes the banks that `resources/opn2/banks.ini` lists: WOPN banks of
  libOPNMIDI, and GYB and GEMS banks from the examples of OPN2 Bank Editor,
  converted as the editor reads them.

libADLMIDI keeps a grey zone of banks that were made without an explicit
permission of their authors, whose legal status is unclear, and has
placeholders for them. The build leaves the grey zone out unless
`-DADLplug_GREYZONE_BANKS=ON` is given: ADLplug then takes libADLMIDI's
`banks.ini` instead, and OPNplug also the banks that its list marks as grey
zone, for which no license or permission of their authors is known. The
binaries of ADLplug-Next are built without them.

Each bank keeps what its sources say of it, such as its authors and its terms
of use. The editor shows that under "Bank information..." in the menu that
loads banks, and the build writes it for all the banks to
`banks/<plugin>-banks.txt` in the build directory.

### Testing

Configure with the tests and the developer tools, build, and run CTest:

```
cmake --preset adl-debug -DADLplug_BUILD_TESTS=ON -DADLplug_BUILD_TOOLS=ON
cmake --build --preset adl-debug
ctest --preset adl-debug
```

- The unit tests (`tests/unit`) check ADLplug's own utilities and the banks it
  embeds. `bankgen.greyzone` generates the banks with the grey zone as well,
  which checks what only those banks need, such as the conversion of GYB and
  GEMS banks.
- The render tests load the VST3 plugin into `tools/render`, play a fixed
  sequence, and compare the output with `tests/render/references.txt`. They
  also check that opening the editor and restoring the saved state change
  nothing, that the plugin keeps its parameters and state when it is prepared
  again, and, in Release builds, that every emulator core plays.
- When [pluginval](https://github.com/Tracktion/pluginval) or
  [lv2lint](https://git.open-music-kontrollers.ch/~hp/lv2lint) is on the
  `PATH` at configure time, it validates the VST3 or LV2 plugin as well.

The editor tests open windows, so on Linux they need an X11 display with a
window manager, as in a desktop session on Xwayland. Without a window manager,
lv2lint stops at an X error from the editor.

### Installing

```
sudo cmake --build . --target install
```

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

ADLplug as a whole is distributed under the **GNU General Public License v3**
(see `LICENSE`). It combines parts under several different free software
licenses; the strongest of those governs the combined work.

Code written for ADLplug-Next — new files, and the changes made to files that
came from upstream — is licensed under the **GNU GPL v3 or (at your option) any
later version**. Its copyright holder, Yuma Kakei, is yumasansansan on GitHub.

The parts developed for the original ADLplug remain available from their
authors under the **Boost Software License 1.0** (see `LICENSES/BSL-1.0.txt`),
which is what the Boost notices in `sources/` refer to. Boost is
GPL-compatible, so those parts may be redistributed as part of this work with
their notices intact.

Every file names its copyright holders and its license in the machine-readable
form of the [REUSE specification](https://reuse.software/spec-3.3/): in a
comment at its top, or, for the files that cannot hold one, such as images and
fonts, in `REUSE.toml`. A file that came from upstream keeps
its original notice, and its header tells ADLplug's part from ADLplug-Next's.
The license texts are in `LICENSES/`, and the CI checks every file with
`reuse lint`.

The binaries themselves are distributed under version 3 of the GPL: the bundled
ASIO and AAX SDKs are available under the GPL v3 with no later-version option,
and JUCE is used under the **AGPLv3** option of its dual licence. GPLv3 §13
explicitly permits combining a GPLv3 work with an AGPLv3 work; the AGPL's
network-interaction clause then applies to the combination. For an audio plugin
this has no practical effect, but it is why the binary cannot be described as
"GPLv3 only".

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
