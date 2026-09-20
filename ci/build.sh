#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/build.sh [--fuzz-only] <preset> <baseline|avx2|arm64> [<cmake option>...]
#
# Configures a CMake preset, with the developer tools and the tests, for the
# given instruction set (arm64 stands for the macOS build, which has no choice)
# and any further options, such as emulator cores to leave out.
#
# With --fuzz-only, it configures the fuzz targets with the whole coverage that
# libFuzzer can steer by and builds those targets and nothing else. A plugin of
# such a build cannot be loaded (fuzz/CMakeLists.txt says why), and the long
# fuzzing does not need one; what the plugin needs is checked by the builds of
# every push.
#
# The configuration checks the toolchain (cmake/LLVMToolchain.cmake), and this
# script shows what it checked, in the log and, in GitHub Actions, in the
# summary of the job. Before building, it checks the -march flags of the
# compile commands, and ThinLTO in those of Release builds, whose every compile
# and link option it lists (ci/flags.py). Then it builds, with every command
# shown in full, those of the builds that the build starts included (the
# instrumented build of profile-guided optimisation), and lists the artefacts,
# the libraries the VST3 plugin links against and, on Linux, the newest version
# it asks of the libraries of the system.
set -euo pipefail

fuzz_only=0
if [ "${1:-}" = "--fuzz-only" ]; then
  fuzz_only=1
  shift
fi

preset=$1
arch=$2
shift 2

args=(--preset "$preset" -DADLplug_BUILD_TOOLS=ON -DADLplug_BUILD_TESTS=ON)
if [ "$fuzz_only" -eq 1 ]; then
  args+=(-DADLplug_BUILD_FUZZERS=ON -DADLplug_FUZZ_FULL_COVERAGE=ON)
fi
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

    # Every option that reaches the compilers, the linker and the resource
    # compiler, wherever it comes from, for the log to show.
    case "$(uname -s)" in
      MINGW* | MSYS* | CYGWIN*) python=python ;;
      *) python=python3 ;;
    esac
    if [ -n "${GITHUB_ACTIONS:-}" ]; then
      echo "::group::== options of the compile and link commands (ci/flags.py)"
    else
      echo "== options of the compile and link commands (ci/flags.py)"
    fi
    "$python" ci/flags.py "build/$preset"
    if [ -n "${GITHUB_ACTIONS:-}" ]; then
      echo "::endgroup::"
    fi
    ;;
esac

# Ninja shows every command in full, rather than its short description. The
# VERBOSE variable carries that into the builds that CMake starts within the
# build, such as the instrumented build of cmake/PGO.cmake.
if [ "$fuzz_only" -eq 1 ]; then
  # The targets CMake wrote a manifest for, which is every fuzz target this
  # build has, and nothing else.
  shopt -s nullglob
  fuzz_targets=()
  for manifest in "build/$preset/fuzz/"*.args; do
    fuzz_targets+=("$(basename "${manifest%.args}")")
  done
  if [ ${#fuzz_targets[@]} -eq 0 ]; then
    echo "error: build/$preset has no fuzz targets to build" >&2
    exit 1
  fi
  echo "== the fuzz targets, and nothing else: ${fuzz_targets[*]}"
  VERBOSE=1 cmake --build --preset "$preset" --target "${fuzz_targets[@]}"
  exit 0
fi

VERBOSE=1 cmake --build --preset "$preset"

artefacts=build/$preset/ADLplug_artefacts
echo "== artefacts"
find "$artefacts" -mindepth 3 -maxdepth 3

echo "== libraries the VST3 plugin links against"
case "$(uname -s)" in
  Linux)
    plugin=$(find "$artefacts" -path '*/VST3/*' -name '*.so' -print -quit)
    ldd "$plugin"
    # The C library and the C++ library are the system's, and the plugin asks for
    # a version of each by name: the newest version it asks for is the oldest
    # system that can load it. A build on a newer system raises them with nothing
    # else to say so, which is what this shows. It is the same arrangement that
    # leaves a fault in either library for the system to update.
    echo "== the newest version the VST3 plugin asks of the system's libraries"
    for library in GLIBC GLIBCXX CXXABI; do
      asked=$(objdump -T "$plugin" | grep -o -E "${library}_[0-9.]+" | sort -u -V | tail -1)
      printf '   %s\n' "${asked:-(no $library)}"
    done
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
