#!/usr/bin/env bash
# Part of ADLplug, distributed under the GNU GPL v3 or later.
#               (See accompanying file LICENSE.)
#
# Sets up a GitHub Actions runner to build ADLplug: Clang, LLD and the LLVM
# tools of one pinned version, put first on PATH for the later steps, and on
# Linux the development packages that JUCE needs. Every download is checked:
# the apt.llvm.org signing key by its fingerprint, LLVM's release archives by
# their SHA-256.
#
# To move to another LLVM version, change the values below. GitHub lists the
# SHA-256 of each release archive as the asset's digest.
set -euo pipefail

llvm_major=23

# Windows and macOS: LLVM's release archives.
llvm_release=23.1.1
windows_archive=clang+llvm-$llvm_release-x86_64-pc-windows-msvc.tar.zst
windows_sha256=c8a12d754b5050c5668b56a5425c806792d46c70f7244b1216046164aa4b6462
macos_archive=LLVM-$llvm_release-macOS-ARM64.tar.zst
macos_sha256=2c4a0fdd1ec6a32d4fd57ff32aa714ec8b3c71bf02a24ec38608a4f23f8aca89

# Linux: the packages of apt.llvm.org, signed with this key.
apt_key_fingerprint=6084F3CF814B57C1CF12EFD515CF4D18AF4F7421

# Development packages for the JUCE modules ADLplug uses, from JUCE's
# docs/Linux Dependencies.md. curl and WebKit are left out: the build disables
# both.
linux_packages=(
  libasound2-dev
  libfontconfig1-dev libfreetype-dev
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxi-dev
  libxinerama-dev libxrandr-dev libxrender-dev
)

# Pipelines below are written so that no command stops reading early: with
# pipefail, a writer killed by SIGPIPE would fail the script.

download() {  # url file sha256
  curl --fail --location --silent --show-error --retry 3 --output "$2" "$1"
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

# Extracts a release archive into a directory, leaving out the static
# libraries at the top of lib/, which are for programs built on LLVM and take
# up most of the space. The compiler runtime lies deeper, in lib/clang/.
extract() {  # archive directory tar static-library-suffix
  mkdir -p "$2"
  zstd --decompress --stdout "$1" |
    "$3" -x -f - -C "$2" --strip-components 1 --no-wildcards-match-slash --exclude "*/lib/*$4"
  rm -f "$1"
}

setup_linux() {
  local key=$RUNNER_TEMP/apt.llvm.org.asc
  local keyring=/usr/share/keyrings/apt.llvm.org.gpg
  curl --fail --location --silent --show-error --retry 3 --output "$key" \
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
  sudo apt-get install -y -qq --no-install-recommends \
    "clang-$llvm_major" "lld-$llvm_major" "llvm-$llvm_major" "${linux_packages[@]}"
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
  # The plugins load the system's libc++, so they are compiled against the
  # SDK's libc++ headers. Clang would take the newer headers that come with
  # the toolchain first.
  rm -rf "$RUNNER_TEMP/llvm/include/c++"
  llvm_bin=$RUNNER_TEMP/llvm/bin
}

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
cmake --version
ninja --version

if [ "$RUNNER_OS" = Windows ]; then
  cygpath --windows "$llvm_bin" >> "$GITHUB_PATH"
else
  echo "$llvm_bin" >> "$GITHUB_PATH"
fi
