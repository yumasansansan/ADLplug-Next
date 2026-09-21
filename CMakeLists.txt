# SPDX-FileCopyrightText: 2018-2021 Jean Pierre Cimalando
# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: BSL-1.0 AND GPL-3.0-or-later
#
# This file comes from ADLplug and was modified for ADLplug-Next. ADLplug gave
# it no notice of its own; it was under ADLplug's license, the Boost Software
# License 1.0 (LICENSES/BSL-1.0.txt). The SPDX lines name the copyright holders
# and licenses in the machine-readable form of the REUSE specification:
# ADLplug's part is under the Boost Software License 1.0, and ADLplug-Next's
# changes are under the GNU General Public License, version 3 or any later
# version (LICENSES/GPL-3.0-or-later.txt).

cmake_minimum_required(VERSION 3.29)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# macOS builds are for Apple Silicon and macOS 26 or later. Both have to be set
# before project(). Without a deployment target, the plugins would require the
# macOS version of whichever SDK built them.
set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Architectures to build for on macOS")

# The version comes from the git history (cmake/Version.cmake).
include(Version)
project(ADLplug VERSION "${ADLplug_VERSION}" LANGUAGES C CXX)
set(PROJECT_VENDOR "DyTect")
set(PROJECT_CONTACT "Yuma Kakei <yumasansansan@gmail.com>")
set(PROJECT_HOMEPAGE "https://github.com/yumasansansan/ADLplug-Next")
# The version and the version to show, for the scripts that pack the builds
# (ci/package.sh).
file(CONFIGURE OUTPUT "version.txt" CONTENT "${ADLplug_VERSION}\n${ADLplug_VERSION_DISPLAY}\n")

# --------------------------------------------------------------------------
# The presets are how this project is configured
#
# CMakePresets.json chooses the compilers, the linker, the generator and the
# settings that ADLplug-Next is built and tested with. A configuration put
# together by hand differs from them without saying so, which is how a build ends
# up with flags the project never chose; the checks below would catch a missing
# compiler or linker, but not the rest. Every preset inherits base, which names
# the preset here, and the instrumented build of profile-guided optimisation
# carries the name into the build it configures itself (cmake/PGO.cmake).
set(ADLplug_PRESET "" CACHE STRING
  "The preset this build was configured with; CMakePresets.json sets it")
if(NOT ADLplug_PRESET)
  message(FATAL_ERROR
    "Configure through a preset: cmake --preset adl-release, and cmake --list-presets names them "
    "all. The presets choose the compilers, the linker, the generator and the settings this "
    "project is built and tested with, which a configuration by hand differs from without saying "
    "so. A build that has to differ can inherit a preset of its own in CMakeUserPresets.json, or "
    "say -DADLplug_PRESET=<a name for it> and own the settings it is then left with.")
endif()
message(STATUS "Preset: ${ADLplug_PRESET}")

# The warning policy, the language standards and LTO below are written for
# Clang; GCC and MSVC are not supported compilers.
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR
    "ADLplug is built with Clang, not ${CMAKE_CXX_COMPILER_ID}. Configure with "
    "CMAKE_C_COMPILER=clang and CMAKE_CXX_COMPILER=clang++, as the presets do.")
endif()

# LLD links everything, on every system: CMake gives CMAKE_LINKER_TYPE to
# every target and to try_compile(). The presets give -fuse-ld=lld, through
# LDFLAGS, to the helpers that JUCE builds as projects of their own too.
if(NOT CMAKE_LINKER_TYPE STREQUAL "LLD")
  message(FATAL_ERROR
    "ADLplug is linked with LLD, but CMAKE_LINKER_TYPE is '${CMAKE_LINKER_TYPE}'. "
    "Configure with -DCMAKE_LINKER_TYPE=LLD, as the presets do.")
endif()

# The toolchain has to be LLVM's, of one version (cmake/LLVMToolchain.cmake).
include(LLVMToolchain)

include(GNUInstallDirs)
include(SystemOption)

# --------------------------------------------------------------------------
# Options

system_option(ADLplug_VST3 "Build VST3" ON)
system_option(ADLplug_LV2 "Build LV2" ON)
system_option(ADLplug_AU "Build AU" OFF "Darwin" ON)
system_option(ADLplug_AAX "Build AAX" OFF "Windows" ON "Darwin" ON)
system_option(ADLplug_Standalone "Build Standalone" ON)
system_option(ADLplug_ASIO "Enable ASIO support (Windows, GPLv3)" OFF "Windows" ON)

