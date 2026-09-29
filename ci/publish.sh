#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/publish.sh fuzzing
#   ci/publish.sh assemble <artifacts> <assets> <notes>
#   ci/publish.sh release <assets> <notes>
#
# The publish job of CI on a push to main (.github/workflows/ci.yml).
#
# fuzzing stops the release while the daily fuzzing fails: the last run of
# .github/workflows/fuzz.yml on main that passed or failed decides, and a
# release goes out only when it passed or there has been none. A run that was
# cancelled or skipped decides nothing, and neither does one that never fuzzed.
# When gh cannot tell, the release waits as well.
#
# assemble first checks that <artifacts> has every pack and package the release
# is made of, for both chips: the archives of Windows and Linux for AVX2 and for
# AVX-512, that of macOS, and the deb and rpm packages for AVX2 and for AVX-512.
# A job that makes some fails when it has none to keep, except the jobs of
# AVX-512, which end without any when none of their tries found a runner with
# AVX-512 (.github/workflows/ci.yml); and a release without them would say
# nothing of what it lacks. Then it merges the packs of both chips that
# ci/package.sh made into one archive for each system and instruction set,
# ADLplug-Next-<version>-<system>.zip or .tar.xz, where <version> is the version
# to show with dashes for its + and the dot before git. With the deb and rpm
# packages of ci/package.sh and ci/package-rpm.sh, the archives go into
# <assets>, which SHA256SUMS lists. The release notes, written to <notes>, give
# the version, the commit, the commits since the tag nightly, the requirements
# and the cautions.
#
# release replaces the pre-release "Nightly" with those: the release and the tag
# nightly are deleted, and made again on the commit built ($GITHUB_SHA), with gh
# and the token of the job.
set -euo pipefail

homepage=$GITHUB_SERVER_URL/$GITHUB_REPOSITORY

