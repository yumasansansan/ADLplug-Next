#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/perf.sh <profile> <report>
#
# For rust-speed's experiments, on Linux under GitHub Actions (ci/speed.sh runs
# it): perf, from Ubuntu's archive, over each part of a sample at every level
# the processor has, by the profile program (<profile>, built from
# crates/adlplug-opl3/examples/profile.rs) pinned to processor 0. perf stat
# counts what the processor did, by its own counters where the virtual machine
# lets the system read them and by the system's clock where it does not, with
# the metrics of the first level of the top-down method where perf has them for
# the processor; perf record samples where the pass spends its time, and perf
# annotate tells it instruction by instruction. The report (<report>, the
# summary of the run) takes the counts, the functions that took the most of the
# samples and the forty instructions that took the most; the log takes the
# whole of each annotation. What the runner does not allow is said and left
# out: nothing here fails the job.
set -uo pipefail

profile=$1
report=$2
work=$(mktemp -d)
pin=(taskset -c 0)
# Enough iterations for a second or so of counting, and for some ten thousand
# samples of a part at the rate below.
stat_iterations=1000000
record_iterations=4000000

# perf: Ubuntu's linux-perf where the archive has it, the tools of the
# runner's kernel otherwise.
install() {
  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends "$@" > "$work/apt.log" 2>&1 &&
    sudo perf version > /dev/null 2>&1
}
if ! install linux-perf && ! install "linux-tools-$(uname -r)"; then
  {
    echo
    echo '#### perf'
    echo
    echo 'perf could not be installed from the archive:'
    echo
    echo '```'
    tail -n 5 "$work/apt.log"
    echo '```'
  } >> "$report"
  exit 0
fi

levels=(portable)
while IFS= read -r level; do
  levels+=("$level")
done < <("$profile" levels)

{
  echo
  echo '#### perf'
  echo
  echo '```'
  sudo perf version
  echo "perf_event_paranoid: $(cat /proc/sys/kernel/perf_event_paranoid)"
  echo "the processor's events that perf knows: $(sudo perf list --raw-dump hw cache 2> /dev/null | wc -w)"
  echo '```'
} >> "$report"

for level in "${levels[@]}"; do
  for part in pass writes; do
    out=$work/stat-$level-$part.txt
    {
      echo "== perf stat -d: $level, $part"
      sudo perf stat -d -- "${pin[@]}" "$profile" run "$level" "$part" "$stat_iterations" 2>&1
      echo "== perf stat -M TopdownL1: $level, $part"
      sudo perf stat -M TopdownL1 -- "${pin[@]}" "$profile" run "$level" "$part" "$stat_iterations" 2>&1 ||
        echo "(perf has no TopdownL1 here)"
    } > "$out"
    cat "$out"
    {
      echo
      echo "##### perf stat: $level, $part"
      echo
      echo '```'
      # The counts and the metrics, without the lines perf frames them with.
      grep -vE '^\s*$|Performance counter stats|seconds (time elapsed|user|sys)' "$out"
      echo '```'
    } >> "$report"
  done
done

for level in "${levels[@]}"; do
  data=$work/record-$level.data
  if ! sudo perf record -q -F 10000 -o "$data" -- "${pin[@]}" "$profile" run "$level" pass "$record_iterations" \
    > "$work/record.log" 2>&1; then
    {
      echo
      echo "perf record failed for $level:"
      echo
      echo '```'
      tail -n 5 "$work/record.log"
      echo '```'
    } >> "$report"
    continue
  fi
  functions=$work/functions-$level.txt
  sudo perf report -i "$data" --stdio --no-children --sort symbol --percent-limit 0.5 2> /dev/null |
    grep -vE '^\s*(#|$)' > "$functions"
  # The function that took the most of the samples: the pass written by hand at
  # a level of its own, the core's own functions for the portable pass.
  symbol=$(awk 'NR == 1 { $1 = ""; $2 = ""; sub(/^ +/, ""); print; exit }' "$functions")
  annotation=$work/annotate-$level.txt
  sudo perf annotate -i "$data" --stdio --no-source "$symbol" > "$annotation" 2> /dev/null
  echo "== perf annotate: $level, pass, $symbol"
  cat "$annotation"
  {
    echo
    echo "##### perf record: $level, pass"
    echo
    echo '```'
    cat "$functions"
    echo
    echo "the forty instructions of $symbol that took the most of its samples:"
    awk -F ':' '$1 ~ /^ *[0-9]+\.[0-9]+ *$/ && $1 + 0 > 0 {
      percent = $1 + 0; sub(/^[^:]*: */, ""); printf "%6.2f  %s\n", percent, $0
    }' "$annotation" | sort -rn | head -n 40
    echo '```'
  } >> "$report"
done
sudo rm -rf "$work"
