#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/fuzz.sh [--with-long] <preset> <seconds> <corpora> <crashes>
#              [<earlier crashes>]
#
# Runs every fuzz target that a build of <preset> with ADLplug_BUILD_FUZZERS
# made, each for <seconds> with libFuzzer. A target whose every input is heavy
# (CMake wrote <target>.long beside its manifest) is left out unless
# --with-long is given: a minute of it would get through too few inputs to be
# worth the time, so the fuzzing of a push passes it by and the daily fuzzing
# takes it. A target runs with the arguments that CMake listed beside it
# (build/<preset>/fuzz/*.args: its dictionary, seed inputs and regression
# inputs), on a corpus of its own in <corpora>/<target>/, which it grows and
# which may carry over from an earlier run. An input that fails is written to
# <crashes>/<target>/.
#
# Given <earlier crashes>, each target first runs once on the inputs under
# <earlier crashes>/<target>/ that failed in an earlier run. One that still
# fails is kept in <crashes>/<target>/ again and the target is not fuzzed:
# fuzzing looks for inputs at random, and a later run need not come upon the
# same one, so it is replayed until it is fixed. The script fails when any
# target fails.
#
# No run of a target lasts longer than the time it was given and ten minutes
# more. A target that does not end of its own by then -- one wedged where it
# cannot say what it found, or one input of which takes forever -- is stopped and
# counted as a failure, so that the targets after it still run, and the run ends
# by itself with everything it found kept, instead of standing until the time of
# the whole job runs out.
set -euo pipefail

# How the stopping of a target shows: the status timeout keeps for the time
# running out, and the ones a program ended by a signal leaves behind.
was_stopped() {  # status
  [ "$1" -eq 124 ] || [ "$1" -eq 137 ] || [ "$1" -eq 143 ]
}

# ThreadSanitizer reports a race and carries on by default, and says so only in
# the exit status at the end, by which time libFuzzer has fuzzed past the input
# that found it. It stops at the first report instead, as the tests have it
# (CMakeLists.txt), so that libFuzzer writes that input out; a report of locks
# taken in two orders names the other order's place as well. A build without
# the thread sanitizer pays this no heed.
export TSAN_OPTIONS=${TSAN_OPTIONS:-halt_on_error=1:second_deadlock_stack=1}

with_long=0
if [ "${1:-}" = "--with-long" ]; then
  with_long=1
  shift
fi

preset=$1
seconds=$2
corpora=$3
crashes=$4
earlier=${5:-}
guard=$((seconds + 600))

shopt -s nullglob
manifests=("build/$preset/fuzz/"*.args)
if [ ${#manifests[@]} -eq 0 ]; then
  echo "error: build/$preset has no fuzz targets; configure it with -DADLplug_BUILD_FUZZERS=ON" >&2
  exit 1
fi

status=0
for manifest in "${manifests[@]}"; do
  fuzzer=${manifest%.args}
  target=$(basename "$fuzzer")
  if [ -e "$fuzzer.long" ] && [ "$with_long" -eq 0 ]; then
    echo "== $target: left to the long runs"
    continue
  fi
  mapfile -t arguments < "$manifest"
  mkdir -p "$corpora/$target" "$crashes/$target"

  if [ -n "$earlier" ]; then
    # What libFuzzer writes beside a failing input, and what it names it: crash-,
    # oom- and timeout- are the ones that failed, and slow-unit- is an input that
    # took long enough to be worth keeping and failed at nothing. Replaying the
    # slow ones would say every run that they are still slow, which is not a
    # failure and not news.
    failed=("$earlier/$target/"crash-* "$earlier/$target/"oom-* "$earlier/$target/"timeout-*)
    if [ ${#failed[@]} -gt 0 ]; then
      echo "== $target: ${#failed[@]} inputs that failed in an earlier run"
      code=0
      timeout --kill-after=60s "$guard" "$fuzzer" "${failed[@]}" || code=$?
      if [ "$code" -ne 0 ]; then
        if was_stopped "$code"; then
          echo "error: $target was still replaying the inputs that failed in an earlier run after $guard seconds, and was stopped" >&2
        else
          echo "error: $target still fails on an input that failed in an earlier run" >&2
        fi
        cp "${failed[@]}" "$crashes/$target/"
        status=1
        continue
      fi
    fi
  fi

  echo "== $target: fuzzing for $seconds seconds"
  code=0
  timeout --kill-after=60s "$guard" "$fuzzer" -max_total_time="$seconds" -print_final_stats=1 \
      -artifact_prefix="$crashes/$target/" "$corpora/$target" "${arguments[@]}" || code=$?
  if [ "$code" -ne 0 ]; then
    if was_stopped "$code"; then
      echo "error: $target was still running $guard seconds after it began, and was stopped; what it found up to then is in $corpora/$target/" >&2
    else
      echo "error: $target failed; the input is in $crashes/$target/" >&2
    fi
    status=1
  fi
done
exit $status
