#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/targets.sh [test]
#
# Builds the workspace for the levels of x86-64 above the one it is shipped for
# (x86-64-v2, .cargo/config.toml), as distributions that raise their baseline
# build it: x86-64-v3, with vectors of 256 bits, and x86-64-v4 with vectors of
# 512 bits, as .cargo/config.toml has every x86-64 build take them where there
# is AVX-512 (its flags come first, and these after them). The portable form of
# a pass is then vector code of that width. The target is named, so that build
# scripts, which run here, are built for this processor; the builds go to
# target/targets/.
#
# With `test`, each build's tests run too, where the processor has what the
# build needs: AVX2, FMA, BMI1, BMI2, F16C, LZCNT and MOVBE for x86-64-v3, and
# AVX-512 F, BW, CD, DQ and VL as well for x86-64-v4.
set -euo pipefail

case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*) triple=x86_64-pc-windows-msvc ;;
Linux) triple=x86_64-unknown-linux-gnu ;;
*)
    echo "error: the builds for x86-64-v3 and v4 are for x86-64 Windows and Linux" >&2
    exit 1
    ;;
esac

# Whether /proc/cpuinfo, which MSYS2 has on Windows too, lists every flag.
has() {
    local flag
    for flag in "$@"; do
        grep -qw "$flag" /proc/cpuinfo 2>/dev/null || return 1
    done
}

builds=(
    'v3 ["-C", "target-cpu=x86-64-v3"] avx2 fma bmi1 bmi2 f16c abm movbe'
    'v4 ["-C", "target-cpu=x86-64-v4"] avx2 fma bmi1 bmi2 f16c abm movbe avx512f avx512bw avx512cd avx512dq avx512vl'
)
for build in "${builds[@]}"; do
    name=${build%% *}
    rest=${build#* }
    flags=${rest%]*}]
    read -r -a needs <<<"${rest##*] }"
    arguments=(--release --locked --workspace --target "$triple" --target-dir target/targets
        --config "target.$triple.rustflags=$flags")
    echo "== x86-64-$name: $flags"
    cargo build "${arguments[@]}" --all-targets
    if [[ "${1:-}" == test ]]; then
        if has "${needs[@]}"; then
            cargo test "${arguments[@]}"
        else
            echo "note: this processor has not all of ${needs[*]}; the build is not tested"
        fi
    fi
done
