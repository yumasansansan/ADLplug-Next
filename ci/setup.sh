#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/setup.sh
#
# Sets up a GitHub Actions runner to build and test ADLplug-Next: Clang, LLD and
# the LLVM tools of one pinned version, first on PATH for the later steps, and
# the latest stable Rust. Clang is the linker driver and LLD the linker on every
# system (.cargo/config.toml), and Clang assembles the passes written in
# assembly. The runner images carry an older LLVM, or none, so the toolchain is
# fetched every time:
#   - Windows and macOS: LLVM's release archives, checked by their SHA-256.
#   - Ubuntu: the packages of apt.llvm.org, whose signing key is checked by its
#     fingerprint. What apt.llvm.org offers for a release is a snapshot of its
#     branch, rebuilt often, so its version can run ahead of the archives'.
# On macOS, SDKROOT names the SDK for whatever runs Clang as a linker (cargo).
#
# To move to another version of LLVM, change the values below. GitHub lists the
# SHA-256 of release archives as the asset's digest.
set -euo pipefail

llvm_major=23
llvm_release=23.1.5
windows_archive=clang+llvm-$llvm_release-x86_64-pc-windows-msvc.tar.zst
windows_sha256=b2be33385ddc86e83511417a060de927ae8b09d1f3017cc3cdce5c73cd5029d7
macos_archive=LLVM-$llvm_release-macOS-ARM64.tar.zst
macos_sha256=503de30ef90311e0b143623801b19af2250adac6d69faed65ad0051170202e3b
apt_key_fingerprint=6084F3CF814B57C1CF12EFD515CF4D18AF4F7421

# Pipelines below are written so that no command stops reading early: with
# pipefail, a writer killed by SIGPIPE would fail the script.

download() {  # url file sha256
  curl --fail --location --silent --show-error --retry 3 --retry-all-errors --output "$2" "$1"
  local actual
  if command -v sha256sum > /dev/null; then
    actual=$(sha256sum "$2")
  else
    actual=$(shasum -a 256 "$2")
  fi
  actual=${actual%% *}
  if [ "$actual" != "$3" ]; then
    echo "error: $2 has SHA-256 $actual, expected $3" >&2
    exit 1
  fi
}

release_url() {  # archive
  local name=${1//+/%2B}
  echo "https://github.com/llvm/llvm-project/releases/download/llvmorg-$llvm_release/$name"
}

# Extracts a release archive, leaving out the static libraries at the top of
# lib/, which are for programs built on LLVM. The compiler runtime lies deeper,
# in lib/clang/. LLVM compresses the archives with a window of 1 GiB, which zstd
# decompresses only when told it may (--long=30).
extract() {  # archive directory tar static-library-suffix
  mkdir -p "$2"
  zstd --decompress --long=30 --stdout "$1" |
    "$3" -x -f - -C "$2" --strip-components 1 --no-wildcards-match-slash --exclude "*/lib/*$4"
  rm -f "$1"
}

setup_linux() {
  local key=$RUNNER_TEMP/apt.llvm.org.asc
  local keyring=/usr/share/keyrings/apt.llvm.org.gpg
  curl --fail --location --silent --show-error --retry 3 --retry-all-errors --output "$key" \
    https://apt.llvm.org/llvm-snapshot.gpg.key
  local fingerprint
  fingerprint=$(gpg --show-keys --with-colons "$key" | awk -F : '$1 == "fpr" && !found { print $10; found = 1 }')
  if [ "$fingerprint" != "$apt_key_fingerprint" ]; then
    echo "error: the apt.llvm.org key has fingerprint $fingerprint, expected $apt_key_fingerprint" >&2
    exit 1
  fi
  gpg --dearmor < "$key" | sudo tee "$keyring" > /dev/null
  local codename
  codename=$(. /etc/os-release && echo "$VERSION_CODENAME")
  echo "deb [signed-by=$keyring] https://apt.llvm.org/$codename/ llvm-toolchain-$codename-$llvm_major main" |
    sudo tee /etc/apt/sources.list.d/apt.llvm.org.list > /dev/null
  sudo apt-get update -qq
  sudo apt-get install -y -qq --no-install-recommends "clang-$llvm_major" "lld-$llvm_major" "llvm-$llvm_major"
  llvm_bin=/usr/lib/llvm-$llvm_major/bin
}

setup_windows() {
  local temp
  temp=$(cygpath --unix "$RUNNER_TEMP")
  download "$(release_url "$windows_archive")" "$temp/$windows_archive" "$windows_sha256"
  extract "$temp/$windows_archive" "$temp/llvm" tar .lib
  llvm_bin=$temp/llvm/bin
}

setup_macos() {
  download "$(release_url "$macos_archive")" "$RUNNER_TEMP/$macos_archive" "$macos_sha256"
  extract "$RUNNER_TEMP/$macos_archive" "$RUNNER_TEMP/llvm" gtar .a
  rm -rf "$RUNNER_TEMP/llvm/include/c++"
  llvm_bin=$RUNNER_TEMP/llvm/bin
  echo "SDKROOT=$(xcrun --show-sdk-path)" >> "$GITHUB_ENV"
}

llvm_bin=
case "${RUNNER_OS:-}" in
  Linux) setup_linux ;;
  Windows) setup_windows ;;
  macOS) setup_macos ;;
  *) echo "error: unsupported runner '${RUNNER_OS:-}'" >&2; exit 1 ;;
esac

version=$("$llvm_bin/clang" --version)
version=${version%%$'\n'*}
echo "$version"
case "$version" in
  *"clang version $llvm_major."*) ;;
  *) echo "error: expected Clang $llvm_major" >&2; exit 1 ;;
esac
# Rust: the latest stable, which rust-toolchain.toml names. A runner's rustup
# holds the stable of its image, which "stable" would take as it is, older than
# the latest; updated here, every run builds with the latest, as a developer's
# does after rustup update.
rustup update stable --no-self-update
rustc --version
echo "== the processor of this runner"
case "$RUNNER_OS" in
  macOS) sysctl -n machdep.cpu.brand_string ;;
  *) grep -m 1 '^model name' /proc/cpuinfo || true ;;
esac

if [ "$RUNNER_OS" = Windows ]; then
  cygpath --windows "$llvm_bin"
else
  echo "$llvm_bin"
fi >> "$GITHUB_PATH"