option(ADLplug_ASSERTIONS "Enable assertions" OFF)
option(ADLplug_WERROR "Treat warnings in ADLplug's own sources as errors" OFF)
option(ADLplug_BUILD_TOOLS "Build developer tools (tools/render: offline VST3 renderer)" OFF)
option(ADLplug_BUILD_TESTS "Build the tests and register them with CTest (render tests need ADLplug_BUILD_TOOLS)" OFF)
option(ADLplug_BUILD_FUZZERS "Build the fuzz targets with libFuzzer (fuzz/; meant for the address and undefined sanitizers)" OFF)
# libFuzzer runs on Ubuntu. On Windows it cannot be linked at all: the
# libFuzzer that LLVM builds for Windows uses the static C runtime, and
# ADLplug-Next the dynamic one, and the two cannot go into one program.
if(ADLplug_BUILD_FUZZERS AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
  message(FATAL_ERROR
    "ADLplug_BUILD_FUZZERS is not for Windows: LLVM's libFuzzer for Windows is linked "
    "with the static C runtime (/MT) and ADLplug-Next with the dynamic one (/MD). "
    "The fuzz targets still run on their inputs as tests there (ADLplug_BUILD_TESTS).")
endif()
option(ADLplug_FUZZ_FULL_COVERAGE
  "Give the libraries every coverage feature libFuzzer steers by, the stack depth included; the plugin of such a build cannot be loaded, so build the fuzz targets alone (ci/build.sh --fuzz-only)" OFF)
if(ADLplug_FUZZ_FULL_COVERAGE AND NOT ADLplug_BUILD_FUZZERS)
  message(FATAL_ERROR
    "ADLplug_FUZZ_FULL_COVERAGE is about the coverage of the fuzz targets, and this build "
    "has none: it needs ADLplug_BUILD_FUZZERS.")
endif()
option(ADLplug_COVERAGE
  "Measure which of ADLplug-Next's own code the tests and the fuzz corpus reach (ci/coverage.sh)" OFF)
option(ADLplug_GREYZONE_BANKS "Include the instrument banks of the grey zone, for which no license or permission of their authors is known" OFF)

set(ADLplug_CHIP "OPL3" CACHE STRING "Kind of synthesizer chip (OPL3, OPN2)")
set(ADLplug_ARCH "baseline" CACHE STRING "Instruction set of x86-64 builds (baseline, avx2)")
set(ADLplug_SANITIZERS "" CACHE STRING "Sanitizers to build with, comma-separated (address, undefined, vptr, thread, memory, realtime)")
set(ADLplug_MSAN_LIBRARIES "" CACHE PATH "Prefix of the libraries a memory-sanitizer build needs and no system ships: a C++ standard library built with that sanitizer, and a libFuzzer built against it (ci/msan-libraries.sh writes them)")

# Applied per-target rather than globally: these variables are inherited by
# add_subdirectory() dependencies, and libADLMIDI/libOPNMIDI build their own
# sources as C90 + GNU extensions.
function(adlplug_set_language_standard target)
  set_target_properties("${target}" PROPERTIES
    C_STANDARD 23 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endfunction()

# No file of this project is a C++ module, and none imports one. Left alone,
# CMake scans every C++23 source for modules and compiles it with a mapping file
# it generates from what the scan found: work for a build to do and nothing for
# it to find. The mapping file is also named in the compile commands, so a reader
# of the code (ci/tidy.sh) could not parse a single file until the build had
# generated one for it. JUCE turns the scanning off for the targets it makes; this
# turns it off for the rest.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_C_VISIBILITY_PRESET "hidden")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if(ADLplug_ASSERTIONS)
  add_definitions("-UNDEBUG")
endif()

# --------------------------------------------------------------------------
# Link-time optimisation
#
# Release builds are made with link-time optimisation, for every target: it is
# set before any dependency is added, so JUCE, libADLMIDI and libOPNMIDI get
# it too, and the synthesis code can be inlined across library boundaries.
# With Clang, CMake's IPO support means ThinLTO (-flto=thin). There is no way
# around it: CMake stops if the compiler cannot do it, and so does the build if
# the linker cannot.

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# With ThinLTO, LLD runs the optimisation across modules and the code generation
# when it links, at levels of its own, which are 2 unless it is told otherwise.
# Clang passes the -O3 of the link command on to LLD for ELF (-plugin-opt=O3),
# but not for Windows or Apple systems. The levels are given to LLD itself, on
# every system.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_link_options("$<$<CONFIG:Release>:LINKER:/opt:lldlto=3,/opt:lldltocgo=3>")
else()
  add_link_options("$<$<CONFIG:Release>:LINKER:--lto-O3,--lto-CGO3>")
endif()

# --------------------------------------------------------------------------
# Instruction set
#
# x86-64 builds come in two kinds: baseline (x86-64, which every such processor
# runs) and avx2 (x86-64-v3: AVX2, BMI2, FMA and the rest of that level). The
# baseline is passed explicitly too, so a compiler configured with another
# default still builds it. Like LTO, this is set before any dependency is
# added, so JUCE and the emulator cores get it as well; the cores are where a
# wider instruction set pays off. ThinLTO takes the choice from the bitcode,
# so the link needs no flag of its own.

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
  if(ADLplug_ARCH STREQUAL "baseline")
    add_compile_options(-march=x86-64)
  elseif(ADLplug_ARCH STREQUAL "avx2")
    add_compile_options(-march=x86-64-v3)
  else()
    message(FATAL_ERROR "unsupported ADLplug_ARCH ${ADLplug_ARCH} (baseline, avx2)")
  endif()
elseif(NOT ADLplug_ARCH STREQUAL "baseline")
  message(FATAL_ERROR "ADLplug_ARCH=${ADLplug_ARCH} applies to x86-64 builds only")
endif()

# --------------------------------------------------------------------------
# Optimisation flags
#
# Release builds are optimised as far as the meaning of the code allows. Besides
# -O3 and LTO at level 3, every target, the dependencies included, is compiled
# with these:
# - -fno-math-errno: the math functions are not expected to set errno, so that
#   the compiler can put instructions in their place and vectorise loops that
#   call them. What they compute stays IEEE's.
# - -ffinite-loops: loops whose condition is not a constant are assumed to end.
#   Clang assumes it for C11 and C++11 code anyway; this extends it to the C90
#   code of the libraries.
# - -fomit-frame-pointer, except on Apple's systems: the frame pointer register
#   is left to the code. Apple's arm64 ABI wants it to address a valid frame
#   record at all times; given there, auval aborted on an ausdk::AUException
#   that the plugin's own dispatch code catches otherwise. Clang already leaves
#   frame records out of leaf functions there.
# - -fvisibility-inlines-hidden-static-local-var (Linux): the static variables
#   of hidden inline member functions are hidden too, and reached without the
#   GOT. It is the default on Apple's systems.
# - -fno-plt (Linux): calls into shared libraries go through the GOT, without
#   the PLT's stubs.
# - -mtls-dialect=gnu2 (x86-64 Linux): thread-local variables of a shared
#   object are reached through TLS descriptors, without a call to
#   __tls_get_addr. The link gives it to the LTO backend as well.
# LLD is given optimisations of its own on Linux: -O3 (branches to branches go
# straight to their targets, and strings share their tails), --icf=safe
# (identical functions whose addresses are not taken are folded) and
# --Bsymbolic-functions (a shared object calls its own functions directly); and
# --icf=safe on Apple's systems. lld-link folds and removes code by default.
# Nothing here trades the precision of floating-point results (no -ffast-math
# or its parts; contraction stays as the compiler has it), protections, or the
# defaults of Windows builds that keep MSVC's semantics (-fwrapv, no strict
# aliasing), on which Windows code may rely.

add_compile_options(
  "$<$<CONFIG:Release>:-fno-math-errno>"
  "$<$<CONFIG:Release>:-ffinite-loops>")
if(APPLE)
  add_link_options("$<$<CONFIG:Release>:LINKER:--icf=safe>")
else()
  add_compile_options("$<$<CONFIG:Release>:-fomit-frame-pointer>")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_compile_options(
    "$<$<CONFIG:Release>:-fno-plt>"
    "$<$<CONFIG:Release>:$<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden-static-local-var>>")
  add_link_options("$<$<CONFIG:Release>:LINKER:-O3,--icf=safe,--Bsymbolic-functions>")
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    add_compile_options("$<$<CONFIG:Release>:-mtls-dialect=gnu2>")
    add_link_options("$<$<CONFIG:Release>:-mtls-dialect=gnu2>")
  endif()
endif()

# --------------------------------------------------------------------------
# Sanitizers
#
# ADLplug_SANITIZERS names the sanitizers of a build, comma-separated (address,
# undefined, vptr, thread, memory, realtime), and gives them to every target: ADLplug-Next's own
# code, JUCE, libADLMIDI and libOPNMIDI. The emulator cores are part of what
# the plugins ship, so a mistake of memory or an undefined operation in them is
# the plugin's. Undefined behaviour stops the program
# (-fno-sanitize-recover=all) instead of being reported and passed over, and
# the frame pointer stays, so that the reports name the callers.
#
# vptr is the check that an object really is of the class it is used as, when a
# downcast or a call of a member function takes it to be one; undefined leaves
# it out. Clang has no such check for the MSVC ABI and refuses the option
# there, so a Windows build leaves vptr out and says so.
#
# thread finds what the others cannot see at all: two threads that reach the
# same memory with nothing to order the one against the other, locks taken in
# an order that could meet another thread's order and stop both, and a thread
# that is left running when the program ends. A host runs a plugin on several
# threads at once -- the audio thread it asks for blocks on, the message thread
# the editor lives on, and the plugin's own worker -- so these are mistakes
# ADLplug-Next can make wherever those threads share anything. It cannot be
# built together with address: each of the two keeps a shadow of all of memory
# in its own way, and Clang refuses the pair. The *-tsan presets name it with
# undefined, and it has a build of its own in CI, beside the address one, so that
# neither kind of mistake is left unlooked-for. It goes without vptr, whose check
# races with the thread sanitizer's runtime inside LLVM itself (below), and the
# address build has that one. Clang has no thread sanitizer at all for Windows,
# and the configuration says so rather than leaving the compiler to refuse the
# option later.
#
# Sanitizers are not for Release builds, whose optimisations, link-time
# optimisation and profile would stand in the way: the *-sanitize and *-tsan
# presets configure RelWithDebInfo.
#
# The tests run with the environment below, given to them by the directories
# that register them (tests/, fuzz/) once all their tests exist. UBSan names the
# callers of what it finds.
#
# Leaks: LeakSanitizer comes with ASan and looks for them when a program ends,
# and a leak fails the test like any other finding. It is asked for on Linux,
# where it is at home. Nowhere else needs telling: it is off by default where it
# does not run, and asking for it there only makes ASan stop with "detect_leaks
# is not supported on this platform". The programs the build itself runs do
# have to be told (CMakePresets.json), since on Linux they would otherwise be
# checked as well, and their leaks say nothing about the plugins. LeakSanitizer
# reports only memory that nothing points to any more, so objects that live
# until the end, JUCE's among them, never show up. It also says nothing when it
# finds nothing, which cannot be told from a check that never ran: the test
# sanitizer.leak_detection leaks on purpose and passes only when that leak is
# reported, so that the silence of the other tests means something.
#
# Races: ThreadSanitizer reports one and carries on by default, and tells the
# caller only in the exit status when the program ends. The tests have it stop
# at the first report instead (halt_on_error=1), as -fno-sanitize-recover=all
# has undefined behaviour stop the program: a fuzz run then keeps the input that
# found it, and nothing else happens in a program whose memory is already in
# doubt. A report of locks taken in two orders names the other order's place as
# well (second_deadlock_stack=1), which is what makes it possible to act on.
if(ADLplug_SANITIZERS)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR
      "ADLplug_SANITIZERS is not for Release builds, which are optimised with LTO and a "
      "profile. Configure RelWithDebInfo, as the *-sanitize presets do.")
  endif()
  string(REPLACE "," ";" ADLplug_SANITIZER_LIST "${ADLplug_SANITIZERS}")
  foreach(sanitizer IN LISTS ADLplug_SANITIZER_LIST)
    if(NOT sanitizer MATCHES "^(address|undefined|vptr|thread|memory|realtime)$")
      message(FATAL_ERROR
        "ADLplug_SANITIZERS names '${sanitizer}', which is not one of address, undefined, vptr, "
        "thread, memory, realtime.")
    endif()
  endforeach()
  if("address" IN_LIST ADLplug_SANITIZER_LIST AND "thread" IN_LIST ADLplug_SANITIZER_LIST)
    message(FATAL_ERROR
      "ADLplug_SANITIZERS names both address and thread, which cannot be built together.")
  endif()
  if("thread" IN_LIST ADLplug_SANITIZER_LIST AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(FATAL_ERROR
      "ADLplug_SANITIZERS names thread, which Clang has no thread sanitizer for on Windows: it "
      "refuses -fsanitize=thread there. The *-tsan presets are for Linux and macOS.")
  endif()
  # LLVM's own runtime races with itself when the vptr check runs on more than
  # one thread: the check asks IsAccessibleMemoryRange() whether a vtable
  # pointer can be read, that makes a pipe, and the write of the two descriptors
  # is what ThreadSanitizer then reports -- a report with no frame of the
  # program in it at all. A program of thirty lines that does nothing but call a
  # virtual function from two threads is enough to bring it out (Clang 23.1.2),
  # and there is nothing in a build here to fix: it has been known upstream since
  # 2019 (google/sanitizers issue 1106). A thread build leaves vptr out; the
  # address build keeps it, and what it finds has nothing to do with threads.
  if("thread" IN_LIST ADLplug_SANITIZER_LIST AND "vptr" IN_LIST ADLplug_SANITIZER_LIST)
    list(REMOVE_ITEM ADLplug_SANITIZER_LIST "vptr")
    message(STATUS
      "Sanitizers: vptr is left out, since its check and the thread sanitizer's runtime race "
      "with one another inside LLVM")
  endif()
  if("vptr" IN_LIST ADLplug_SANITIZER_LIST AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    list(REMOVE_ITEM ADLplug_SANITIZER_LIST "vptr")
    message(STATUS "Sanitizers: vptr is left out, since Clang has no vptr check for the MSVC ABI")
    if(NOT ADLplug_SANITIZER_LIST)
      message(FATAL_ERROR "ADLplug_SANITIZERS names no sanitizer that this build can have besides vptr.")
    endif()
  endif()
  # realtime asks a different question from all of them: not whether the code is
  # wrong, but whether it may be run where a moment's wait is a gap in the sound.
  # A function marked [[clang::nonblocking]] -- processBlock and what it calls --
  # may not take a lock, allocate, free, read a file or throw, and the sanitizer
  # reports the first such call it intercepts. This project is written that way
  # already: a lock-free queue carries what the editor asks for, a worker thread
  # does the measuring, and the audio thread takes the player's lock with
  # try_to_lock so that it never waits for it. Nothing checked that until now, and
  # one new allocation under processBlock would have gone unnoticed.
  #
  # It is on its own: Clang refuses -fsanitize=realtime with every other sanitizer,
  # undefined included, so the *-rtsan presets name it alone. The attribute costs
  # nothing in a build without it, which is why the code carries it always -- it
  # says what the contract is whether or not anything is checking.
  if("realtime" IN_LIST ADLplug_SANITIZER_LIST)
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      message(FATAL_ERROR
        "ADLplug_SANITIZERS names realtime, which Clang has no realtime sanitizer for on Windows. "
        "The *-rtsan presets are for Linux and macOS.")
    endif()
    list(LENGTH ADLplug_SANITIZER_LIST ADLplug_SANITIZER_COUNT)
    if(NOT ADLplug_SANITIZER_COUNT EQUAL 1)
      message(FATAL_ERROR
        "ADLplug_SANITIZERS names realtime together with something else (${ADLplug_SANITIZERS}). "
        "Clang refuses -fsanitize=realtime with every other sanitizer, undefined included: the "
        "*-rtsan presets name realtime and nothing besides.")
    endif()
  endif()

  # memory reads what the others cannot: a value that was never written. It sees
  # only the code it has instrumented, and memory written by code it has not seen
  # is memory it believes was never written, so every read of that is reported.
  # That makes the C++ standard library part of what has to be instrumented, and
  # no system ships one that is: ADLplug_MSAN_LIBRARIES names a prefix holding
  # libc++ built with the sanitizer, which ci/msan-libraries.sh writes, and the
  # build compiles and links against that in place of the system's library. The
  # same prefix holds a libFuzzer built against it, which the fuzz targets link in
  # place of the compiler's own (fuzz/CMakeLists.txt says why).
  #
  # Those two libraries are linked statically, which is the one place this project
  # links a standard library that way -- and the reason is not size. A shared
  # instrumented libc++ makes the sanitizer's own report walk the stack into
  # itself until the stack runs out, so that a real finding is answered with
  # "stack-overflow ... nested bug in the same thread, aborting" and never named
  # (measured, with the static library and with fast_unwind_on_fatal=1 as the two
  # ways round it). Nothing of such a build is shipped or installed, so what the
  # system's library gains later reaches nothing that leaves here.
  #
  # Origins are tracked to the fullest, so a report says where the value it read
  # was made and through which stores it travelled. Without that it names the read
  # alone, which is where the value arrived rather than where it came from.
  if("memory" IN_LIST ADLplug_SANITIZER_LIST)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
      message(FATAL_ERROR
        "ADLplug_SANITIZERS names memory, which Clang has a memory sanitizer for on Linux alone. "
        "The *-msan presets are for Linux.")
    endif()
    foreach(other IN ITEMS address thread)
      if("${other}" IN_LIST ADLplug_SANITIZER_LIST)
        message(FATAL_ERROR
          "ADLplug_SANITIZERS names both memory and ${other}, which cannot be built together: "
          "each keeps a shadow of all of memory in its own way.")
      endif()
    endforeach()
    if(NOT ADLplug_MSAN_LIBRARIES)
      message(FATAL_ERROR
        "ADLplug_SANITIZERS names memory, which needs a C++ standard library built with it: "
        "reads of what the system's library wrote would be reported as reads of what was never "
        "written. Give ADLplug_MSAN_LIBRARIES the prefix of one, which ci/msan-libraries.sh "
        "writes.")
    endif()
    foreach(part IN ITEMS lib/libc++.a lib/libc++abi.a lib/libFuzzer.a include/c++/v1/version
                          llvm.txt)
      if(NOT EXISTS "${ADLplug_MSAN_LIBRARIES}/${part}")
        message(FATAL_ERROR
          "ADLplug_MSAN_LIBRARIES is '${ADLplug_MSAN_LIBRARIES}', which holds no ${part}. "
          "ci/msan-libraries.sh writes a prefix that holds all of them.")
      endif()
    endforeach()
    # And they have to be this compiler's. A standard library of a different LLVM
    # than the compiler that reads its headers is a thing to find out here rather
    # than in whatever it makes of a template: llvm.txt says which compiler built
    # them and from which sources, both asked of that compiler.
    file(STRINGS "${ADLplug_MSAN_LIBRARIES}/llvm.txt" ADLplug_MSAN_BUILT_BY LIMIT_COUNT 1)
    string(REGEX MATCH "^[^ ]+" ADLplug_MSAN_LIBRARY_VERSION "${ADLplug_MSAN_BUILT_BY}")
    string(REGEX REPLACE "^[^ ]+ " "" ADLplug_MSAN_LIBRARY_SOURCES "${ADLplug_MSAN_BUILT_BY}")
    string(REGEX MATCH "^[0-9]+" ADLplug_MSAN_LIBRARY_MAJOR "${ADLplug_MSAN_LIBRARY_VERSION}")
    string(REGEX MATCH "^[0-9]+" ADLplug_MSAN_COMPILER_MAJOR "${CMAKE_CXX_COMPILER_VERSION}")
    if(NOT ADLplug_MSAN_LIBRARY_MAJOR STREQUAL ADLplug_MSAN_COMPILER_MAJOR)
      message(FATAL_ERROR
        "ADLplug_MSAN_LIBRARIES holds libraries built by Clang ${ADLplug_MSAN_LIBRARY_VERSION}, "
        "and this build's compiler is ${CMAKE_CXX_COMPILER_VERSION} (${CMAKE_CXX_COMPILER}). "
        "Run ci/msan-libraries.sh with the compiler this build uses first on PATH.")
    endif()
    # Written where every other part of the toolchain is written, so that the log
    # and the summary of a job name these two as they name the compiler and the
    # linker (ci/build.sh). The C++ library the check above found is the system's,
    # which a build with this sanitizer is the one kind that does not use it.
    message(STATUS
      "Sanitizers: the C++ standard library is libc++ ${ADLplug_MSAN_LIBRARY_VERSION} built with "
      "the sanitizer, from ${ADLplug_MSAN_LIBRARY_SOURCES}")
    file(APPEND "${ADLplug_LLVM_TOOLCHAIN_FILE}"
      "C++ library (sanitized)\t${ADLplug_MSAN_LIBRARY_VERSION}\tlibc++ ${ADLplug_MSAN_LIBRARIES}/lib/libc++.a (${ADLplug_MSAN_LIBRARY_SOURCES})\n"
      "libFuzzer (sanitized)\t${ADLplug_MSAN_LIBRARY_VERSION}\t${ADLplug_MSAN_LIBRARIES}/lib/libFuzzer.a\n")
    add_compile_options(-fsanitize-memory-track-origins=2
      "$<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>"
      "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-isystem ${ADLplug_MSAN_LIBRARIES}/include/c++/v1>")
    add_link_options(-nostdlib++)
    # The standard library goes at the very end of every link line, which is where
    # a static one has to be, and the two archives refer to one another, so the
    # linker is told to read them until nothing more is needed.
    string(APPEND CMAKE_CXX_STANDARD_LIBRARIES
      " -Wl,--start-group ${ADLplug_MSAN_LIBRARIES}/lib/libc++.a"
      " ${ADLplug_MSAN_LIBRARIES}/lib/libc++abi.a -Wl,--end-group")
  endif()
  list(JOIN ADLplug_SANITIZER_LIST "," ADLplug_SANITIZER_OPTION)
  message(STATUS "Sanitizers: ${ADLplug_SANITIZER_OPTION}")
  add_compile_options("-fsanitize=${ADLplug_SANITIZER_OPTION}" -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options("-fsanitize=${ADLplug_SANITIZER_OPTION}")

  set(ADLplug_SANITIZER_TEST_ENVIRONMENT "UBSAN_OPTIONS=print_stacktrace=1")
  if("address" IN_LIST ADLplug_SANITIZER_LIST AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND ADLplug_SANITIZER_TEST_ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1")
  endif()
  if("thread" IN_LIST ADLplug_SANITIZER_LIST)
    list(APPEND ADLplug_SANITIZER_TEST_ENVIRONMENT
      "TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1")
  endif()

  # On Windows the address sanitizer's runtime is a DLL that stays beside the
  # compiler, where a test does not look for it, and Windows answers a program
  # that cannot find a DLL with a dialog box instead of an error message. The
  # tests have its directory put at the front of PATH. The programs that the
  # build itself runs still need it on PATH from outside.
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND "address" IN_LIST ADLplug_SANITIZER_LIST)
    execute_process(
      COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=clang_rt.asan_dynamic-x86_64.dll
      OUTPUT_VARIABLE ADLplug_ASAN_RUNTIME OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(IS_ABSOLUTE "${ADLplug_ASAN_RUNTIME}" AND EXISTS "${ADLplug_ASAN_RUNTIME}")
      cmake_path(GET ADLplug_ASAN_RUNTIME PARENT_PATH ADLplug_ASAN_RUNTIME_DIRECTORY)
      set(ADLplug_SANITIZER_TEST_PATH "PATH=path_list_prepend:${ADLplug_ASAN_RUNTIME_DIRECTORY}")
    else()
      message(WARNING
        "The address sanitizer's runtime DLL was not found beside the compiler; the tests "
        "will need its directory on PATH.")
    endif()
  endif()
endif()

# Gives the tests of the directory it is called in the sanitizers' environment;
# a directory defers the call until its tests all exist.
function(adlplug_sanitizer_test_environment)
  get_property(tests DIRECTORY PROPERTY TESTS)
  if(tests AND ADLplug_SANITIZER_TEST_ENVIRONMENT)
    set_property(TEST ${tests} APPEND PROPERTY ENVIRONMENT ${ADLplug_SANITIZER_TEST_ENVIRONMENT})
  endif()
  if(tests AND ADLplug_SANITIZER_TEST_PATH)
    set_property(TEST ${tests} APPEND PROPERTY ENVIRONMENT_MODIFICATION "${ADLplug_SANITIZER_TEST_PATH}")
  endif()
endfunction()

# Profile-guided optimisation of Release builds, with ADLplug_PGO
# (cmake/PGO.cmake).
include(PGO)

# --------------------------------------------------------------------------
# Plugin identity
#
# The names, the maker and the identifiers differ from those of upstream
# ADLplug, so that both can be installed side by side; only the plugin codes
# stay. The VST3 plugins declare themselves compatible with the VST2 and VST3
# plugins of upstream (JUCE_VST3_COMPATIBLE_CLASSES below, and the VST3
# extensions of AdlplugAudioProcessor), so that hosts which support it open
# projects that used those. Audio Unit and LV2 hosts see other plugins.

set(ADLplug_MANUFACTURER_CODE "DyTc")
set(ADLplug_UPSTREAM_MANUFACTURER "Jean Pierre Cimalando")
set(ADLplug_UPSTREAM_MANUFACTURER_CODE "JPCm")
if(ADLplug_CHIP STREQUAL "OPL3")
  set(ADLplug_NAME "ADLplug-Next")
  set(ADLplug_UPSTREAM_NAME "ADLplug")
  set(ADLplug_PLUGIN_CODE "ADLM")
elseif(ADLplug_CHIP STREQUAL "OPN2")
  set(ADLplug_NAME "OPNplug-Next")
  set(ADLplug_UPSTREAM_NAME "OPNplug")
  set(ADLplug_PLUGIN_CODE "OPNM")
else()
  message(FATAL_ERROR "unsupported chip ${ADLplug_CHIP}")
endif()
set(ADLplug_LV2URI "${PROJECT_HOMEPAGE}#${ADLplug_NAME}")
set(ADLplug_BUNDLE_ID "io.github.yumasansansan.${ADLplug_NAME}")

# The emulator cores of the chip (cmake/Cores.cmake), for the tests and for the
# training of profile-guided optimisation.
include(Cores)

# --------------------------------------------------------------------------
# Dependencies

# The patches that ADLplug-Next carries for its submodules
# (cmake/Patches.cmake), applied before anything is built from them.
include(Patches)

include(ADLMIDI)
add_subdirectory("thirdparty/JUCE" EXCLUDE_FROM_ALL SYSTEM)

# On Linux, JUCE gives its modules the system libraries they need through
# pkg-config, with the compile options of the libraries' .pc files besides the
# include directories: AlmaLinux's add -pthread and -DWITH_GZFILEOP (zlib-ng),
# which nothing here needs. Those options go; the include directories, the
# libraries and the link options stay. JUCE creates the targets in its own
# directory and in that of its modules.
foreach(ADLplug_JUCE_DIRECTORY IN ITEMS "thirdparty/JUCE" "thirdparty/JUCE/modules")
  get_property(ADLplug_JUCE_TARGETS DIRECTORY "${ADLplug_JUCE_DIRECTORY}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(ADLplug_JUCE_TARGET IN LISTS ADLplug_JUCE_TARGETS)
    if(ADLplug_JUCE_TARGET MATCHES "^pkgconfig_")
      set_property(TARGET "${ADLplug_JUCE_TARGET}" PROPERTY INTERFACE_COMPILE_OPTIONS "")
    endif()
  endforeach()
endforeach()

set(ADLplug_FORMATS "")
if(ADLplug_VST3)
  list(APPEND ADLplug_FORMATS VST3)
endif()
if(ADLplug_LV2)
  list(APPEND ADLplug_FORMATS LV2)
endif()
if(ADLplug_AU)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(FATAL_ERROR "AU is not supported on ${CMAKE_SYSTEM_NAME}")
  endif()
  list(APPEND ADLplug_FORMATS AU)
endif()
if(ADLplug_AAX)
  list(APPEND ADLplug_FORMATS AAX)
endif()
if(ADLplug_Standalone)
  list(APPEND ADLplug_FORMATS Standalone)
endif()
if(NOT ADLplug_FORMATS)
  message(FATAL_ERROR "No plugin format selected.")
endif()

message("Chip: ${ADLplug_CHIP} (${ADLplug_NAME})")
message("Formats: ${ADLplug_FORMATS}")
message("Enable ASIO: ${ADLplug_ASIO}")

# --------------------------------------------------------------------------
# Plugin

juce_add_plugin(ADLplug
  PRODUCT_NAME "${ADLplug_NAME}"
  VERSION "${PROJECT_VERSION}"
  COMPANY_NAME "${PROJECT_VENDOR}"
  COMPANY_WEBSITE "${PROJECT_HOMEPAGE}"
  BUNDLE_ID "${ADLplug_BUNDLE_ID}"
  PLUGIN_MANUFACTURER_CODE "${ADLplug_MANUFACTURER_CODE}"
  PLUGIN_CODE "${ADLplug_PLUGIN_CODE}"
  FORMATS ${ADLplug_FORMATS}
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  NEEDS_CURL FALSE
  NEEDS_WEB_BROWSER FALSE
  VST3_CATEGORIES Instrument Synth
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  AAX_IDENTIFIER "${ADLplug_BUNDLE_ID}"
  LV2URI "${ADLplug_LV2URI}"
  # The icons keep the file names of upstream.
  ICON_BIG "resources/application/${ADLplug_UPSTREAM_NAME}-96.png"
  ICON_SMALL "resources/application/${ADLplug_UPSTREAM_NAME}-32.png")

juce_generate_juce_header(ADLplug)
# JuceHeader.h is generated by JUCE, so like the JUCE headers it includes it
# is not held to the warning flags of ADLplug's own code.
get_target_property(ADLplug_JUCE_LIBRARY_CODE ADLplug JUCE_GENERATED_SOURCES_DIRECTORY)
target_include_directories(ADLplug SYSTEM PRIVATE "${ADLplug_JUCE_LIBRARY_CODE}")
adlplug_set_language_standard(ADLplug)
# JUCE builds each format in a target of its own, which asks for C++17 and so,
# left alone, is compiled as GNU C++17 (-std=gnu++17, or no flag where that is
# the compiler's default). The formats are compiled as the shared code is.
foreach(ADLplug_FORMAT IN LISTS ADLplug_FORMATS)
  adlplug_set_language_standard("ADLplug_${ADLplug_FORMAT}")
endforeach()

# The plugins of a sanitizer build do not carry the sanitizers' runtime: it
# belongs to the program that loads the plugin, and its symbols are left
# undefined in the plugin's shared object. JUCE asks the linker to refuse a
# plugin that has undefined symbols, which is right for a plugin that is to be
# published and wrong for this one.
if(ADLplug_SANITIZERS)
  foreach(ADLplug_FORMAT IN LISTS ADLplug_FORMATS)
    get_target_property(ADLplug_FORMAT_LINK_OPTIONS
      "ADLplug_${ADLplug_FORMAT}" LINK_OPTIONS)
    if(ADLplug_FORMAT_LINK_OPTIONS)
      list(REMOVE_ITEM ADLplug_FORMAT_LINK_OPTIONS "-Wl,--no-undefined")
      set_property(TARGET "ADLplug_${ADLplug_FORMAT}"
        PROPERTY LINK_OPTIONS "${ADLplug_FORMAT_LINK_OPTIONS}")
    endif()
  endforeach()
endif()

if(TARGET ADLplug_LV2)
  # JUCE writes the whole numbers of the parameters' atom:Float ranges as
  # integers; the script rewrites them after JUCE's helper has written dsp.ttl.
  add_custom_command(TARGET ADLplug_LV2 POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DTTL=$<TARGET_FILE_DIR:ADLplug_LV2>/dsp.ttl"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LV2FloatLiterals.cmake"
    VERBATIM)
endif()

target_sources(ADLplug PRIVATE
  "sources/utility/midi.cc"
  "sources/utility/simple_fifo.cc"
  "sources/utility/pak.cc"
  "sources/midi/insnames.cc"
  "sources/plugin_editor.cc"
  "sources/plugin_processor.cc"
  "sources/plugin_state.cc"
  "sources/bank_manager.cc"
  "sources/bank_load.cc"
  "sources/configuration.cc"
  "sources/messages.cc"
  "sources/worker.cc"
  "sources/resources.c"
  "sources/ui/look_and_feel.cc"
  "sources/ui/components/algorithm_component.cc"
  "sources/ui/components/indicator_NxM.cc"
  "sources/ui/components/knob_component.cc"
  "sources/ui/components/midi_keyboard_ex.cc"
  "sources/ui/components/new_program_editor.cc"
  "sources/ui/components/program_name_editor.cc"
  "sources/ui/components/styled_knobs.cc"
  "sources/ui/components/vu_meter.cc"
  "sources/ui/components/wave_label.cc"
  "sources/ui/utility/knobman_skin.cc"
  "sources/ui/utility/graphics.cc"
  "sources/ui/utility/image.cc"
  "sources/ui/utility/key_maps.cc")

target_include_directories(ADLplug PUBLIC "sources")

# ADLplug reads no audio files, so it needs none of JUCE's audio codecs. JUCE 9
# builds the MP3 decoder by default, and its tables are computed when the
# plug-in loads.
target_compile_definitions(ADLplug PUBLIC
  JUCE_STRICT_REFCOUNTEDPOINTER=1
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_USE_FLAC=0
  JUCE_USE_OGGVORBIS=0
  JUCE_USE_MP3AUDIOFORMAT=0
  JUCE_USE_WINDOWS_MEDIA_FORMAT=0
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0)

# The version to show (cmake/Version.cmake), and what the plugin takes over
# from upstream ADLplug: the VST2 and VST3 classes of its plugin, which hosts may
# replace with this one (see "Plugin identity"), and its configuration
# (sources/configuration.cc).
#
# JUCE_VST3_COMPATIBLE_CLASSES names the classes of upstream by their IDs in
# memory. JUCE 9.0.2 writes those into moduleinfo.json byte by byte, where hosts
# read class IDs in their notation (VST3::UID::toString()). The two agree except
# on Windows, where the first three fields of an ID are little-endian in memory:
# there the classes are named in the notation as well, so that hosts find them
# in moduleinfo.json, and the IDs in memory stay, as hosts give those when they
# ask for the parameters of an old class. In the notation, the VST2 plugin is
# "VST", the plugin code and at most 9 characters of the name in lower case, and
# a JUCE VST3 plugin ABCDEF019182FAEB, the maker code and the plugin code.
# tests/format/vst3_compatibility.cmake checks moduleinfo.json.
string(HEX "${ADLplug_PLUGIN_CODE}" ADLplug_PLUGIN_CODE_HEX)
string(HEX "${ADLplug_MANUFACTURER_CODE}" ADLplug_MANUFACTURER_CODE_HEX)
string(HEX "${ADLplug_UPSTREAM_MANUFACTURER_CODE}" ADLplug_UPSTREAM_MANUFACTURER_CODE_HEX)
string(TOLOWER "${ADLplug_UPSTREAM_NAME}" ADLplug_UPSTREAM_VST2_NAME)
string(SUBSTRING "${ADLplug_UPSTREAM_VST2_NAME}" 0 9 ADLplug_UPSTREAM_VST2_NAME)
string(HEX "${ADLplug_UPSTREAM_VST2_NAME}" ADLplug_UPSTREAM_VST2_NAME_HEX)
string(LENGTH "${ADLplug_UPSTREAM_VST2_NAME_HEX}" ADLplug_UPSTREAM_VST2_NAME_LENGTH)
math(EXPR ADLplug_UPSTREAM_VST2_PADDING "18 - ${ADLplug_UPSTREAM_VST2_NAME_LENGTH}")
string(REPEAT "0" ${ADLplug_UPSTREAM_VST2_PADDING} ADLplug_UPSTREAM_VST2_PADDING)
string(TOUPPER "565354${ADLplug_PLUGIN_CODE_HEX}${ADLplug_UPSTREAM_VST2_NAME_HEX}${ADLplug_UPSTREAM_VST2_PADDING}"
  ADLplug_UPSTREAM_VST2_CLASS)
string(TOUPPER "ABCDEF019182FAEB${ADLplug_UPSTREAM_MANUFACTURER_CODE_HEX}${ADLplug_PLUGIN_CODE_HEX}"
  ADLplug_UPSTREAM_VST3_CLASS)
string(TOUPPER "ABCDEF019182FAEB${ADLplug_MANUFACTURER_CODE_HEX}${ADLplug_PLUGIN_CODE_HEX}" ADLplug_VST3_CLASS)
set(ADLplug_UPSTREAM_CLASSES
  "VST3Interface::vst2PluginId (0x${ADLplug_PLUGIN_CODE_HEX}, \"${ADLplug_UPSTREAM_NAME}\")"
  "VST3Interface::jucePluginId (0x${ADLplug_UPSTREAM_MANUFACTURER_CODE_HEX}, 0x${ADLplug_PLUGIN_CODE_HEX})")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  list(APPEND ADLplug_UPSTREAM_CLASSES
    "VST3Interface::hexStringToId (\"${ADLplug_UPSTREAM_VST2_CLASS}\")"
    "VST3Interface::hexStringToId (\"${ADLplug_UPSTREAM_VST3_CLASS}\")")
endif()
list(JOIN ADLplug_UPSTREAM_CLASSES ", " ADLplug_UPSTREAM_CLASSES)
target_compile_definitions(ADLplug PRIVATE
  "ADLPLUG_VERSION_DISPLAY=\"${ADLplug_VERSION_DISPLAY}\""
  "ADLPLUG_UPSTREAM_NAME=\"${ADLplug_UPSTREAM_NAME}\""
  "ADLPLUG_UPSTREAM_MANUFACTURER=\"${ADLplug_UPSTREAM_MANUFACTURER}\""
  "ADLPLUG_UPSTREAM_VST2_CLASS=\"${ADLplug_UPSTREAM_VST2_CLASS}\"")
target_compile_definitions(ADLplug PUBLIC "JUCE_VST3_COMPATIBLE_CLASSES=${ADLplug_UPSTREAM_CLASSES}")

if(ADLplug_ASIO)
  # JUCE bundles the ASIO SDK headers (2025, dual Steinberg/GPLv3).
  target_compile_definitions(ADLplug PUBLIC JUCE_ASIO=1)
endif()

# The modules are linked privately, as JUCE's plugin targets expect: the shared
# code compiles them once, and the plugin wrappers see their headers through
# JUCE's own settings. Linked publicly, every wrapper compiled all the modules
# again, and LLD stopped on duplicate symbols when a link took a module's object
# from the shared code as well (the standalone program, in AlmaLinux).
target_link_libraries(ADLplug PRIVATE
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_audio_devices
  juce::juce_audio_formats
  juce::juce_audio_basics
  juce::juce_gui_extra
  juce::juce_gui_basics
  juce::juce_graphics
  juce::juce_data_structures
  juce::juce_events
  juce::juce_core)

# LTO comes from the Release setting above rather than juce_recommended_lto_flags,
# which would add full -flto to this target alone.
target_link_libraries(ADLplug PRIVATE juce::juce_recommended_config_flags)

# --------------------------------------------------------------------------
# Chip-specific sources

if(ADLplug_CHIP STREQUAL "OPL3")
  target_compile_definitions(ADLplug PUBLIC "ADLPLUG_OPL3")

  target_sources(ADLplug PRIVATE
    "sources/opl3/parameter_block.cc"
    "sources/opl3/adl/chip_settings.cc"
    "sources/opl3/adl/instrument.cc"
    "sources/opl3/adl/instrument_properties.cc"
    "sources/opl3/adl/player.cc"
    "sources/opl3/adl/measurer/measurer.cpp"
    "sources/opl3/ui/operator_editor.cc"
    "sources/opl3/ui/about_component.cc"
    "sources/opl3/ui/main_component.cc"
    "sources/opl3/ui/components/algorithm_help.cc"
    "sources/opl3/ui/components/opl3_waves.cc")

  add_library(wopl STATIC "thirdparty/libADLMIDI/src/wopl/wopl_file.c")
  adlplug_set_language_standard(wopl)
  target_include_directories(wopl SYSTEM PUBLIC "thirdparty/libADLMIDI/src")
  set_property(TARGET wopl PROPERTY POSITION_INDEPENDENT_CODE ON)
  # Upstream libADLMIDI code; its strncpy calls fill fixed-width name fields.
  target_compile_definitions(wopl PRIVATE $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>)

  # measurer.cpp drives libADLMIDI's own DosBoxOPL3 emulator, which lives in
  # the library's private src/ tree rather than its public headers.
  target_include_directories(ADLplug SYSTEM PRIVATE "thirdparty/libADLMIDI/src")
  target_link_libraries(ADLplug PUBLIC wopl ADLMIDI_static)

elseif(ADLplug_CHIP STREQUAL "OPN2")
  target_compile_definitions(ADLplug PUBLIC "ADLPLUG_OPN2")

  target_sources(ADLplug PRIVATE
    "sources/opn2/parameter_block.cc"
    "sources/opn2/adl/chip_settings.cc"
    "sources/opn2/adl/instrument.cc"
    "sources/opn2/adl/instrument_properties.cc"
    "sources/opn2/adl/player.cc"
    "sources/opn2/adl/measurer/measurer.cpp"
    "sources/opn2/ui/operator_editor.cc"
    "sources/opn2/ui/about_component.cc"
    "sources/opn2/ui/main_component.cc"
    "sources/opn2/ui/components/algorithm_help.cc"
    "sources/opn2/ui/components/ssgeg_waves.cc")

  add_library(wopn STATIC "thirdparty/libOPNMIDI/src/wopn/wopn_file.c")
  adlplug_set_language_standard(wopn)
  target_include_directories(wopn SYSTEM PUBLIC "thirdparty/libOPNMIDI/src")
  set_property(TARGET wopn PROPERTY POSITION_INDEPENDENT_CODE ON)
  # Upstream libOPNMIDI code; its strncpy calls fill fixed-width name fields.
  target_compile_definitions(wopn PRIVATE $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>)

  # measurer.cpp drives libOPNMIDI's own MameOPN2 emulator, which lives in
  # the library's private src/ tree rather than its public headers.
  target_include_directories(ADLplug SYSTEM PRIVATE "thirdparty/libOPNMIDI/src")
  target_link_libraries(ADLplug PUBLIC wopn OPNMIDI_static)
endif()

# --------------------------------------------------------------------------
# simpleini

add_library(simple-ini INTERFACE)
target_include_directories(simple-ini SYSTEM INTERFACE "thirdparty/simpleini")

target_link_libraries(ADLplug PUBLIC simple-ini)

# --------------------------------------------------------------------------
# Instrument banks
#
# tools/bankgen generates the pack of instrument banks from the submodules in
# thirdparty/ when the plugin is built (README, "Instrument banks"), and
# sources/resources.c embeds it with #embed <banks.pak>. The tool is built for
# the chip of the plugin. With ADLplug_GREYZONE_BANKS, the pack includes the
# banks of the grey zone.

juce_add_console_app(ADLplug_bankgen)
adlplug_set_language_standard(ADLplug_bankgen)
target_sources(ADLplug_bankgen PRIVATE "tools/bankgen/bankgen.cc")
target_include_directories(ADLplug_bankgen PRIVATE "sources")
target_compile_definitions(ADLplug_bankgen PRIVATE
  "ADLPLUG_${ADLplug_CHIP}"
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0)
target_link_libraries(ADLplug_bankgen PRIVATE
  simple-ini
  juce::juce_core
  juce::juce_recommended_config_flags)
if(ADLplug_CHIP STREQUAL "OPL3")
  target_sources(ADLplug_bankgen PRIVATE "tools/bankgen/woplx.cc")
  target_link_libraries(ADLplug_bankgen PRIVATE wopl)
else()
  # The banks converted from other formats are measured as the plugin measures
  # its instruments, on libOPNMIDI's MameOPN2.
  target_sources(ADLplug_bankgen PRIVATE
    "tools/bankgen/opn2_import.cc"
    "sources/opn2/adl/instrument.cc"
    "sources/opn2/adl/measurer/measurer.cpp")
  target_include_directories(ADLplug_bankgen SYSTEM PRIVATE "thirdparty/libOPNMIDI/src")
  target_link_libraries(ADLplug_bankgen PRIVATE wopn OPNMIDI_static)
endif()

set(ADLplug_BANKS_DIR "${CMAKE_CURRENT_BINARY_DIR}/banks")
set(ADLplug_BANKS_PAK "${ADLplug_BANKS_DIR}/banks.pak")
set(ADLplug_BANKS_NOTICES "${ADLplug_BANKS_DIR}/${ADLplug_NAME}-banks.txt")
set(ADLplug_BANKGEN_ARGS --source "${CMAKE_CURRENT_SOURCE_DIR}")
if(ADLplug_CHIP STREQUAL "OPN2")
  list(APPEND ADLplug_BANKGEN_ARGS --list "${CMAKE_CURRENT_SOURCE_DIR}/resources/opn2/banks.ini")
endif()
set(ADLplug_BANKGEN_GREYZONE "")
if(ADLplug_GREYZONE_BANKS)
  set(ADLplug_BANKGEN_GREYZONE --greyzone)
endif()

# The tool writes a depfile of the files it read, so that a change to any of
# them generates the pack again.
add_custom_command(
  OUTPUT "${ADLplug_BANKS_PAK}" "${ADLplug_BANKS_NOTICES}"
  COMMAND ADLplug_bankgen ${ADLplug_BANKGEN_ARGS} ${ADLplug_BANKGEN_GREYZONE}
    --pak "${ADLplug_BANKS_PAK}" --notices "${ADLplug_BANKS_NOTICES}"
    --depfile "${ADLplug_BANKS_DIR}/banks.d"
  DEPENDS ADLplug_bankgen
  DEPFILE "${ADLplug_BANKS_DIR}/banks.d"
  COMMENT "Generating the instrument banks"
  VERBATIM)
add_custom_target(ADLplug_banks DEPENDS "${ADLplug_BANKS_PAK}" "${ADLplug_BANKS_NOTICES}")
add_dependencies(ADLplug ADLplug_banks)

# Lets a source file embed the pack. Source file properties belong to the
# directory that sets them, so tests/ calls this for what it compiles too, after
# adlplug_own_sources(), which replaces the compile options.
function(adlplug_embed_banks source)
  set_property(SOURCE "${source}" APPEND PROPERTY COMPILE_OPTIONS "--embed-dir=${ADLplug_BANKS_DIR}")
  set_property(SOURCE "${source}" APPEND PROPERTY OBJECT_DEPENDS "${ADLplug_BANKS_PAK}")
endfunction()

# --------------------------------------------------------------------------
# Developer tools

if(ADLplug_BUILD_TOOLS)
  # tools/render plays a fixed MIDI sequence through a VST3 build of the plugin
  # and dumps the output, so two builds (Debug against Release/ThinLTO, say)
  # can be compared sample for sample.
  juce_add_console_app(ADLplug_render)
  adlplug_set_language_standard(ADLplug_render)
  target_sources(ADLplug_render PRIVATE "tools/render/render.cc")
  # A console host has to pump the message loop itself; JUCE only offers that
  # with modal loops enabled. Fine for a tool, never for the plugin.
  target_compile_definitions(ADLplug_render PRIVATE
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_LV2=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
  target_link_libraries(ADLplug_render PRIVATE
    juce::juce_audio_processors
    juce::juce_recommended_config_flags)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # X errors are printed rather than fatal (tools/render/x_errors.h).
    find_package(X11 REQUIRED)
    target_sources(ADLplug_render PRIVATE "tools/render/x_errors.cc")
    target_link_libraries(ADLplug_render PRIVATE X11::X11)
  endif()
endif()

# --------------------------------------------------------------------------
# Settings for ADLplug's own translation units only
#
# juce_add_plugin() compiles the JUCE module sources into this same target
# through INTERFACE_SOURCES, so anything set target-wide lands on JUCE's code
# too. The Windows header macros and the warning policy are therefore attached
# to our sources individually. Headers from JUCE and the other dependencies
# stay quiet because every third-party include directory above is SYSTEM.

get_target_property(ADLplug_OWN_SOURCES ADLplug SOURCES)
list(FILTER ADLplug_OWN_SOURCES INCLUDE REGEX "^sources/")
if(TARGET ADLplug_render)
  get_target_property(ADLplug_RENDER_SOURCES ADLplug_render SOURCES)
  list(FILTER ADLplug_RENDER_SOURCES INCLUDE REGEX "^tools/")
  list(APPEND ADLplug_OWN_SOURCES ${ADLplug_RENDER_SOURCES})
endif()
get_target_property(ADLplug_BANKGEN_SOURCES ADLplug_bankgen SOURCES)
list(FILTER ADLplug_BANKGEN_SOURCES INCLUDE REGEX "^(sources|tools)/")
list(APPEND ADLplug_OWN_SOURCES ${ADLplug_BANKGEN_SOURCES})
list(REMOVE_DUPLICATES ADLplug_OWN_SOURCES)

set(ADLplug_OWN_COMPILE_DEFINITIONS "")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  # NOMINMAX / WIN32_LEAN_AND_MEAN keep <windows.h> from defining min/max macros
  # that break std::min and std::max. JUCE defines NOMINMAX in its own native
  # headers, so passing it to JUCE's translation units only causes redefinition
  # warnings there.
  # _CRT_SECURE_NO_WARNINGS: strncpy and sscanf are used deliberately for
  # fixed-width, zero-padded fields; the MSVC CRT flags every such call and
  # suggests its non-portable *_s forms.
  set(ADLplug_OWN_COMPILE_DEFINITIONS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
  set(ADLplug_WARNING_FLAGS -Wall -Wextra -Wpedantic)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # ADLplug's own code is kept clean of these as well. The spellings are
    # Clang's; GCC is not a supported compiler.
    list(APPEND ADLplug_WARNING_FLAGS
      -Wconversion -Wdouble-promotion -Wshadow-all -Wcast-qual -Wcast-align
      -Wformat=2 -Wformat-signedness -Wimplicit-fallthrough -Wundef
      -Wunreachable-code -Wcomma -Wmissing-prototypes
      -Wmissing-variable-declarations -Wdeprecated -Wmultichar
      -Wfour-char-constants -Wextra-semi -Wextra-semi-stmt -Wnewline-eof
      -Wreserved-identifier
      # Asked of the C as well as the C++. C23 has nullptr, so nought standing in
      # for it is as much a thing this project does not write there; clang has
      # nothing to say about it in C today, and clang-tidy's modernize-use-nullptr
      # is what catches it there (.clang-tidy). Asking for it of both languages
      # costs nothing and is what the project means, whichever of them clang comes
      # to say it in.
      -Wzero-as-null-pointer-constant
      $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
      $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
      $<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
      $<$<COMPILE_LANGUAGE:CXX>:-Wsuggest-override>
      $<$<COMPILE_LANGUAGE:CXX>:-Wsuggest-destructor-override>
      $<$<COMPILE_LANGUAGE:CXX>:-Winconsistent-missing-destructor-override>
      $<$<COMPILE_LANGUAGE:CXX>:-Wundefined-func-template>
      $<$<COMPILE_LANGUAGE:CXX>:-Wunused-member-function>
      $<$<COMPILE_LANGUAGE:CXX>:-Wunused-template>)
  endif()
  if(ADLplug_WERROR)
    list(APPEND ADLplug_WARNING_FLAGS -Werror)
  else()
    # Falling off the end of a non-void function is undefined behaviour.
    list(APPEND ADLplug_WARNING_FLAGS -Werror=return-type)
  endif()
endif()

# --------------------------------------------------------------------------
# Coverage
#
# ADLplug_COVERAGE measures which of this project's own code a run reaches:
# every source that adlplug_own_sources() names is compiled to count what runs
# (-fprofile-instr-generate -fcoverage-mapping), and every program that links
# such an object writes a profile when it ends. Nothing of JUCE or of the
# libraries is counted: what the measurement is for is knowing what the tests and
# the fuzz corpus of this project never reach, so that the next test or target has
# somewhere to aim, and a line of a submodule is not this project's to reach.
#
# ci/coverage.sh runs the tests and the fuzz corpus of a build configured this way,
# merges what they wrote and reports it.
#
# Not for Release builds: those are optimised and built with a profile of their own
# (ADLplug_PGO, cmake/PGO.cmake), and a line that an optimiser has folded into
# another answers nothing about which line ran. The *-coverage presets configure
# Debug, where each line is its own.
if(ADLplug_COVERAGE)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR
      "ADLplug_COVERAGE is not for Release builds, which are optimised and carry a profile of "
      "their own. Configure Debug, as the *-coverage presets do.")
  endif()
  set(ADLplug_COVERAGE_FLAGS -fprofile-instr-generate -fcoverage-mapping)
  # Every program that links an instrumented object needs the runtime that writes
  # the profile, and the tools and the tests link them as the plugin does.
  add_link_options(-fprofile-instr-generate)
  message(STATUS "Coverage: this project's own sources count what runs")
endif()

# Gives source files the settings above. Source file properties belong to the
# directory that sets them, so tests/ calls this again for what it compiles.
function(adlplug_own_sources)
  if(ADLplug_OWN_COMPILE_DEFINITIONS)
    set_source_files_properties(${ARGN} PROPERTIES COMPILE_DEFINITIONS "${ADLplug_OWN_COMPILE_DEFINITIONS}")
  endif()
  set(options ${ADLplug_WARNING_FLAGS} ${ADLplug_COVERAGE_FLAGS})
  if(options)
    set_source_files_properties(${ARGN} PROPERTIES COMPILE_OPTIONS "${options}")
  endif()
endfunction()

adlplug_own_sources(${ADLplug_OWN_SOURCES})
adlplug_embed_banks("sources/resources.c")

# --------------------------------------------------------------------------
# MediaPerch's resampler, which is held to the same flags and is therefore built
# here, after the function that applies them.

include("${CMAKE_CURRENT_LIST_DIR}/cmake/MediaPerch.cmake")

target_link_libraries(ADLplug PUBLIC mediaperch-resample)

# --------------------------------------------------------------------------
# Tests

if(ADLplug_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# The fuzz targets (fuzz/): replayed on their inputs as tests, and built with
# libFuzzer on request.
if(ADLplug_BUILD_TESTS OR ADLplug_BUILD_FUZZERS)
  add_subdirectory(fuzz)
endif()

# --------------------------------------------------------------------------
# Installation (Linux/BSD; the other platforms install through their bundles)
#
# The plugins go where hosts look for them. VST3 hosts look in lib/vst3 under
# /usr and /usr/local, whatever the library directory of the system. LV2 hosts
# mostly look where lilv does, in the library directory of the system that
# built lilv: lib/lv2, or lib64/lv2 on systems whose libraries are in lib64
# (ci/package-rpm.sh sets that). JUCE's bundles are copied whole.
set(ADLplug_INSTALL_VST3DIR "lib/vst3" CACHE STRING
  "Installation directory of the VST3 plugin, relative to the prefix")
set(ADLplug_INSTALL_LV2DIR "lib/lv2" CACHE STRING
  "Installation directory of the LV2 plugin, relative to the prefix")

if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Windows" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
  if(ADLplug_VST3)
    install(DIRECTORY "$<GENEX_EVAL:$<TARGET_PROPERTY:ADLplug_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
      DESTINATION "${ADLplug_INSTALL_VST3DIR}" USE_SOURCE_PERMISSIONS)
  endif()
  if(ADLplug_LV2)
    install(DIRECTORY "$<GENEX_EVAL:$<TARGET_PROPERTY:ADLplug_LV2,JUCE_PLUGIN_ARTEFACT_FILE>>"
      DESTINATION "${ADLplug_INSTALL_LV2DIR}" USE_SOURCE_PERMISSIONS)
  endif()
  if(ADLplug_Standalone)
    install(PROGRAMS "$<TARGET_FILE:ADLplug_Standalone>"
      DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "resources/application/${ADLplug_UPSTREAM_NAME}.desktop"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
      RENAME "${ADLplug_NAME}.desktop")
    install(FILES "resources/application/${ADLplug_UPSTREAM_NAME}-32.png"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/pixmaps"
      RENAME "${ADLplug_NAME}.png")
    foreach(size 32;96)
      install(FILES "resources/application/${ADLplug_UPSTREAM_NAME}-${size}.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps"
        RENAME "${ADLplug_NAME}.png")
    endforeach()
  endif()
  install(FILES "${ADLplug_BANKS_NOTICES}"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/${ADLplug_NAME}")
endif()

# --------------------------------------------------------------------------
# The resource compiler
#
# On Windows, the resources of the plugins are compiled by LLVM's resource
# compiler, which is checked here, once the targets that have resources are
# set up, as the rest of the toolchain was at the top
# (cmake/LLVMToolchain.cmake).

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  adlplug_check_llvm_tool("resource compiler" "${CMAKE_RC_COMPILER}" "^llvm-rc(\\.exe)?$")
endif()