assemble() {  # artifacts assets notes
  local artifacts=$1 assets=$2 notes=$3
  local work
  work=$(mktemp -d)
  mkdir -p "$assets"
  assets=$(cd "$assets" && pwd)

  local missing=() label chip package pattern
  for label in windows-x86_64-avx2 windows-x86_64-avx512 linux-x86_64-avx2 linux-x86_64-avx512 macos-arm64; do
    for chip in adl opn; do
      if ! compgen -G "$artifacts/*/$label-$chip.tar" > /dev/null; then
        missing+=("$label-$chip.tar")
      fi
    done
  done
  for package in adlplug-next opnplug-next; do
    for pattern in "${package}_*_amd64v3.deb" "${package}_*_amd64v4.deb" \
                   "$package-*.x86_64_v3.rpm" "$package-*.x86_64_v4.rpm"; do
      if ! compgen -G "$artifacts/*/$pattern" > /dev/null; then
        missing+=("$pattern")
      fi
    done
  done
  if [ ${#missing[@]} -gt 0 ]; then
    echo "error: $artifacts lacks what the release is made of, and no Nightly goes out without it:" >&2
    printf '  %s\n' "${missing[@]}" >&2
    exit 1
  fi

  shopt -s nullglob
  local packs=("$artifacts"/*/*.tar)

  # Each pack is <system>-<machine>-<chip>.tar; both chips go into one directory.
  local pack base label version display
  for pack in "${packs[@]}"; do
    base=$(basename "$pack" .tar)
    label=${base%-*}
    mkdir -p "$work/$label"
    tar -xf "$pack" -C "$work/$label"
    { read -r version; read -r display; } < "$work/$label/version.txt"
    rm "$work/$label/version.txt"
  done
  local file_version=${display/+/-}
  file_version=${file_version/.git/-git}

  local directory top
  for directory in "$work"/*/; do
    label=$(basename "$directory")
    top=ADLplug-Next-$file_version-$label
    mv "$work/$label" "$work/$top"
    case $label in
      linux-*) tar -C "$work" -cJf "$assets/$top.tar.xz" "$top" ;;
      *) (cd "$work" && zip -q -r -y -X "$assets/$top.zip" "$top") ;;
    esac
  done

  local package
  for package in "$artifacts"/*/*.deb "$artifacts"/*/*.rpm; do
    cp "$package" "$assets/"
  done
  (cd "$assets" && sha256sum -- * > SHA256SUMS)

  local previous
  previous=$(git ls-remote origin refs/tags/nightly | cut -f 1)
  {
    echo "Development builds of ADLplug-Next and OPNplug-Next, version \`$display\`,"
    echo "from commit [${GITHUB_SHA:0:7}]($homepage/commit/$GITHUB_SHA) ([run]($homepage/actions/runs/$GITHUB_RUN_ID))."
    echo
    echo "## Changes"
    echo
    if [ -n "$previous" ] && git merge-base --is-ancestor "$previous" "$GITHUB_SHA" 2> /dev/null; then
      if [ "$previous" = "$GITHUB_SHA" ]; then
        echo "None since the last Nightly."
      else
        git log --first-parent --format="- %s ([%h]($homepage/commit/%H))" "$previous..$GITHUB_SHA"
      fi
    else
      git log --first-parent --max-count 20 --format="- %s ([%h]($homepage/commit/%H))" "$GITHUB_SHA"
    fi
    echo
    cat << 'EOF'
## Files

- `*-windows-x86_64-avx2.zip`: VST3, LV2, AAX and standalone, for Windows 11 or later.
- `*-linux-x86_64-avx2.tar.xz`: VST3, LV2 and standalone, for Linux.
- `*-macos-arm64.zip`: VST3, AU, LV2, AAX and standalone, for macOS 26 or later on Apple Silicon (M1 and later).
- `adlplug-next_*_amd64v3.deb`, `opnplug-next_*_amd64v3.deb`: packages for Ubuntu 26.04 or later.
- `adlplug-next-*.x86_64_v3.rpm`, `opnplug-next-*.x86_64_v3.rpm`: packages for RHEL and AlmaLinux 10 or later and openSUSE.
- `*-windows-x86_64-avx512.zip`, `*-linux-x86_64-avx512.tar.xz`, `*_amd64v4.deb`, `*.x86_64_v4.rpm`: the same, for processors with AVX-512.
- `SHA256SUMS`: the SHA-256 of every file.

Each archive and package has the licenses of the binaries, and each plugin the terms of its instrument banks.

## Requirements and cautions

- Every x86-64 build needs a processor with AVX2 (x86-64-v3): Intel Haswell, AMD Zen or later. The AVX-512 builds (`x86_64-avx512`, `amd64v4`, `x86_64_v4`) need AVX-512 as well (x86-64-v4: AVX-512 F, BW, CD, DQ and VL), which Intel's Core processors of Ice Lake, Tiger Lake and Rocket Lake have, as do most of Intel's server processors since Skylake-SP and AMD's since Zen 4; Intel's Core processors from Alder Lake to Arrow Lake do not. A host that loads a build on a processor without what it needs crashes, even while it scans for plugins.
- Windows needs the [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) (x64).
- On Linux, the user interface runs on X11: Xwayland in a Wayland session.
- The macOS files are not notarized. Remove the quarantine before use, for example `xattr -dr com.apple.quarantine ADLplug-Next.vst3`.
- The AAX plugins have no PACE signature, so only Pro Tools Developer loads them.
- The deb packages are for Ubuntu. Debian's dpkg does not know `Architecture-Variant`, so it would install the `amd64v3` and `amd64v4` packages on any processor.
- These builds are replaced by the next Nightly. An odd minor version marks a development version.
EOF
  } > "$notes"
  cat "$notes"
  ls -l "$assets"
}

release() {  # assets notes
  local assets=$1 notes=$2
  local display
  display=$(sed -n 's/^Development builds of ADLplug-Next and OPNplug-Next, version `\(.*\)`,$/\1/p' "$notes")
  gh release delete nightly --repo "$GITHUB_REPOSITORY" --yes 2> /dev/null || true
  gh api --method DELETE "repos/$GITHUB_REPOSITORY/git/refs/tags/nightly" 2> /dev/null || true
  gh release create nightly --repo "$GITHUB_REPOSITORY" --target "$GITHUB_SHA" --prerelease \
    --title "Nightly $display" --notes-file "$notes" "$assets"/*
}

fuzzing() {
  local last
  # Only a run that fuzzed has a say. The fuzzing workflow is started by its
  # schedule or by hand, and a run of it on a push is what GitHub writes down
  # when it cannot read the workflow file itself: such a run has no jobs, fuzzed
  # nothing, and would otherwise hold back every Nightly until the next fuzzing.
  last=$(gh run list --repo "$GITHUB_REPOSITORY" --workflow fuzz.yml --branch main \
    --status completed --limit 20 --json conclusion,event,url \
    --jq '[.[] | select(.event == "schedule" or .event == "workflow_dispatch")
               | select(.conclusion == "success" or .conclusion == "failure")][0] // empty | "\(.conclusion) \(.url)"')
  case $last in
    "")
      echo "The daily fuzzing has not finished a run yet."
      ;;
    success\ *)
      echo "The daily fuzzing passed its last run: ${last#success }"
      ;;
    *)
      echo "error: the daily fuzzing failed its last run, and no Nightly goes out until a run passes: ${last#* }" >&2
      exit 1
      ;;
  esac
}

case ${1:-} in
  fuzzing) fuzzing ;;
  assemble) assemble "$2" "$3" "$4" ;;
  release) release "$2" "$3" ;;
  *) echo "usage: ci/publish.sh fuzzing | assemble <artifacts> <assets> <notes> | release <assets> <notes>" >&2; exit 2 ;;
esac
