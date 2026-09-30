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

add_library(adlplug-opl3-core STATIC EXCLUDE_FROM_ALL ${ADLplug_OPL3_CORE_SOURCES})
adlplug_set_language_standard(adlplug-opl3-core)
set_property(TARGET adlplug-opl3-core PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(adlplug-opl3-core SYSTEM PRIVATE
  "${PROJECT_SOURCE_DIR}/thirdparty/libADLMIDI/src/chips")
adlplug_own_sources(${ADLplug_OPL3_CORE_SOURCES})

# A dependency of the build alone: the library's own install(EXPORT), which this
# project never runs but CMake checks, would otherwise ask for the core in an
# export set too.
target_link_libraries(ADLMIDI_static PRIVATE $<BUILD_INTERFACE:adlplug-opl3-core>)
