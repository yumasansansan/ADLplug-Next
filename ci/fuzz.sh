#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/fuzz.sh [--with-long] [--only <targets> | --skip <targets>] <preset>
#              <seconds> <corpora> <crashes> [<earlier crashes>]
#
# Runs every fuzz target that a build of <preset> with ADLplug_BUILD_FUZZERS
# made, each for <seconds> with libFuzzer. A target whose every input is heavy
# (CMake wrote <target>.long beside its manifest) is left out unless
# --with-long is given: a minute of it would get through too few inputs to be
# worth the time, so the fuzzing of a push passes it by and the daily fuzzing
# takes it. --only runs no target but the ones named, and --skip every target
# but those, each given as names joined by commas: the daily fuzzing splits the
# targets of a preset between two jobs this way, since one job fuzzing all of
# them in turn outgrew the time a job may take under the memory sanitizer. A
# name that is no target of the build is an error, so that a renamed target
# cannot fall out of every job without a word. A target runs with the arguments
# that CMake listed beside it (build/<preset>/fuzz/*.args: its dictionary, seed
# inputs and regression inputs), on a corpus of its own in <corpora>/<target>/,
# which it grows and which may carry over from an earlier run. Once a target has
# run, that corpus is merged down to the fewest inputs that reach what all of it
# reached. An input that fails is written to <crashes>/<target>/.
#
# Given <earlier crashes>, each target first runs once on the inputs under
# <earlier crashes>/<target>/ that failed in an earlier run. One that still
# fails is kept in <crashes>/<target>/ again and the target is not fuzzed:
# fuzzing looks for inputs at random, and a later run need not come upon the
# same one, so it is replayed until it is fixed. The script fails when any
# target fails.
#
# No run of a target lasts longer than the time it was given and ten minutes
# more, or an hour more in a build with the memory sanitizer, which is slower by
# a good deal (the guard below). A target that does not end of its own by then
# -- one wedged where it cannot say what it found, or one input of which takes
# forever -- is stopped and counted as a failure, so that the targets after it
# still run, and the run ends by itself with everything it found kept, instead
# of standing until the time of the whole job runs out.
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
only=
skip=
while [ $# -gt 0 ]; do
  case "$1" in
    --with-long)
      with_long=1
      shift
      ;;
    --only | --skip)
      if [ -z "${2:-}" ]; then
        echo "error: $1 takes the names of targets, joined by commas" >&2
        exit 2
      fi
      if [ "$1" = --only ]; then
        only=$2
      else
        skip=$2
      fi
      shift 2
      ;;
    *)
      break
      ;;
  esac
done
if [ -n "$only" ] && [ -n "$skip" ]; then
  echo "error: --only and --skip do not go together" >&2
  exit 2
fi

preset=$1
seconds=$2
corpora=$3
crashes=$4
earlier=${5:-}

# The time a target is given beyond the time it was asked to run for, after which
# it is taken to have hung rather than to be slow. Ten minutes is room enough for a
# build with the address or thread sanitizer to start, read every seed and
# regression input, and stop when it is told to.
#
# A build with the memory sanitizer is slower than that by a good deal, and what
# counts as hung has to allow for it: measured on one CI run of the same commit and
# the same target, the bank manager's did 2 inputs a second under the address
# sanitizer and 0.3 under the memory one, and its slowest single input took 31
# seconds where the address sanitizer's took under one. That target asks the
# plugin's own worker to measure an instrument, which plays it for as long as forty
# seconds and listens for sixty more, and every one of those samples is carried
# through the shadow memory and the origins. So the memory sanitizer's builds get
# an hour instead of ten minutes: the same work, at the speed it really runs. Half
# an hour was what they had first, and with the twenty minutes the daily fuzzing
# asks for, that was not enough for OPNplug-Next's measurement target: on three
# runs in a row it was still replaying its corpus of some 180 inputs when it was
# stopped, before any fuzzing had begun.
guard=$((seconds + 600))
if grep -q '^ADLplug_SANITIZERS:STRING=.*\bmemory\b' "build/$preset/CMakeCache.txt" 2>/dev/null; then
  guard=$((seconds + 3600))
