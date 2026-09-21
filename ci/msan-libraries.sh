#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   ci/msan-libraries.sh [<prefix>]
#
# Builds the two libraries a memory-sanitizer build needs and no system ships,
# and prints the prefix they are in, for ADLplug_MSAN_LIBRARIES.
#
# The sanitizer sees only the code it has instrumented: memory written by a
# library it has not seen is memory it believes was never written, so every read
# of it is reported. That makes the C++ standard library part of what has to be
# instrumented. No distribution ships one that is, and libstdc++ cannot be built
# with the sanitizer at all, so this builds libc++ and libc++abi from LLVM's own
# sources with it.
#
#   - libunwind is not built, and libc++abi is told to use the system unwinder.
#     What walks the stack for a report must not be instrumented: otherwise the
#     walk is itself instrumented into the report, and a real finding comes out as
#     "stack-overflow ... nested bug in the same thread, aborting" with nothing
#     named. Measured, both ways round.
#   - libFuzzer comes next, from the same sources. The one that comes with the
#     compiler is compiled against libstdc++, so a program that links libc++
#     instead is left with every std::string and std::ifstream of libFuzzer's own
#     code undefined. This builds it the way LLVM's own build.sh does, against the
#     instrumented library and with the sanitizer, as a fuzzer of such a build has
#     to be.
#
# The sources must be the compiler's own, and no version is written down here to
# say which: this asks the compiler what it is, and asks wherever that compiler
# came from for its sources. Where that is, is this project's one rule for LLVM and
# ci/setup.sh is where it is written: Ubuntu takes the packages of apt.llvm.org and
# every other system the archives of GitHub Releases, and that script says which it
# did in ADLplug_LLVM_FROM. Both ways are exact.
#
#   - apt, which is where the Linux builds get LLVM (ci/setup.sh). What
#     apt.llvm.org offers for a release is a snapshot of its branch, rebuilt
#     often, so two builds that both say 23.1.2 are not the same sources and no
#     version number tells them apart. The source package apt names for the very
#     binaries installed is what is unpacked, and its version -- which names the
#     build, not only the release -- is what the prefix records. ci/setup.sh asks
#     apt.llvm.org for sources as well as binaries, which is what makes this
#     possible.
#   - a release of LLVM proper, which is where the Windows and macOS builds get it
#     (the archives of GitHub Releases, each pinned by its digest in ci/setup.sh).
#     Such a build reports a plain version and no snapshot, and llvmorg-<version>
#     is a tag, which does not move: the tag is as exact as the source package.
#     Clang has no memory sanitizer for either of those systems, so this way round
#     is for a Linux without apt.llvm.org rather than for them.
#
# A snapshot on a system whose packages apt does not keep is the one case with
# nothing to ask, and ADLplug_MSAN_SOURCES then names an unpacked llvm-project to
# build from instead.
#
# Which clang is another question, since a runner has several of them and the one
# first on PATH is the one every part of this build uses (ci/setup.sh puts LLVM's
# there for the steps that follow it). Four things see to it that these libraries
# cannot be another compiler's: the path of the clang that answered is printed;
# ADLplug_LLVM_MAJOR, which ci/setup.sh sets to the version the build is of, is
# compared with what that clang reports; the source package's version is compared
# with the version of the installed clang; and the prefix records the version and
# the sources, which the configuration compares with its own compiler and writes
# beside the rest of the toolchain (CMakeLists.txt, ci/build.sh).
#
# Nothing of a memory-sanitizer build is shipped or installed: it exists to be run
# by the fuzz targets of one CI job. That is why the standard library this project
# otherwise links dynamically is linked statically there; CMakeLists.txt says why
# that is not about size either.
#
# Building again over a prefix that already holds the libraries of the compiler in
# use does nothing, so a job may call this unconditionally.
set -euo pipefail

prefix=${1:-$HOME/libcxx-msan}
unpacked=${ADLplug_MSAN_UNPACKED:-$HOME/llvm-sources}
build=${ADLplug_MSAN_BUILD:-$HOME/libcxx-msan-build}

