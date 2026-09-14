#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/package.sh <preset> <baseline|avx2|arm64> <directory>
#
# Packs a Release build of ci/build.sh for the Nightly workflow
# (.github/workflows/nightly.yml) into <directory>:
#  - <system>-<machine>-<chip>.tar: the plugins and the standalone program as
#    the build made them, the texts of the banks, the licenses
#    (ci/licenses.py) and version.txt, which ci/publish.sh merges with the
#    other chip's into the archive of the system;
#  - on Linux, the deb package for Ubuntu 26.04 or later (plan D59) of what the
#    build installs under /usr. The AVX2 build is the amd64v3 variant of the
#    same package.
set -euo pipefail

preset=$1
arch=$2
out=$3

build=build/$preset
# Absolute, because each -C of tar is taken from the directory of the one before.
artefacts=$(cd "$build/ADLplug_artefacts/Release" && pwd)
{ read -r version; read -r display; } < <(tr -d '\r' < "$build/version.txt")

case $preset in
  adl-release)
    name=ADLplug-Next
    summary="FM synthesizer plugins emulating the OPL3 chip"
    chips="the Yamaha YMF262 (OPL3) and its relatives, through libADLMIDI"
    ;;
  opn-release)
    name=OPNplug-Next
    summary="FM synthesizer plugins emulating the OPN2 chip"
    chips="the Yamaha YM2612 (OPN2) and YM2608 (OPNA), through libOPNMIDI"
    ;;
  *) echo "error: '$preset' is not a Release preset" >&2; exit 2 ;;
esac
chip=${preset%%-*}
package=$(tr '[:upper:]' '[:lower:]' <<< "$name")

case "$(uname -s)" in
  Linux) system=linux ;;
  Darwin) system=macos ;;
  MINGW* | MSYS*) system=windows ;;
  *) echo "error: unknown system $(uname -s)" >&2; exit 2 ;;
esac
case $arch in
  baseline) machine=x86_64 ;;
  avx2) machine=x86_64-avx2 ;;
  arm64) machine=arm64 ;;
  *) echo "error: unknown instruction set '$arch'" >&2; exit 2 ;;
esac

python=python3
if [ "$system" = windows ]; then
  python=python
fi
mkdir -p "$out"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# The bundles and the program.
contents=()
add() {  # directory name
  if [ ! -e "$1/$2" ]; then
    echo "error: $1/$2 is missing" >&2
    exit 1
  fi
  contents+=(-C "$1" "$2")
}
add "$artefacts/VST3" "$name.vst3"
add "$artefacts/LV2" "$name.lv2"
case $system in
  windows)
    add "$artefacts/AAX" "$name.aaxplugin"
    add "$artefacts/Standalone" "$name.exe"
    ;;
  macos)
    add "$artefacts/AU" "$name.component"
    add "$artefacts/AAX" "$name.aaxplugin"
    add "$artefacts/Standalone" "$name.app"
    ;;
  linux)
    add "$artefacts/Standalone" "$name"
    ;;
esac

# The texts of the banks, the licenses and the version.
mkdir "$work/extra"
cp "$build/banks/$name-banks.txt" "$work/extra/"
"$python" ci/licenses.py "$build" "$work/extra/licenses"
printf '%s\n%s\n' "$version" "$display" > "$work/extra/version.txt"

# macOS's tar would store extended attributes as extra ._ files.
export COPYFILE_DISABLE=1
tar -cf "$out/$system-$machine-$chip.tar" "${contents[@]}" -C "$work/extra" .
tar -tf "$out/$system-$machine-$chip.tar" | awk -F / '{ print $1 "/" $2 }' | sort -u

if [ "$system" != linux ]; then
  exit 0
fi

# The deb package.
root=$work/deb
DESTDIR=$root cmake --install "$build" --prefix /usr
doc=$root/usr/share/doc/$package
mv "$root/usr/share/doc/$name" "$doc"
cp -R "$work/extra/licenses" "$doc/licenses"
cp "$work/extra/licenses/README.txt" "$doc/copyright"
find "$root" -type d -exec chmod 755 {} +

# The packages of the libraries that the binaries link against, and of those
# that JUCE opens at run time for its windows (juce_XSymbols_linux.h), which
# dpkg-shlibdeps cannot see.
mkdir -p "$work/shlibs/debian"
printf 'Source: %s\n\nPackage: %s\nArchitecture: amd64\n' "$package" "$package" > "$work/shlibs/debian/control"
binaries=()
while IFS= read -r -d '' binary; do
  binaries+=("-e$binary")
done < <(find "$root" -type f \( -name '*.so' -o -path "$root/usr/bin/*" \) -print0)
shlibs=$(cd "$work/shlibs" && dpkg-shlibdeps -O "${binaries[@]}")
depends="${shlibs#shlibs:Depends=}, libx11-6, libxext6, libxcursor1, libxinerama1, libxrender1, libxrandr2, libxi6"

file=${package}_${version}_amd64.deb
mkdir "$root/DEBIAN"
{
  echo "Package: $package"
  echo "Version: $version"
  echo "Architecture: amd64"
  if [ "$arch" = avx2 ]; then
    echo "Architecture-Variant: amd64v3"
    file=${package}_${version}_amd64v3.deb
  fi
  echo "Maintainer: Yuma Kakei <yumasansansan@gmail.com>"
  echo "Installed-Size: $(du -sk "$root" | cut -f 1)"
  echo "Depends: $depends"
  echo "Section: sound"
  echo "Priority: optional"
  echo "Homepage: https://github.com/yumasansansan/ADLplug-Next"
  echo "Description: $summary"
  echo " $name plays MIDI on emulations of $chips,"
  echo " as VST3 and LV2 plugins and as a standalone program."
  echo " ."
  echo " This is version $display."
  if [ "$arch" = avx2 ]; then
    echo " It needs a processor with AVX2 (x86-64-v3)."
  fi
} > "$root/DEBIAN/control"

dpkg-deb --root-owner-group -Zxz --build "$root" "$out/$file"
dpkg-deb --info "$out/$file"
