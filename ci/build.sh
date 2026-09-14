#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/build.sh <preset> <baseline|avx2|arm64>
#
# Configures and builds a CMake preset, with the developer tools and the tests,
# for the given instruction set (arm64 stands for the macOS build, which has no
# choice). Then lists the -march flags of the compile commands, the artefacts,
# and the libraries the VST3 plugin links against.
set -euo pipefail

preset=$1
arch=$2

args=(--preset "$preset" -DADLplug_BUILD_TOOLS=ON -DADLplug_BUILD_TESTS=ON)
case $arch in
  baseline | avx2) args+=("-DADLplug_ARCH=$arch") ;;
  arm64) ;;
  *) echo "error: unknown instruction set '$arch'" >&2; exit 2 ;;
esac

cmake "${args[@]}"
cmake --build --preset "$preset"

echo "== -march flags in the compile commands"
grep -o -E -- '-march=[a-z0-9-]+' "build/$preset/compile_commands.json" | sort | uniq -c || echo "(none)"

artefacts=build/$preset/ADLplug_artefacts
echo "== artefacts"
find "$artefacts" -mindepth 3 -maxdepth 3

echo "== libraries the VST3 plugin links against"
case "$(uname -s)" in
  Linux)
    ldd "$(find "$artefacts" -path '*/VST3/*' -name '*.so' -print -quit)"
    ;;
  Darwin)
    binary=$(find "$artefacts" -path '*/VST3/*/Contents/MacOS/*' -type f -print -quit)
    libraries=$(otool -L "$binary")
    echo "$libraries"
    # The plugins have to use the system's libc++ (see ci/setup.sh).
    while IFS= read -r line; do
      case $line in
        */usr/lib/libc++*) ;;
        *libc++*)
          echo "error: $binary links against a libc++ other than the system's: $line" >&2
          exit 1
          ;;
      esac
    done <<< "$libraries"
    ;;
  *)
    echo "(not listed on this system)"
    ;;
esac
