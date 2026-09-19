# SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
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
#
# libADLMIDI / libOPNMIDI
#
# Both libraries declare `cmake_minimum_required(VERSION 3.2...4.0)`. CMake 4.x
# refuses a <min> below 3.5, so raise the floor for the duration of the two
# add_subdirectory() calls and restore it afterwards.

set(_ADLplug_saved_policy_min "${CMAKE_POLICY_VERSION_MINIMUM}")
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

# Real-time plugin use: no file loading, no sequencer, no embedded banks
# (ADLplug ships its own bank set in sources/resources.c).
set(WITH_MIDI_SEQUENCER OFF CACHE BOOL "" FORCE)
set(WITH_XMI_SUPPORT OFF CACHE BOOL "" FORCE)
set(WITH_EMBEDDED_BANKS OFF CACHE BOOL "" FORCE)
set(WITH_HQ_RESAMPLER OFF CACHE BOOL "" FORCE)

# Both libraries call the standard C string and file functions that the MSVC
# CRT reports as "unsafe". That is upstream code behaving as intended, so keep
# it out of this build's warning output on Windows.
set(ADLplug_UPSTREAM_CRT_DEFS $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>)

# For compilers other than MSVC, both libraries change the compiler and linker
# flags of their directories: -O3 becomes -O2 with -ffunction-sections and
# -fdata-sections, and -fno-omit-frame-pointer, -Wall -Wextra, -std=c89 or
# -std=c++98 and -Wl,--no-undefined are added. Their libraries also get
# -fvisibility options of their own. None of that is wanted: the libraries are
# compiled with the flags of the rest of the build. A directory's flags are
# those it holds when its processing ends, so the flags saved here are put back
# then: CMake includes cmake/UpstreamFlags.cmake at the end of each library's
# project() call, and that file defers adlplug_restore_upstream_flags() to the
# end of the library's directory. The language standards that the libraries
# set for their code (C90, C++98 or C++14, and C++14 for the YMFM sources)
# stay.
set(ADLplug_UPSTREAM_FLAG_VARIABLES "")
foreach(ADLplug_KIND IN ITEMS C CXX EXE_LINKER SHARED_LINKER MODULE_LINKER)
  foreach(ADLplug_CONFIG IN ITEMS "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
    list(APPEND ADLplug_UPSTREAM_FLAG_VARIABLES "CMAKE_${ADLplug_KIND}_FLAGS${ADLplug_CONFIG}")
  endforeach()
endforeach()
foreach(ADLplug_VARIABLE IN LISTS ADLplug_UPSTREAM_FLAG_VARIABLES)
  set("ADLplug_SAVED_${ADLplug_VARIABLE}" "${${ADLplug_VARIABLE}}")
endforeach()

# A macro, so that it sets the variables of the directory it is called in.
macro(adlplug_restore_upstream_flags)
  foreach(ADLplug_VARIABLE IN LISTS ADLplug_UPSTREAM_FLAG_VARIABLES)
    set("${ADLplug_VARIABLE}" "${ADLplug_SAVED_${ADLplug_VARIABLE}}")
  endforeach()
endmacro()

set(CMAKE_PROJECT_libADLMIDI_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/UpstreamFlags.cmake")
set(CMAKE_PROJECT_libOPNMIDI_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/UpstreamFlags.cmake")

# Emulator cores: each library builds a core when its USE_*_EMULATOR option is
# ON, and README lists them. ADLplug builds every core: the choice of chips and
# cores matters more than how fast the slowest of them runs. The libraries
# already build all but the low-level (LLE) ones by default, and leave those
# out as too slow for real time on ordinary processors, so they are turned on
# here. These are cache defaults: -D on the command line takes precedence, and
# a build directory configured earlier keeps the value in its cache. Leaving a
# core out saves only its code; a project saved with it plays on the default
# core for the same chip (available_emulator() in sources/*/adl/chip_settings.cc).
set(USE_NUKED_OPL2_LLE_EMULATOR ON CACHE BOOL "Use Nuked OPL2-LLE emulator [!EXTRA HEAVY!]")
set(USE_NUKED_OPL3_LLE_EMULATOR ON CACHE BOOL "Use Nuked OPL3-LLE emulator [!EXTRA HEAVY!]")
set(USE_NUKED_OPN2_LLE_EMULATOR ON CACHE BOOL "Use Nuked OPN2-LLE emulator [!EXTRA HEAVY!]")
set(USE_NUKED_OPNA_LLE_EMULATOR ON CACHE BOOL "Use Nuked OPNA-LLE emulator [!EXTRA HEAVY!]")

set(libADLMIDI_STATIC ON CACHE BOOL "" FORCE)
set(libADLMIDI_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory("${PROJECT_SOURCE_DIR}/thirdparty/libADLMIDI" EXCLUDE_FROM_ALL SYSTEM)
target_compile_definitions(ADLMIDI_static PRIVATE "ADLMIDI_EXPORT=" ${ADLplug_UPSTREAM_CRT_DEFS})
target_compile_definitions(ADLMIDI_static PUBLIC "ADLMIDI_UNSTABLE_API=")

set(libOPNMIDI_STATIC ON CACHE BOOL "" FORCE)
set(libOPNMIDI_SHARED OFF CACHE BOOL "" FORCE)
set(USE_VGM_FILE_DUMPER OFF CACHE BOOL "" FORCE)
add_subdirectory("${PROJECT_SOURCE_DIR}/thirdparty/libOPNMIDI" EXCLUDE_FROM_ALL SYSTEM)
target_compile_definitions(OPNMIDI_static PRIVATE "OPNMIDI_EXPORT=" ${ADLplug_UPSTREAM_CRT_DEFS})
target_compile_definitions(OPNMIDI_static PUBLIC "OPNMIDI_UNSTABLE_API=")

unset(CMAKE_PROJECT_libADLMIDI_INCLUDE)
unset(CMAKE_PROJECT_libOPNMIDI_INCLUDE)

# The -fvisibility options that the libraries give their targets go; the
# visibility settings of the build apply to them as to every other target.
foreach(ADLplug_TARGET IN ITEMS ADLMIDI_static OPNMIDI_static)
  get_target_property(ADLplug_OPTIONS ${ADLplug_TARGET} COMPILE_OPTIONS)
  if(ADLplug_OPTIONS)
    list(REMOVE_ITEM ADLplug_OPTIONS
      "-fvisibility=hidden" "$<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>")
    set_property(TARGET ${ADLplug_TARGET} PROPERTY COMPILE_OPTIONS "${ADLplug_OPTIONS}")
  endif()
endforeach()

# The low-level emulators (LLE) simulate their chips gate by gate and keep the
# latches in int. A bit shifted left through one of them passes the sign in the
# end, which C leaves undefined and which the sanitizers stop the program at:
#
#   nuked_fmopl3.c:48:48: runtime error: left shift of 2147483647 by 1 places
#   cannot be represented in type 'int'
#
# Wrapping is what the code means, since a hardware shift register does nothing
# else, and -fwrapv promises exactly that. These sources are compiled with it in
# every build: the code means the same thing whether or not the sanitizers are
# on, and the flag only keeps the compiler from assuming the overflow cannot
# happen. The shifts are in hundreds of places here, so the patches of D79,
# which are for faults with one right answer each, are not the way.
set(ADLplug_ADLMIDI_LLE_SOURCES
  "src/chips/ym3812_lle/nopl2.c" "src/chips/ym3812_lle/nuked_fmopl2.c"
  "src/chips/ymf262_lle/nopl3.c" "src/chips/ymf262_lle/nuked_fmopl3.c")
set(ADLplug_OPNMIDI_LLE_SOURCES
  "src/chips/nuked_lle/fmopn2.c" "src/chips/nuked_lle/fmopna_2608.c"
  "src/chips/nuked_lle/fmopna_2610.c" "src/chips/nuked_lle/fmopna_2612.c"
  "src/chips/nuked_lle/nopn2.c" "src/chips/nuked_lle/nopn2f.c"
  "src/chips/nuked_lle/nopna.c")

foreach(ADLplug_LIBRARY IN ITEMS ADLMIDI OPNMIDI)
  set(ADLplug_LLE_FILES "")
  foreach(ADLplug_LLE_SOURCE IN LISTS ADLplug_${ADLplug_LIBRARY}_LLE_SOURCES)
    list(APPEND ADLplug_LLE_FILES
      "${PROJECT_SOURCE_DIR}/thirdparty/lib${ADLplug_LIBRARY}/${ADLplug_LLE_SOURCE}")
  endforeach()
  set_property(SOURCE ${ADLplug_LLE_FILES}
    TARGET_DIRECTORY ${ADLplug_LIBRARY}_static APPEND PROPERTY COMPILE_OPTIONS -fwrapv)
endforeach()

# The measurers (sources/*/adl/measurer) run on these cores, and the plugins
# select them by default.
if(ADLplug_CHIP STREQUAL "OPL3" AND NOT USE_DOSBOX_EMULATOR)
  message(FATAL_ERROR "ADLplug needs USE_DOSBOX_EMULATOR: its measurer runs on the DOSBox OPL3 core.")
elseif(ADLplug_CHIP STREQUAL "OPN2" AND NOT USE_MAME_EMULATOR)
  message(FATAL_ERROR "OPNplug needs USE_MAME_EMULATOR: its measurer runs on the MAME YM2612 core.")
endif()

set(CMAKE_POLICY_VERSION_MINIMUM "${_ADLplug_saved_policy_min}")
unset(_ADLplug_saved_policy_min)