reported=$(clang --version | head -1)
number=$(printf '%s\n' "$reported" | sed -nE 's/.*clang version ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p')
if [ -z "$number" ]; then
  echo "error: clang did not say which version it is: $reported" >&2
  exit 1
fi
major=${number%%.*}
if [ -n "${ADLplug_LLVM_MAJOR:-}" ] && [ "$major" != "$ADLplug_LLVM_MAJOR" ]; then
  echo "error: the clang first on PATH is $number ($(command -v clang)), and this build is of" >&2
  echo "       LLVM $ADLplug_LLVM_MAJOR (ADLplug_LLVM_MAJOR). The standard library of the memory" >&2
  echo "       sanitizer has to be the one the compiler in use was built from." >&2
  exit 1
fi
# The metadata of a snapshot build, which a release proper does not have.
snapshot=$(printf '%s\n' "$reported" | sed -nE 's/.*(\+[0-9]{14}\+[0-9a-f]{12,40}).*/\1/p')
echo "== the compiler in use: $reported"
echo "   $(command -v clang)"

package=llvm-toolchain-$major
tree=
from=${ADLplug_LLVM_FROM:-}
if [ -n "${ADLplug_MSAN_SOURCES:-}" ]; then
  from=named
fi
case $from in
  named)
    tree=$ADLplug_MSAN_SOURCES
    identity=$(basename "$tree")
    echo "   the sources: $tree (ADLplug_MSAN_SOURCES)"
    ;;
  apt)
    identity=$(apt-cache showsrc "$package" 2>/dev/null | sed -nE 's/^Version: (.*)$/\1/p' | head -1)
    if [ -z "$identity" ]; then
      echo "error: apt has no sources for $package. ci/setup.sh asks apt.llvm.org for them with a" >&2
      echo "       deb-src line beside the deb one; without it there is no way to have the sources" >&2
      echo "       of the snapshot that is installed." >&2
      exit 1
    fi
    installed=$(dpkg-query -W -f='${Version}' "clang-$major" 2>/dev/null || true)
    if [ -n "$installed" ] && [ "${installed#*:}" != "${identity#*:}" ]; then
      echo "error: apt would give the sources of $package $identity, and the clang installed is" >&2
      echo "       $installed. A snapshot is rebuilt often, so those are not the same sources:" >&2
      echo "       update apt and install the compiler again, in one go." >&2
      exit 1
    fi
    echo "   the sources: $package $identity, which apt has for the clang it installed"
    ;;
  release)
    if [ -n "$snapshot" ]; then
      echo "error: LLVM came from a release (ADLplug_LLVM_FROM), and this clang is a snapshot" >&2
      echo "       ($number$snapshot), whose sources no tag names. Unpack the llvm-project it was" >&2
      echo "       built from and name it in ADLplug_MSAN_SOURCES." >&2
      exit 1
    fi
    identity=llvmorg-$number
    echo "   the sources: $identity, the tag of the release this compiler is"
    ;;
  *)
    echo "error: ADLplug_LLVM_FROM does not say where LLVM came from, and the sources have to" >&2
    echo "       come from the same place (ci/setup.sh writes it: apt for Ubuntu, release for" >&2
    echo "       every other system). Set it, or name an unpacked llvm-project of this compiler's" >&2
    echo "       own version in ADLplug_MSAN_SOURCES." >&2
    exit 1
    ;;
esac

have=$(cat "$prefix/llvm.txt" 2>/dev/null || true)
if [ -f "$prefix/lib/libc++.a" ] && [ -f "$prefix/lib/libc++abi.a" ] &&
   [ -f "$prefix/lib/libFuzzer.a" ] && [ -d "$prefix/include/c++/v1" ] &&
   [ "$have" = "$number $identity" ]; then
  echo "== the libraries of the memory sanitizer are already in $prefix, of these sources"
  echo "$prefix"
  exit 0
fi
# Anything left of another compiler's build is of no use and would be linked in
# its place, so it goes.
rm -rf "$prefix/lib" "$prefix/include" "$prefix/llvm.txt" "$build"

