#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
# Sets up a GitHub Actions runner to build and test ADLplug-Next: Clang, LLD and the
# LLVM tools of one pinned version, pluginval, and on Linux the development
# packages that JUCE needs, the tools for the tests and lv2lint. In the
# AlmaLinux container of the Nightly workflow, which builds the rpm packages,
# it sets up LLVM and the development packages alone, as root. The tools go
# first on PATH for the later steps. Every download is checked: the
# apt.llvm.org signing key by its fingerprint, archives by their SHA-256, and
# lv2lint's source by its commit.
#
# To move to another version of a tool, change the values below. GitHub lists
# the SHA-256 of release archives as the asset's digest; pluginval's releases
# are older than that, so their SHA-256 were taken from the downloads.
set -euo pipefail

llvm_major=23

# Windows, macOS and the AlmaLinux container: LLVM's release archives.
llvm_release=23.1.1
windows_archive=clang+llvm-$llvm_release-x86_64-pc-windows-msvc.tar.zst
windows_sha256=c8a12d754b5050c5668b56a5425c806792d46c70f7244b1216046164aa4b6462
macos_archive=LLVM-$llvm_release-macOS-ARM64.tar.zst
macos_sha256=2c4a0fdd1ec6a32d4fd57ff32aa714ec8b3c71bf02a24ec38608a4f23f8aca89
linux_archive=LLVM-$llvm_release-Linux-X64.tar.zst
linux_sha256=b7ddbabd70fa1d206948bc83f59e59aa84eaf4cd09b6b89cc3ece28177710a6f

# Linux: the packages of apt.llvm.org, signed with this key.
apt_key_fingerprint=6084F3CF814B57C1CF12EFD515CF4D18AF4F7421

# pluginval, which loads the plugins as hosts do and tests them.
pluginval_version=v1.0.4
pluginval_windows_sha256=c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab
pluginval_linux_sha256=c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352
pluginval_macos_sha256=3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f

# lv2lint, which checks LV2 bundles, is not packaged by Ubuntu. It is built
# from its 0.16.2 release, at the commit of that tag in the sfztools mirror.
lv2lint_repository=https://github.com/sfztools/lv2lint.git
lv2lint_commit=ea7126042356d245610ecf7a56354dd196fafff7

# Development packages for the JUCE modules ADLplug uses, from JUCE's
# docs/Linux Dependencies.md. curl and WebKit are left out: the build disables
# both.
linux_packages=(
  libasound2-dev
  libfontconfig1-dev libfreetype-dev
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxi-dev
  libxinerama-dev libxrandr-dev libxrender-dev
)
# For the tests (ci/test.sh): xwfb-run, which runs a command on Xwayland under
# a headless Weston, a window manager for that Xwayland
# (ci/with-window-manager.sh), and what lv2lint is built with.
linux_packages+=(xwayland-run weston xwayland xauth openbox)
linux_packages+=(meson liblilv-dev lv2-dev libelf-dev)

# The same development packages in AlmaLinux 10, from its BaseOS, AppStream and
# CRB repositories, with what the build needs besides: the headers and the
# runtime of libstdc++, which come with GCC's C++ package (Clang compiles and
# links), and rpmbuild.
almalinux_packages=(
  alsa-lib-devel fontconfig-devel freetype-devel
  libX11-devel libXcomposite-devel libXcursor-devel libXext-devel libXi-devel
  libXinerama-devel libXrandr-devel libXrender-devel
  cmake ninja-build gcc-c++ git python3 tar xz zstd rpm-build
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

# Downloads pluginval into <directory>/pluginval.
setup_pluginval() {  # system sha256 directory
  local archive=$3/pluginval.zip
  download "https://github.com/Tracktion/pluginval/releases/download/$pluginval_version/pluginval_$1.zip" "$archive" "$2"
  unzip -q -o "$archive" -d "$3/pluginval"
  rm -f "$archive"
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

  setup_pluginval Linux "$pluginval_linux_sha256" "$RUNNER_TEMP"
  chmod +x "$RUNNER_TEMP/pluginval/pluginval"
  tool_paths+=("$RUNNER_TEMP/pluginval")

  # lv2lint, with Clang and LLD like everything else.
  local source=$RUNNER_TEMP/lv2lint
  git -c init.defaultBranch=main init --quiet "$source"
  git -C "$source" fetch --quiet --depth 1 "$lv2lint_repository" "$lv2lint_commit"
  git -C "$source" checkout --quiet --detach FETCH_HEAD
  if [ "$(git -C "$source" rev-parse HEAD)" != "$lv2lint_commit" ]; then
    echo "error: lv2lint is not at commit $lv2lint_commit" >&2
    exit 1
  fi
  # lv2lint enables LTO by default. It is turned off: the archiver meson would
  # pick up is binutils' ar, which cannot index Clang's bitcode.
  PATH=$llvm_bin:$PATH CC=clang CC_LD=lld meson setup --buildtype release -Db_lto=false \
    -Delf-tests=enabled -Dx11-tests=enabled "$source/build" "$source"
  PATH=$llvm_bin:$PATH ninja -C "$source/build"
  tool_paths+=("$source/build")
}

setup_windows() {
  local temp
  temp=$(cygpath --unix "$RUNNER_TEMP")
  download "$(release_url "$windows_archive")" "$temp/$windows_archive" "$windows_sha256"
  extract "$temp/$windows_archive" "$temp/llvm" tar .lib
  llvm_bin=$temp/llvm/bin

  setup_pluginval Windows "$pluginval_windows_sha256" "$temp"
  tool_paths+=("$temp/pluginval")
}

setup_macos() {
  download "$(release_url "$macos_archive")" "$RUNNER_TEMP/$macos_archive" "$macos_sha256"
  extract "$RUNNER_TEMP/$macos_archive" "$RUNNER_TEMP/llvm" gtar .a
  # The plugins load the system's libc++, so they are compiled against the
  # SDK's libc++ headers. Clang would take the newer headers that come with
  # the toolchain first.
  rm -rf "$RUNNER_TEMP/llvm/include/c++"
  llvm_bin=$RUNNER_TEMP/llvm/bin

  setup_pluginval macOS "$pluginval_macos_sha256" "$RUNNER_TEMP"
  tool_paths+=("$RUNNER_TEMP/pluginval/pluginval.app/Contents/MacOS")
}

setup_almalinux() {
  dnf install -y -q --setopt=install_weak_deps=False "${almalinux_packages[@]}"
  download "$(release_url "$linux_archive")" "$RUNNER_TEMP/$linux_archive" "$linux_sha256"
  extract "$RUNNER_TEMP/$linux_archive" "$RUNNER_TEMP/llvm" tar .a
  llvm_bin=$RUNNER_TEMP/llvm/bin
}

tool_paths=()
case "${RUNNER_OS:-}" in
  Linux)
    if [ -f /etc/almalinux-release ]; then
      setup_almalinux
    else
      setup_linux
    fi
    ;;
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

for path in "$llvm_bin" ${tool_paths[@]+"${tool_paths[@]}"}; do
  if [ "$RUNNER_OS" = Windows ]; then
    cygpath --windows "$path"
  else
    echo "$path"
  fi
done >> "$GITHUB_PATH"
