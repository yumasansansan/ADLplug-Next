#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/build.sh <preset> <baseline|avx2|arm64> [<cmake option>...]
#
# Configures a CMake preset, with the developer tools and the tests, for the
# given instruction set (arm64 stands for the macOS build, which has no choice)
# and any further options, such as emulator cores to leave out. The
# configuration checks the toolchain (cmake/LLVMToolchain.cmake), and this
# script shows what it checked, in the log and, in GitHub Actions, in the
# summary of the job. Before building, it checks the -march flags of the
# compile commands, and ThinLTO in those of Release builds. Then it builds, and
# lists the artefacts and the libraries the VST3 plugin links against.
set -euo pipefail

preset=$1
arch=$2
shift 2

args=(--preset "$preset" -DADLplug_BUILD_TOOLS=ON -DADLplug_BUILD_TESTS=ON)
case $arch in
  baseline | avx2) args+=("-DADLplug_ARCH=$arch") ;;
  arm64) ;;
  *) echo "error: unknown instruction set '$arch'" >&2; exit 2 ;;
esac
args+=("$@")

cmake "${args[@]}"

# Every tool that the configuration checked, with the version it reported
# (none for llvm-rc, which has to lie beside the compiler instead). A missing
# record means that the check did not run, and the build does not go on.
toolchain=build/$preset/llvm-toolchain.txt
if [ ! -s "$toolchain" ]; then
  echo "error: the configuration left no record of its toolchain check ($toolchain)" >&2
  exit 1
fi
echo "== LLVM toolchain, as the configuration checked it"
if [ -n "${ADLplug_LLVM_MAJOR:-}" ]; then
  echo "   required major version (ADLplug_LLVM_MAJOR): $ADLplug_LLVM_MAJOR"
fi
while IFS=$'\t' read -r part version program; do
  printf '   %-8s  %-26s %s\n' "$version" "$part" "$program"
done < "$toolchain"
if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
  {
    echo "### LLVM toolchain of $preset ($arch)"
    echo
    if [ -n "${ADLplug_LLVM_MAJOR:-}" ]; then
      echo "Required major version (\`ADLplug_LLVM_MAJOR\`): $ADLplug_LLVM_MAJOR"
      echo
    fi
    echo "| Version | Part | Program |"
    echo "| --- | --- | --- |"
    while IFS=$'\t' read -r part version program; do
      echo "| $version | $part | \`$program\` |"
    done < "$toolchain"
    echo
  } >> "$GITHUB_STEP_SUMMARY"
fi

echo "== -march flags in the compile commands"
grep -o -E -- '-march=[a-z0-9-]+' "build/$preset/compile_commands.json" | sort | uniq -c || echo "(none)"

# Release builds are made with ThinLTO throughout, and a compile command
# without it would mean code left out of link-time optimisation with nothing
# to say so. Windows resource scripts are not code.
case $preset in
  *-release)
    commands=$(grep '"command"' "build/$preset/compile_commands.json" | grep -v -E 'cmake_llvm_rc|\.rc\.res' || true)
    total=$(grep -c . <<< "$commands" || true)
    without=$(grep -v -c -- '-flto=thin' <<< "$commands" || true)
    echo "== ThinLTO: $((total - without)) of $total compile commands"
    if [ "$total" -eq 0 ] || [ "$without" -ne 0 ]; then
      echo "error: $preset has compile commands without -flto=thin:" >&2
      grep -v -- '-flto=thin' <<< "$commands" | head -n 5 >&2 || true
      exit 1
    fi
    ;;
esac

cmake --build --preset "$preset"

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