case $from in
  apt)
    if ! command -v dpkg-source > /dev/null; then
      echo "error: unpacking a source package needs dpkg-dev, which is not installed." >&2
      exit 1
    fi
    echo "== unpack the sources apt has for this compiler"
    rm -rf "$unpacked"
    mkdir -p "$unpacked"
    (cd "$unpacked" && apt-get source "$package=$identity" > /dev/null)
    tree=$(find "$unpacked" -maxdepth 1 -mindepth 1 -type d -name "$package-*" | head -1)
    ;;
  release)
    echo "== check out the sources of this release"
    mkdir -p "$unpacked"
    if [ ! -d "$unpacked/.git" ]; then
      git -C "$unpacked" init --quiet
      git -C "$unpacked" remote add origin https://github.com/llvm/llvm-project.git
    fi
    # Only what the build reads: runtimes/ drives it, libcxx/ and libcxxabi/ are
    # what is built, cmake/ and llvm/cmake/ hold the modules both read,
    # third-party/ is where the build looks for its unit test library, libc/ holds
    # the headers libc++ shares with LLVM's C library (src/charconv.cpp reads
    # shared/fp_bits.h from there), and compiler-rt/lib/fuzzer is libFuzzer. A
    # checkout missing one of them stops CMake or the compiler, so each is named
    # rather than left to chance. A source package holds all of it already.
    git -C "$unpacked" sparse-checkout set \
      cmake runtimes libcxx libcxxabi third-party llvm/cmake libc/shared libc/include libc/hdr \
      libc/src/__support compiler-rt/lib/fuzzer
    git -C "$unpacked" fetch --depth 1 --filter=blob:none --quiet origin "$identity"
    git -C "$unpacked" checkout --quiet --detach FETCH_HEAD
    tree=$unpacked
    ;;
esac
if [ -z "$tree" ] || [ ! -d "$tree/runtimes" ] || [ ! -d "$tree/compiler-rt/lib/fuzzer" ]; then
  echo "error: the sources in '$tree' hold no runtimes/ and compiler-rt/lib/fuzzer/ to build" >&2
  exit 1
fi
echo "   $tree, $(du -sh "$tree" | cut -f1)"

echo "== configure the standard library"
cmake -G Ninja -S "$tree/runtimes" -B "$build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
  -DLLVM_USE_SANITIZER=MemoryWithOrigins \
  -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
  -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
  -DLLVM_INCLUDE_TESTS=OFF \
  -DLIBCXX_INCLUDE_TESTS=OFF \
  -DLIBCXXABI_INCLUDE_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX="$prefix" > /dev/null

echo "== build the standard library"
start=$(date +%s)
ninja -C "$build" cxx cxxabi
ninja -C "$build" install-cxx install-cxxabi > /dev/null
echo "   $(( $(date +%s) - start )) seconds"

echo "== build libFuzzer against it"
start=$(date +%s)
objects=$(mktemp -d)
trap 'rm -rf "$objects"' EXIT
for source in "$tree"/compiler-rt/lib/fuzzer/*.cpp; do
  clang++ -c -g -O2 -std=c++17 -fno-omit-frame-pointer \
    -fsanitize=memory -fsanitize-memory-track-origins=2 \
    -nostdinc++ -isystem "$prefix/include/c++/v1" \
    "$source" -o "$objects/$(basename "${source%.cpp}").o" &
done
wait
llvm-ar r "$prefix/lib/libFuzzer.a" "$objects"/*.o
echo "   $(( $(date +%s) - start )) seconds, $(ls "$objects" | wc -l) objects"

for part in lib/libc++.a lib/libc++abi.a lib/libFuzzer.a include/c++/v1/version; do
  if [ ! -e "$prefix/$part" ]; then
    echo "error: the build left no $part in $prefix" >&2
    exit 1
  fi
done
printf '%s %s\n' "$number" "$identity" > "$prefix/llvm.txt"
echo "== the libraries of the memory sanitizer are in $prefix"
ls -l "$prefix/lib/libc++.a" "$prefix/lib/libc++abi.a" "$prefix/lib/libFuzzer.a" | sed 's/^/   /'
echo "$prefix"