fi

shopt -s nullglob
manifests=("build/$preset/fuzz/"*.args)
if [ ${#manifests[@]} -eq 0 ]; then
  echo "error: build/$preset has no fuzz targets; configure it with -DADLplug_BUILD_FUZZERS=ON" >&2
  exit 1
fi

# Whether the target named is one of the names joined by commas.
named() {  # names target
  [[ ",$1," == *",$2,"* ]]
}

# Every name given to --only or --skip is a target of the build.
IFS=, read -r -a given <<< "$only$skip"
for name in ${given[@]+"${given[@]}"}; do
  if [ ! -e "build/$preset/fuzz/$name.args" ]; then
    echo "error: $name is no fuzz target of build/$preset" >&2
    exit 2
  fi
done

status=0
for manifest in "${manifests[@]}"; do
  fuzzer=${manifest%.args}
  target=$(basename "$fuzzer")
  if [ -e "$fuzzer.long" ] && [ "$with_long" -eq 0 ]; then
    echo "== $target: left to the long runs"
    continue
  fi
  if { [ -n "$only" ] && ! named "$only" "$target"; } || { [ -n "$skip" ] && named "$skip" "$target"; }; then
    echo "== $target: left to the other job"
    continue
  fi
  mapfile -t arguments < "$manifest"
  mkdir -p "$corpora/$target" "$crashes/$target"

  # The flags of the manifest, the time an input may take among them, for the
  # runs below that are not the fuzzing itself. The directories of seed and
  # regression inputs stay out of those, since libFuzzer would fuzz them.
  flags=()
  for argument in "${arguments[@]}"; do
    if [[ $argument == -* ]]; then
      flags+=("$argument")
    fi
  done

  if [ -n "$earlier" ]; then
    # What libFuzzer writes beside a failing input, and what it names it: crash-,
    # oom- and timeout- are the ones that failed, and slow-unit- is an input that
    # took long enough to be worth keeping and failed at nothing. Replaying the
    # slow ones would say every run that they are still slow, which is not a
    # failure and not news.
    failed=("$earlier/$target/"crash-* "$earlier/$target/"oom-* "$earlier/$target/"timeout-*)
    if [ ${#failed[@]} -gt 0 ]; then
      echo "== $target: ${#failed[@]} inputs that failed in an earlier run"
      # They are replayed with the flags of the manifest: without the time an
      # input may take, libFuzzer gives each input twenty minutes, and one that
      # ran out of the target's time would pass.
      code=0
      timeout --kill-after=60s "$guard" "$fuzzer" ${flags[@]+"${flags[@]}"} "${failed[@]}" || code=$?
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
    continue
  fi

  # libFuzzer replays the whole corpus it is given before it looks at the time
  # it was asked to run for, and every run adds to that corpus what it found,
  # so a corpus that only grows makes each run start later than the last: some
  # targets had come to take longer replaying theirs than a short run was asked
  # to fuzz for. The merge costs one more pass over the corpus here, and leaves
  # the next run the inputs its coverage needs rather than every input ever
  # kept. It happens beside the corpus, not in it, so that a job stopped in the
  # middle keeps the corpus it had; a merge that does not finish loses nothing
  # either.
  echo "== $target: merging the corpus down"
  merged=$(mktemp -d)
  code=0
  timeout --kill-after=60s "$guard" "$fuzzer" -merge=1 ${flags[@]+"${flags[@]}"} \
      "$merged" "$corpora/$target" || code=$?
  kept=("$corpora/$target"/*)
  if [ "$code" -eq 0 ]; then
    rm -rf "$corpora/$target"
    mv "$merged" "$corpora/$target"
    now=("$corpora/$target"/*)
    echo "== $target: ${#kept[@]} inputs merged down to ${#now[@]}"
  else
    rm -rf "$merged"
    echo "== $target: the merge did not finish (status $code), and the corpus of ${#kept[@]} inputs carries over as it is"
  fi
done
exit $status
