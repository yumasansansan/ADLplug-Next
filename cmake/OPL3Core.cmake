# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
# ADLplug-Next's own OPL3 core (sources/opl3/core), which libADLMIDI makes as
# emulator 14 with this project's patch (patches/libADLMIDI/0010-...). The
# library declares the function that makes it and leaves the code to this
# build: a library of its own, compiled with this project's warning flags, and
# linked into libADLMIDI, which is where the function is called from. Built
# only when something links libADLMIDI.
#
# The chip's base class comes from the library's own source tree, which is
# upstream code, and is included as a system header, as the plugin includes
# the library.

if(NOT USE_ADLPLUG_OPL3_EMULATOR)
  return()
endif()

set(ADLplug_OPL3_CORE_SOURCES
  "${PROJECT_SOURCE_DIR}/sources/opl3/core/core.cc"
  "${PROJECT_SOURCE_DIR}/sources/opl3/core/adlmidi_chip.cc")

# The core's pass written by hand for AVX2 (pass_avx2.S), in the x86-64 builds
# whose C++ is not for AVX-512, whose whole width the C++ passes use where the
# build has it (ADLplug_wide_vectors: avx512, and native on a machine with
# AVX-512); Apple's builds are for arm64. Clang, the build's C compiler,
# assembles it with the warning flags of the project's own sources, and the core
# takes it in its form for the processors without AVX-VNNI or for those with it
# (core.cc).
set(ADLplug_OPL3_AVX2 OFF)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$" AND NOT APPLE AND NOT ADLplug_wide_vectors)
  set(ADLplug_OPL3_AVX2 ON)
  enable_language(ASM)
  list(APPEND ADLplug_OPL3_CORE_SOURCES "${PROJECT_SOURCE_DIR}/sources/opl3/core/pass_avx2.S")
endif()

add_library(adlplug-opl3-core STATIC EXCLUDE_FROM_ALL ${ADLplug_OPL3_CORE_SOURCES})
adlplug_set_language_standard(adlplug-opl3-core)
set_property(TARGET adlplug-opl3-core PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(adlplug-opl3-core SYSTEM PRIVATE
  "${PROJECT_SOURCE_DIR}/thirdparty/libADLMIDI/src/chips")
if(ADLplug_OPL3_AVX2)
  target_compile_definitions(adlplug-opl3-core PRIVATE ADLPLUG_OPL3_AVX2)
endif()
adlplug_own_sources(${ADLplug_OPL3_CORE_SOURCES})

# A dependency of the build alone: the library's own install(EXPORT), which this
# project never runs but CMake checks, would otherwise ask for the core in an
# export set too.
target_link_libraries(ADLMIDI_static PRIVATE $<BUILD_INTERFACE:adlplug-opl3-core>)
