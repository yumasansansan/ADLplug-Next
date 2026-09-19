#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/package-rpm.sh <preset> <baseline|avx2> <directory>
#
# Makes the rpm package of a Release build of ci/build.sh, for RHEL and
# AlmaLinux 10 or later and openSUSE, in the AlmaLinux container of CI
# (.github/workflows/ci.yml): what the build installs under /usr, with the
# texts of the banks and the licenses (ci/licenses.py); rpmbuild compiles
# nothing and sets no build flags. Those systems keep their libraries in lib64,
# and so do their LV2 hosts look there: the build has to be configured with
# -DADLplug_INSTALL_LV2DIR=lib64/lv2. The AVX2 build is the x86_64_v3 package
# of the same name.
set -euo pipefail

preset=$1
arch=$2
out=$3

build=build/$preset
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
package=$(tr '[:upper:]' '[:lower:]' <<< "$name")

note=
case $arch in
  baseline) target=x86_64 ;;
  avx2) target=x86_64_v3 note=" It needs a processor with AVX2 (x86-64-v3)." ;;
  *) echo "error: unknown instruction set '$arch'" >&2; exit 2 ;;
esac

vst3dir=$(sed -n 's/^ADLplug_INSTALL_VST3DIR:STRING=//p' "$build/CMakeCache.txt")
lv2dir=$(sed -n 's/^ADLplug_INSTALL_LV2DIR:STRING=//p' "$build/CMakeCache.txt")
if [ "$lv2dir" != lib64/lv2 ]; then
  echo "error: $build installs the LV2 plugin into $lv2dir, not lib64/lv2" >&2
  exit 1
fi

mkdir -p "$out"
out=$(cd "$out" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

root=$work/root
DESTDIR=$root cmake --install "$build" --prefix /usr
doc=$root/usr/share/doc/$package
mv "$root/usr/share/doc/$name" "$doc"
python3 ci/licenses.py "$build" "$doc/licenses"

cat > "$work/package.spec" << EOF
# The files as the build made them: no debugging information to split off, and
# nothing to strip or compress.
%global debug_package %{nil}
%global __os_install_post %{nil}
%global _build_id_links none
# Nothing is compiled here. The distribution's compiler and linker flags, which
# redhat-rpm-config would export to the scriptlets, are left out, and the
# installation checks that none are set.
%undefine _auto_set_build_flags
%global optflags %{nil}
%global build_ldflags %{nil}
# The plugins are no libraries for other packages to require.
%global __provides_exclude_from ^/usr/($vst3dir|$lv2dir)/

Name: $package
Version: $version
Release: 1
Summary: $summary
License: GPL-3.0-only AND AGPL-3.0-only
URL: https://github.com/yumasansansan/ADLplug-Next
Packager: Yuma Kakei <yumasansansan@gmail.com>
# The libraries that JUCE opens at run time for its windows
# (juce_XSymbols_linux.h), which rpm cannot find in the binaries.
Requires: libX11.so.6()(64bit)
Requires: libXext.so.6()(64bit)
Requires: libXcursor.so.1()(64bit)
Requires: libXinerama.so.1()(64bit)
Requires: libXrender.so.1()(64bit)
Requires: libXrandr.so.2()(64bit)
Requires: libXi.so.6()(64bit)

%description
$name plays MIDI on emulations of $chips,
as VST3 and LV2 plugins and as a standalone program.

This is version $display.$note

%install
for variable in CC CXX CFLAGS CXXFLAGS FFLAGS FCFLAGS VALAFLAGS RUSTFLAGS LDFLAGS RPM_OPT_FLAGS RPM_LD_FLAGS; do
  if [ -n "\$(printenv "\$variable")" ]; then
    echo "error: rpmbuild has set \$variable" >&2
    exit 1
  fi
done
cp -a "$root/." "%{buildroot}/"

%files
/usr/$vst3dir/$name.vst3
/usr/$lv2dir/$name.lv2
/usr/bin/$name
/usr/share/applications/$name.desktop
/usr/share/icons/hicolor/32x32/apps/$name.png
/usr/share/icons/hicolor/96x96/apps/$name.png
/usr/share/pixmaps/$name.png
%doc /usr/share/doc/$package
EOF

rpmbuild -bb --target "$target" \
  --define "_topdir $work/rpmbuild" \
  --define "_rpmdir $out" \
  --define "_rpmfilename %%{NAME}-%%{VERSION}-%%{RELEASE}.%%{ARCH}.rpm" \
  "$work/package.spec"

for rpm in "$out"/*.rpm; do
  rpm -qip "$rpm"
  echo "== requires"
  rpm -qpR "$rpm"
done
