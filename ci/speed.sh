#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/speed.sh
#
# Measures how long ADLplug-Next OPL3's core takes for a sample at every level
# the processor has (crates/adlplug-opl3/examples/timing.rs, the release
# profile), twice, one run after the other, and records what the machine is:
# the system and, under GitHub Actions, the runner's image; the processor's
# name; the extensions the system reports for it; and those adlplug-cpu
# detects, which the timing program prints. Then what the parts of a sample
# take at every level, twice as well (crates/adlplug-opl3/examples/profile.rs,
# built with the feature experiment): the pass alone, the writes' handling
# alone, the pass without one of its parts each, and the other ways of writing
# the pass that this branch tries, each checked to make the samples of its
# level. On Linux the runs are pinned to one processor (taskset), elsewhere they
# are not, and under GitHub Actions perf counts and samples them as well
# (ci/perf.sh). Under GitHub Actions all of it goes into the summary of the run
# as well.
set -euo pipefail

report=$(mktemp)

{
  echo "### $(uname -s) on $(uname -m)"
  echo
  echo '```'
  echo "runner: ${RUNNER_OS:-} ${RUNNER_ARCH:-}, image ${ImageOS:-?} ${ImageVersion:-?}"
  rustc --version
  # Clang's first line, without a reader that stops early, which under pipefail
  # would fail the script (ci/setup.sh says why).
  clang_version=$(clang --version)
  echo "${clang_version%%$'\n'*}"
  echo
  case "$(uname -s)" in
    Linux)
      LC_ALL=C lscpu
      # The length of SVE's vectors that a process starts with, in bytes, where
      # the processor has SVE.
      if [ -r /proc/sys/abi/sve_default_vector_length ]; then
        echo "SVE's vectors: $(cat /proc/sys/abi/sve_default_vector_length) bytes"
      fi
      ;;
    Darwin)
      sysctl -n machdep.cpu.brand_string
      sysctl -n hw.ncpu hw.perflevel0.physicalcpu hw.perflevel1.physicalcpu 2> /dev/null || true
      # The extensions macOS reports: every hw.optional key that is 1.
      sysctl hw.optional | awk -F ': ' '$2 == 1 { print $1 }' | sort | tr '\n' ' '
      echo
      ;;
    MINGW* | MSYS* | CYGWIN*)
      powershell -NoProfile -Command \
        'Get-CimInstance Win32_Processor | Format-List Name, Manufacturer, Description, NumberOfCores, NumberOfLogicalProcessors, MaxClockSpeed'
      ;;
  esac
  echo '```'
} > "$report"

cargo build --release --locked -p adlplug-opl3 --example timing
timing=target/release/examples/timing
pin=()
if command -v taskset > /dev/null; then
  pin=(taskset -c 0)
fi
for round in 1 2; do
  {
    echo
    echo "#### Round $round${pin[0]:+, pinned to processor 0}"
    echo
    echo '```'
    # The array spelled so that an empty one expands to nothing under set -u
    # in every bash, macOS's 3.2 among them.
    ${pin[@]+"${pin[@]}"} "$timing"
    echo '```'
  } >> "$report"
done

cargo build --release --locked -p adlplug-opl3 --features experiment --example profile
profile=target/release/examples/profile
# A variant that makes other samples than its level fails the profile program
# once it has printed everything, and the script once the report is out.
status=0
for round in 1 2; do
  {
    echo
    echo "#### The parts of a sample, round $round${pin[0]:+, pinned to processor 0}"
    echo
    echo '```'
    ${pin[@]+"${pin[@]}"} "$profile" 2>&1 || status=1
    echo '```'
  } >> "$report"
done

if [ "$(uname -s)" = Linux ] && [ -n "${GITHUB_ACTIONS:-}" ]; then
  bash ci/perf.sh "$profile" "$report"
fi

cat "$report"
if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
  cat "$report" >> "$GITHUB_STEP_SUMMARY"
fi
exit "$status"
