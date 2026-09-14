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

cmake_minimum_required(VERSION 3.25)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# macOS builds are for Apple Silicon and macOS 26 or later. Both have to be set
# before project(). Without a deployment target, the plugins would require the
# macOS version of whichever SDK built them.
set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Architectures to build for on macOS")

project(ADLplug VERSION "1.0.2" LANGUAGES C CXX)
set(PROJECT_VENDOR "Jean Pierre Cimalando")
set(PROJECT_CONTACT "Jean Pierre Cimalando <http://jpcima.sdf1.org/>")
set(PROJECT_HOMEPAGE "https://github.com/jpcima/ADLplug")

# The warning policy, the language standards and LTO below are written for
# Clang; GCC and MSVC are not supported compilers.
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR
    "ADLplug is built with Clang, not ${CMAKE_CXX_COMPILER_ID}. Configure with "
    "CMAKE_C_COMPILER=clang and CMAKE_CXX_COMPILER=clang++, as the presets do.")
endif()

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
option(ADLplug_LTO "Use link-time optimisation (ThinLTO with Clang) in Release builds" ON)
option(ADLplug_BUILD_TOOLS "Build developer tools (tools/render: offline VST3 renderer)" OFF)
option(ADLplug_BUILD_TESTS "Build the tests and register them with CTest (render tests need ADLplug_BUILD_TOOLS)" OFF)

set(ADLplug_CHIP "OPL3" CACHE STRING "Kind of synthesizer chip (OPL3, OPN2)")
set(ADLplug_ARCH "baseline" CACHE STRING "Instruction set of x86-64 builds (baseline, avx2)")

# Applied per-target rather than globally: these variables are inherited by
# add_subdirectory() dependencies, and libADLMIDI/libOPNMIDI build their own
# sources as C90 + GNU extensions.
function(adlplug_set_language_standard target)
  set_target_properties("${target}" PROPERTIES
    C_STANDARD 23 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endfunction()

set(CMAKE_C_VISIBILITY_PRESET "hidden")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if(ADLplug_ASSERTIONS)
  add_definitions("-UNDEBUG")
endif()

# --------------------------------------------------------------------------
# Link-time optimisation
#
# Enabled before any dependency is added, so CMAKE_INTERPROCEDURAL_OPTIMIZATION_
# RELEASE initialises every target below -- JUCE, libADLMIDI and libOPNMIDI
# included -- and the synthesis code can be inlined across library boundaries.
# With Clang, CMake's IPO support means ThinLTO (-flto=thin).

if(ADLplug_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ADLplug_LTO_SUPPORTED OUTPUT ADLplug_LTO_ERROR LANGUAGES C CXX)
  if(ADLplug_LTO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  else()
    message(WARNING "ADLplug_LTO is on but LTO is not supported here: ${ADLplug_LTO_ERROR}")
  endif()
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
# Chip identity
#
# The plugin codes, LV2 URIs and bundle IDs are deliberately unchanged from
# upstream ADLplug so that existing host projects keep resolving to this build.

if(ADLplug_CHIP STREQUAL "OPL3")
  set(ADLplug_NAME "ADLplug")
  set(ADLplug_PLUGIN_CODE "ADLM")
  set(ADLplug_LV2URI "https://github.com/jpcima/ADLplug")
  set(ADLplug_BUNDLE_ID "jpcima.ADLplug")
elseif(ADLplug_CHIP STREQUAL "OPN2")
  set(ADLplug_NAME "OPNplug")
  set(ADLplug_PLUGIN_CODE "OPNM")
  set(ADLplug_LV2URI "https://github.com/jpcima/OPNplug")
  set(ADLplug_BUNDLE_ID "jpcima.OPNplug")
else()
  message(FATAL_ERROR "unsupported chip ${ADLplug_CHIP}")
endif()

# --------------------------------------------------------------------------
# Dependencies

include(ADLMIDI)
add_subdirectory("thirdparty/JUCE" EXCLUDE_FROM_ALL SYSTEM)

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
  COMPANY_NAME "${PROJECT_VENDOR}"
  COMPANY_WEBSITE "${PROJECT_HOMEPAGE}"
  BUNDLE_ID "${ADLplug_BUNDLE_ID}"
  PLUGIN_MANUFACTURER_CODE "JPCm"
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
  ICON_BIG "resources/application/${ADLplug_NAME}-96.png"
  ICON_SMALL "resources/application/${ADLplug_NAME}-32.png")

juce_generate_juce_header(ADLplug)
# JuceHeader.h is generated by JUCE, so like the JUCE headers it includes it
# is not held to the warning flags of ADLplug's own code.
get_target_property(ADLplug_JUCE_LIBRARY_CODE ADLplug JUCE_GENERATED_SOURCES_DIRECTORY)
target_include_directories(ADLplug SYSTEM PRIVATE "${ADLplug_JUCE_LIBRARY_CODE}")
adlplug_set_language_standard(ADLplug)

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

if(ADLplug_ASIO)
  # JUCE bundles the ASIO SDK headers (2025, dual Steinberg/GPLv3).
  target_compile_definitions(ADLplug PUBLIC JUCE_ASIO=1)
endif()

target_link_libraries(ADLplug PUBLIC
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

# LTO comes from ADLplug_LTO above rather than juce_recommended_lto_flags,
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
      $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
      $<$<COMPILE_LANGUAGE:CXX>:-Wzero-as-null-pointer-constant>
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

# Gives source files the settings above. Source file properties belong to the
# directory that sets them, so tests/ calls this again for what it compiles.
function(adlplug_own_sources)
  if(ADLplug_OWN_COMPILE_DEFINITIONS)
    set_source_files_properties(${ARGN} PROPERTIES COMPILE_DEFINITIONS "${ADLplug_OWN_COMPILE_DEFINITIONS}")
  endif()
  if(ADLplug_WARNING_FLAGS)
    set_source_files_properties(${ARGN} PROPERTIES COMPILE_OPTIONS "${ADLplug_WARNING_FLAGS}")
  endif()
endfunction()

adlplug_own_sources(${ADLplug_OWN_SOURCES})

# --------------------------------------------------------------------------
# Tests

if(ADLplug_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# --------------------------------------------------------------------------
# Installation (Linux/BSD; the other platforms install through their bundles)

if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Windows" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
  if(ADLplug_VST3)
    install(DIRECTORY "$<TARGET_FILE_DIR:ADLplug_VST3>/../../../${ADLplug_NAME}.vst3"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/vst3")
  endif()
  if(ADLplug_LV2)
    install(DIRECTORY "$<TARGET_FILE_DIR:ADLplug_LV2>/../../../${ADLplug_NAME}.lv2"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/lv2")
  endif()
  if(ADLplug_Standalone)
    install(PROGRAMS "$<TARGET_FILE:ADLplug_Standalone>"
      DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "resources/application/${ADLplug_NAME}.desktop"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "resources/application/${ADLplug_NAME}-32.png"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/pixmaps"
      RENAME "${ADLplug_NAME}.png")
    foreach(size 32;96)
      install(FILES "resources/application/${ADLplug_NAME}-${size}.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps"
        RENAME "${ADLplug_NAME}.png")
    endforeach()
  endif()
endif()

include("CPackLists.txt")
