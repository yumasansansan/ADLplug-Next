#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/coverage.sh [--report-only] <preset> [<test argument>...]
#
# Measures which of this project's own code the tests and the fuzz corpus reach,
# in a build configured with ADLplug_COVERAGE (the *-coverage presets).
#
# One run of the tests is the whole of it. CTest runs the unit tests, the renders
# through the plugin, and the replay of every seed and regression input through
# each fuzz target, and each program writes what it counted as it ends; the
# profiles are merged and reported together, so the number is what all of them
# reach between them.
#
# What is counted is this project's own sources and nothing else: the instrumenting
# is put on the files adlplug_own_sources() names (CMakeLists.txt), and the report
# leaves out what a build generates and whatever else came in with it. A line of
# JUCE or of the emulator cores is not this project's to reach, and counting it
# would only make the number look better.
#
# With --report-only it reads the profiles the build directory already holds
# rather than running anything, which is how the same run is looked at again
# without waiting for it twice.
#
# The report says three things, and the third is the point of the exercise: the
# summary for the whole of it, the file it says the least about, and every file no
# test and no input reaches at all. What to aim the next test or fuzz target at is
# meant to be read off the last of those.
set -euo pipefail

report_only=0
case "${1:-}" in
  --report-only) report_only=1; shift ;;
esac

preset=$1
shift

build=build/$preset
if [ ! -f "$build/CMakeCache.txt" ]; then
  echo "error: $build is not configured" >&2
  exit 1
fi
if ! grep -q '^ADLplug_COVERAGE:BOOL=ON' "$build/CMakeCache.txt"; then
  echo "error: $build is not configured with ADLplug_COVERAGE=ON. The *-coverage presets are." >&2
  exit 1
fi

profiles=$build/coverage
if [ "$report_only" -eq 0 ]; then
  rm -rf "$profiles"
  mkdir -p "$profiles"

  # Each image writes its own profile: %m is the binary it came from and %p the
  # process, so the plugin a render loads does not write over its host's.
  export LLVM_PROFILE_FILE="$PWD/$profiles/%m-%p.profraw"

  echo "== the tests, counting what they reach"
  bash ci/test.sh "$preset" "$@"
fi

echo "== the profiles the run left"
shopt -s nullglob
raw=("$profiles"/*.profraw)
echo "   ${#raw[@]} files, $(du -sh "$profiles" | cut -f1)"
if [ ${#raw[@]} -eq 0 ]; then
  echo "error: the run wrote no profile. Was the build made with ADLplug_COVERAGE=ON?" >&2
  exit 1
fi
llvm-profdata merge -sparse -o "$profiles/all.profdata" "${raw[@]}"

# Every program and library of the build that carries a coverage mapping. Asked of
# each rather than listed, since what a build makes depends on its options: a
# binary without a mapping is one that links nothing of this project's own code.
echo "== the binaries that carry what was counted"
objects=()
while IFS= read -r candidate; do
  if llvm-cov report -instr-profile="$profiles/all.profdata" "$candidate" > /dev/null 2>&1; then
    objects+=(-object "$candidate")
    echo "   ${candidate#"$build"/}"
  fi
done < <(find -L "$build" -type f \( -perm -u+x -o -name '*.so' -o -name '*.vst3' \) \
           -name 'ADLplug*' ! -name '*.profraw' ! -name '*.profdata' | sort)
if [ ${#objects[@]} -eq 0 ]; then
  echo "error: no binary of $build carries a coverage mapping" >&2
  exit 1
fi
# The first object is given without -object, as llvm-cov wants it.
main=${objects[1]}
unset "objects[0]" "objects[1]"

# The plugin's own code is the subject. fuzz/, tests/ and tools/ are what does the
# reaching, and what of a driver its own run reaches says nothing about the plugin;
# thirdparty/ and what a build generates are not this project's to reach at all.
elsewhere='(thirdparty|JuceLibraryCode|_deps|fuzz|tests|tools)/'

# Asked once, and read four times from what it wrote. Several binaries here are
# built from the same sources, so llvm-cov says that some functions' counters
# belong to another build of them ("functions have mismatched data") and leaves
# those out; asking once is what keeps that from being said four times over.
llvm-cov report -instr-profile="$profiles/all.profdata" "$main" ${objects[@]+"${objects[@]}"} \
  -ignore-filename-regex="$elsewhere" -show-region-summary=false > "$profiles/report.txt"

echo "== what the tests and the corpus reach, of the plugin's own code"
tail -3 "$profiles/report.txt"
summary=$(tail -1 "$profiles/report.txt")

echo "== the files they say the least about"
awk 'NR > 2 && $1 ~ /\// && $NF ~ /^[0-9.]+%$/ { print $NF, $1 }' "$profiles/report.txt" |
  sed 's/%//' | sort -n | head -12 | awk '{ printf "   %6s%%  %s\n", $1, $2 }'

echo "== reached by nothing at all"
awk 'NR > 2 && $1 ~ /\// && $NF == "0.00%" { print "   " $1 }' "$profiles/report.txt" | head -20

# The whole of it as a page, for whoever wants to read a file line by line.
llvm-cov show "$main" ${objects[@]+"${objects[@]}"} -instr-profile="$profiles/all.profdata" \
  -ignore-filename-regex="$elsewhere" -format=html -output-dir="$profiles/html" \
  -show-line-counts-or-regions -show-instantiation-summary > /dev/null 2>&1
echo "== the page: $profiles/html/index.html"

if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
  {
    echo "### What the tests and the fuzz corpus reach ($preset)"
    echo
    echo '```'
    cat "$profiles/report.txt"
    echo '```'
  } >> "$GITHUB_STEP_SUMMARY"
fi
echo "   $summary"
