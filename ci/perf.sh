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
# it): where the parts of a sample spend their time, by perf from Ubuntu's
# archive and the profile program (<profile>, a build of
# crates/adlplug-opl3/examples/profile.rs with the line tables, which
# ci/speed.sh makes for it), pinned to processor 0. Of the counters of the
# processor, GitHub's virtual machines give perf on the Neoverse-N2 the cycles
# and the instructions, and on AMD's and Intel's processors none (perf stat
# finds cycles not supported, and the events of the top-down method missing on
# all three). So perf stat counts the cycles and the instructions of every form
# of each part where it can, and perf record samples by the system's clock:
# over the pass, over whole samples with no write to take, over the writes'
# handling and over whole samples with their writes at every level the
# processor has, and over the pass of the variant all where the level has one.
# The report (<report>, the summary of the run) takes the counts, the functions
# that took the most of the samples, the forty instructions that took the most
# of the first of them, or for whole samples of the first besides the pass
# written by hand, and for the writes' handling and whole samples the lines of
# the source that took the most; the log takes the whole of each annotation.
# Whether the counters are there is asked once and printed, so that a runner
# that gets them shows it. What the runner does not allow is said and left out:
# nothing here fails the job.
set -uo pipefail

profile=$1
report=$2
work=$(mktemp -d)
pin=(taskset -c 0)
# Enough iterations for some ten thousand samples of a part at the rate below.
iterations=4000000

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
  echo "the processor's counters, as perf stat finds cycles and instructions:"
  sudo perf stat -x, -e cycles,instructions -- true 2>&1 | cut -d, -f1,3
  echo '```'
} >> "$report"

# perf record over one part, perf report and perf annotate of it: the level,
# the part, the variant or nothing.
record() {
  local level=$1 part=$2 variant=${3:-}
  local name="$level, $part${variant:+, the variant $variant}"
  local data=$work/record.data
  if ! sudo perf record -q -F 10000 -o "$data" -- \
    "${pin[@]}" "$profile" run "$level" "$part" "$iterations" ${variant:+"$variant"} > "$work/record.log" 2>&1; then
    {
      echo
      echo "perf record failed for $name:"
      echo
      echo '```'
      tail -n 5 "$work/record.log"
      echo '```'
    } >> "$report"
    return
  fi
  local functions=$work/functions.txt
  sudo perf report -i "$data" --stdio --no-children --sort symbol --percent-limit 0.5 2> /dev/null |
    grep -vE '^\s*(#|$)' > "$functions"
  # The function that took the most of the samples, the third field of its
  # line: its share, its kind ([.]) and its name, which has no spaces; for
  # whole samples the first besides the pass written by hand, where the rest of
  # a sample goes.
  local symbol
  case "$part" in
    samples | quiet) symbol=$(awk '$3 !~ /^adlplug_opl3_pass/ { print $3; exit }' "$functions") ;;
    *) symbol=$(awk 'NR == 1 { print $3; exit }' "$functions") ;;
  esac
  local annotation=$work/annotation.txt
  sudo perf annotate -i "$data" --stdio --no-source "$symbol" > "$annotation" 2> /dev/null
  echo "== perf annotate: $name: $symbol"
  cat "$annotation"
  {
    echo
    echo "##### perf record: $name"
    echo
    echo '```'
    cat "$functions"
    echo
    echo "the forty instructions of $symbol that took the most of its samples:"
    awk -F ':' '$1 ~ /^ *[0-9]+\.[0-9]+ *$/ && $1 + 0 > 0 {
      percent = $1 + 0; sub(/^[^:]*: */, ""); printf "%6.2f  %s\n", percent, $0
    }' "$annotation" | sort -rn | head -n 40
    if [ "$part" != pass ]; then
      echo
      echo "the lines of the source that took the most of the samples:"
      sudo perf report -i "$data" --stdio --no-children --sort srcline --percent-limit 1 2> /dev/null |
        grep -vE '^\s*(#|$)' | head -n 40
    fi
    echo '```'
  } >> "$report"
}

# Where the virtual machine lets perf read the processor's cycles and
# instructions (on GitHub's runners the Neoverse-N2's), each part's per pass:
# the counts of a run of the part less those of a run of none, which is the
# program's setting up and the writes before the part, over the part's
# iterations. The pass, a whole sample with no write to take, the writes'
# handling and a whole sample with its writes, by the level's own code; and
# whole samples with their writes by each variant, which changes the writes'
# handling in ways that show only with writes between.
count_iterations=1000000
counts() {  # level part variant: the two counts of the run, cycles first
  sudo perf stat -x, -e cycles,instructions -- \
    "${pin[@]}" "$profile" run "$1" "$2" "$3" ${4:+"$4"} 2>&1 > /dev/null |
    awk -F, '$3 == "cycles" { c = $1 } $3 == "instructions" { i = $1 } END { print c + 0, i + 0 }'
}
count() {  # level part variant
  local full none
  full=$(counts "$1" "$2" "$count_iterations" "${3:-}")
  none=$(counts "$1" "$2" 0 "${3:-}")
  # The ratio in parentheses, as a > among printf's arguments would send its
  # output to a file.
  awk -v full="$full" -v none="$none" -v n="$count_iterations" -v name="$1, $2${3:+, $3}" 'BEGIN {
    split(full, f, " "); split(none, z, " ")
    cycles = (f[1] - z[1]) / n; instructions = (f[2] - z[2]) / n
    printf "%-44s %9.0f %9.0f %6.2f\n", name, cycles, instructions, (cycles > 0 ? instructions / cycles : 0)
  }'
}
if sudo perf stat -x, -e cycles -- true 2>&1 | grep -q '^[0-9]'; then
  {
    echo
    echo '##### perf stat: cycles and instructions of a pass'
    echo
    echo '```'
    printf '%-44s %9s %9s %6s\n' 'level, part, variant' cycles insns IPC
    for level in "${levels[@]}"; do
      for part in pass quiet writes samples; do
        count "$level" "$part"
      done
      while IFS= read -r variant; do
        count "$level" samples "$variant"
      done < <("$profile" variants "$level")
    done
    echo '```'
  } >> "$report"
fi

for level in "${levels[@]}"; do
  record "$level" pass
  record "$level" quiet
  record "$level" writes
  record "$level" samples
  # grep reads to the end, so that the profile program is never cut off, which
  # under pipefail would fail the test.
  if "$profile" variants "$level" | grep -x all > /dev/null; then
    record "$level" pass all
  fi
done
sudo rm -rf "$work"
