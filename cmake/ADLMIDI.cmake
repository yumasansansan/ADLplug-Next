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

# For GCC-style compilers both libraries rewrite their Release flags: -O3 is
# replaced with -O2, and -fno-omit-frame-pointer (a debugging/profiling aid) is
# forced on. Release is the distribution build and should be fully optimised
# with no debugging aids, so put both back for Release only. Target options
# follow CMAKE_<LANG>_FLAGS_RELEASE on the command line, so these take effect.
# RelWithDebInfo keeps the libraries' own choices.
set(ADLplug_UPSTREAM_RELEASE_OPTS "")
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
  set(ADLplug_UPSTREAM_RELEASE_OPTS $<$<CONFIG:Release>:-O3> $<$<CONFIG:Release>:-fomit-frame-pointer>)
endif()

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
target_compile_options(ADLMIDI_static PRIVATE ${ADLplug_UPSTREAM_RELEASE_OPTS})

set(libOPNMIDI_STATIC ON CACHE BOOL "" FORCE)
set(libOPNMIDI_SHARED OFF CACHE BOOL "" FORCE)
set(USE_VGM_FILE_DUMPER OFF CACHE BOOL "" FORCE)
add_subdirectory("${PROJECT_SOURCE_DIR}/thirdparty/libOPNMIDI" EXCLUDE_FROM_ALL SYSTEM)
target_compile_definitions(OPNMIDI_static PRIVATE "OPNMIDI_EXPORT=" ${ADLplug_UPSTREAM_CRT_DEFS})
target_compile_definitions(OPNMIDI_static PUBLIC "OPNMIDI_UNSTABLE_API=")
target_compile_options(OPNMIDI_static PRIVATE ${ADLplug_UPSTREAM_RELEASE_OPTS})

# The measurers (sources/*/adl/measurer) run on these cores, and the plugins
# select them by default.
if(ADLplug_CHIP STREQUAL "OPL3" AND NOT USE_DOSBOX_EMULATOR)
  message(FATAL_ERROR "ADLplug needs USE_DOSBOX_EMULATOR: its measurer runs on the DOSBox OPL3 core.")
elseif(ADLplug_CHIP STREQUAL "OPN2" AND NOT USE_MAME_EMULATOR)
  message(FATAL_ERROR "OPNplug needs USE_MAME_EMULATOR: its measurer runs on the MAME YM2612 core.")
endif()

set(CMAKE_POLICY_VERSION_MINIMUM "${_ADLplug_saved_policy_min}")
unset(_ADLplug_saved_policy_min)
