# libADLMIDI / libOPNMIDI
#
# Both libraries declare `cmake_minimum_required(VERSION 3.2...4.0)`. CMake 4.x
# refuses a <min> below 3.5, so raise the floor for the duration of the two
# add_subdirectory() calls and restore it afterwards.

set(_ADLplug_saved_policy_min "${CMAKE_POLICY_VERSION_MINIMUM}")
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

# Real-time plugin use: no file loading, no sequencer, no embedded banks
# (ADLplug ships its own bank set in sources/resources.cc).
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

set(CMAKE_POLICY_VERSION_MINIMUM "${_ADLplug_saved_policy_min}")
unset(_ADLplug_saved_policy_min)
