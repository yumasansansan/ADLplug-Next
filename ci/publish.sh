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
# fuzzing stops the release while the daily fuzzing fails (plan D48): the last
# run of .github/workflows/fuzz.yml on main that passed or failed decides, and a
# release goes out only when it passed or there has been none. A run that was
# cancelled or skipped decides nothing. When gh cannot tell, the release waits
# as well.
#
# assemble merges the packs of both chips that ci/package.sh made into one
# archive for each system and instruction set, ADLplug-Next-<version>-<system>
# .zip or .tar.xz, where <version> is the version to show with dashes for its +
# and the dot before git (plan D43). With the deb and rpm packages of
# ci/package.sh and ci/package-rpm.sh, the archives go into <assets>, which
# SHA256SUMS lists. The release notes, written to <notes>, give the version, the
# commit, the commits since the tag nightly, the requirements and the cautions.
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

  shopt -s nullglob
  local packs=("$artifacts"/*/*.tar)
  if [ ${#packs[@]} -eq 0 ]; then
    echo "error: $artifacts has no packs" >&2
    exit 1
  fi

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

- `*-windows-x86_64.zip`, `*-windows-x86_64-avx2.zip`: VST3, LV2, AAX and standalone, for Windows 11 or later.
- `*-linux-x86_64.tar.xz`, `*-linux-x86_64-avx2.tar.xz`: VST3, LV2 and standalone, for Linux.
- `*-macos-arm64.zip`: VST3, AU, LV2, AAX and standalone, for macOS 26 or later on Apple Silicon.
- `adlplug-next_*.deb`, `opnplug-next_*.deb`: packages for Ubuntu 26.04 or later; `amd64v3` is the AVX2 build.
- `adlplug-next-*.rpm`, `opnplug-next-*.rpm`: packages for RHEL and AlmaLinux 10 or later and openSUSE; `x86_64_v3` is the AVX2 build.
- `SHA256SUMS`: the SHA-256 of every file.

Each archive and package has the licenses of the binaries, and each plugin the terms of its instrument banks.

## Requirements and cautions

- The AVX2 builds (`x86_64-avx2`, `amd64v3`, `x86_64_v3`) need a processor with AVX2 (x86-64-v3). A host that loads one on another processor crashes, even while it scans for plugins.
- Windows needs the [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) (x64).
- On Linux, the user interface runs on X11: Xwayland in a Wayland session.
- The macOS files are not notarized. Remove the quarantine before use, for example `xattr -dr com.apple.quarantine ADLplug-Next.vst3`.
- The AAX plugins have no PACE signature, so only Pro Tools Developer loads them.
- The deb packages are for Ubuntu. Debian's dpkg does not know `Architecture-Variant`, so it would install the `amd64v3` packages on any processor.
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
  last=$(gh run list --repo "$GITHUB_REPOSITORY" --workflow fuzz.yml --branch main \
    --status completed --limit 20 --json conclusion,url \
    --jq '[.[] | select(.conclusion == "success" or .conclusion == "failure")][0] // empty | "\(.conclusion) \(.url)"')
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
