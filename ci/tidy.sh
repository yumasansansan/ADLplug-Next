#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/tidy.sh <preset>
#
# Runs clang-tidy over the project's own code, with the checks that .clang-tidy
# names, on the compile commands of build/<preset> (every preset writes them).
# Each file of sources/, tests/, fuzz/ and tools/ that the build compiles is
# looked at once, however many targets compile it, and the headers of those
# directories are looked at along with them. Nothing of thirdparty/ is reported:
# the submodules carry their own projects' code, which this project does not
# write, and a fault of theirs is fixed by a patch of patches/, found by the
# sanitizers and the fuzzing -- which read what the code does rather than how it
# is written. The static analyser looks inside their headers all the same, since
# the files of this project include them, so they are excluded by name as well.
#
# clang-tidy reads each file with the very options the build compiles it with,
# so what the build generates has to be there already: run ci/build.sh on the
# preset first. The checks are the ones this project is clean of, and
# .clang-tidy makes every one of them an error, so anything reported fails the
# script -- which is what the static analysis job of every push is.
set -euo pipefail

preset=${1:-}
if [ -z "$preset" ]; then
  echo "usage: ci/tidy.sh <preset>" >&2
  exit 2
fi

build=build/$preset
if [ ! -f "$build/compile_commands.json" ]; then
  echo "error: $build has no compile commands; configure and build the preset first (ci/build.sh)" >&2
  exit 1
fi

# The paths of this project's own code, as an anchored pattern: clang-tidy takes
# regular expressions for the files to read and for the headers to report in,
# and both are matched against the whole path.
root=$(pwd -P)
ours="^$root/(sources|tests|fuzz|tools)/"

jobs=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
echo "== $(clang-tidy --version | sed -n 's/^.*LLVM version/LLVM/p' | head -n 1), $jobs files at a time, on $build"
clang-tidy --version | sed -n 's/^ *//p' | head -n 4

# A file that several targets compile has a compile command for each of them --
# the plugin's own sources have one per format, and the harnesses one per fuzz
# target -- and clang-tidy reads the file once for every command it finds. The
# checks say the same thing every time, so the commands are reduced to the first
# of each file, which is the difference between minutes and half an hour.
commands=$(mktemp -d)
trap 'rm -rf "$commands"' EXIT
python3 - "$build/compile_commands.json" "$commands/compile_commands.json" <<'REDUCE'
import json, sys
first = {}
for entry in json.load(open(sys.argv[1])):
    first.setdefault(entry["file"], entry)
json.dump(list(first.values()), open(sys.argv[2], "w"))
print("== %d compile commands, %d files" % (len(json.load(open(sys.argv[1]))), len(first)))
REDUCE

# run-clang-tidy comes with the toolchain, reads the compile commands, and takes
# the files of ours from them as a set. It ends with a status of its own when any
# file was reported.
run-clang-tidy -p "$commands" -j "$jobs" -quiet -use-color=0 \
  -header-filter="$ours" -exclude-header-filter="/thirdparty/" "$ours"
