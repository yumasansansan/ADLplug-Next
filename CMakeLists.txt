cmake_minimum_required(VERSION 3.25)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(ADLplug VERSION "1.0.2" LANGUAGES C CXX)
set(PROJECT_VENDOR "Jean Pierre Cimalando")
set(PROJECT_CONTACT "Jean Pierre Cimalando <http://jpcima.sdf1.org/>")
set(PROJECT_HOMEPAGE "https://github.com/jpcima/ADLplug")

include(GNUInstallDirs)
include(SystemOption)
include(GNUWarnings)

# --------------------------------------------------------------------------
# Options

system_option(ADLplug_VST3 "Build VST3" ON)
system_option(ADLplug_LV2 "Build LV2" ON)
system_option(ADLplug_AU "Build AU" OFF "Darwin" ON)
system_option(ADLplug_AAX "Build AAX" OFF "Windows" ON "Darwin" ON)
system_option(ADLplug_Standalone "Build Standalone" ON)
system_option(ADLplug_ASIO "Enable ASIO support (Windows, GPLv3)" OFF "Windows" ON)

option(ADLplug_PCH "Enable precompiled headers" OFF)
option(ADLplug_ASSERTIONS "Enable assertions" OFF)
option(ADLplug_USE_SYSTEM_FMT "Use fmt libraries from system" OFF)

set(ADLplug_CHIP "OPL3" CACHE STRING "Kind of synthesizer chip (OPL3, OPN2)")

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_VISIBILITY_PRESET "hidden")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

enable_gnu_warning("all")
disable_gnu_warning("unknown-warning-option")
disable_gnu_warning("multichar")
enable_gnu_warning("error=return-type")

if(ADLplug_ASSERTIONS)
  add_definitions("-UNDEBUG")
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
add_subdirectory("thirdparty/JUCE" EXCLUDE_FROM_ALL)

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
  "sources/resources.cc"
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

target_compile_definitions(ADLplug PUBLIC
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0
  JUCE_STRICT_REFCOUNTEDPOINTER=1
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_USE_FLAC=0
  JUCE_USE_OGGVORBIS=0
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

target_link_libraries(ADLplug PRIVATE
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags)

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
    "sources/opl3/adl/measurer/chips/dosbox/dbopl.cpp"
    "sources/opl3/adl/measurer/chips/dosbox_opl3.cpp"
    "sources/opl3/ui/operator_editor.cc"
    "sources/opl3/ui/about_component.cc"
    "sources/opl3/ui/main_component.cc"
    "sources/opl3/ui/components/algorithm_help.cc"
    "sources/opl3/ui/components/opl3_waves.cc")

  add_library(wopl STATIC "thirdparty/libADLMIDI/src/wopl/wopl_file.c")
  target_include_directories(wopl PUBLIC "thirdparty/libADLMIDI/src")
  set_property(TARGET wopl PROPERTY POSITION_INDEPENDENT_CODE ON)

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
    "sources/opn2/adl/measurer/chips/mame/mame_ym2612fm.cpp"
    "sources/opn2/adl/measurer/chips/mame_opn2.cpp"
    "sources/opn2/ui/operator_editor.cc"
    "sources/opn2/ui/about_component.cc"
    "sources/opn2/ui/main_component.cc"
    "sources/opn2/ui/components/algorithm_help.cc"
    "sources/opn2/ui/components/ssgeg_waves.cc")

  add_library(wopn STATIC "thirdparty/libOPNMIDI/src/wopn/wopn_file.c")
  target_include_directories(wopn PUBLIC "thirdparty/libOPNMIDI/src")
  set_property(TARGET wopn PROPERTY POSITION_INDEPENDENT_CODE ON)

  target_link_libraries(ADLplug PUBLIC wopn OPNMIDI_static)
endif()

# --------------------------------------------------------------------------
# fmt / simpleini

if(ADLplug_USE_SYSTEM_FMT)
  find_package(fmt 11.0.0)
  if(NOT fmt_FOUND)
    message(FATAL_ERROR "Unable to find fmt >= 11 on system.")
  endif()
  add_library(ADLplug_fmt INTERFACE)
  target_link_libraries(ADLplug_fmt INTERFACE fmt)
else()
  add_library(ADLplug_fmt STATIC
    "thirdparty/fmt/src/format.cc"
    "thirdparty/fmt/src/os.cc")
  target_include_directories(ADLplug_fmt PUBLIC "thirdparty/fmt/include")
  set_property(TARGET ADLplug_fmt PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

add_library(simple-ini INTERFACE)
target_include_directories(simple-ini INTERFACE "thirdparty/simpleini")

target_link_libraries(ADLplug PUBLIC ADLplug_fmt simple-ini)

if(ADLplug_PCH)
  target_precompile_headers(ADLplug PRIVATE "sources/pch/ADLplug_core.h")
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
