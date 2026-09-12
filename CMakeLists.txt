cmake_minimum_required(VERSION 3.25)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(ADLplug VERSION "1.0.2" LANGUAGES CXX C)
set(PROJECT_VENDOR "Jean Pierre Cimalando")
set(PROJECT_CONTACT "Jean Pierre Cimalando <http://jpcima.sdf1.org/>")
set(PROJECT_HOMEPAGE "https://github.com/jpcima/ADLplug")

enable_language(C)
enable_language(CXX)
include(GNUInstallDirs)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

include(ADLMIDI)

include(SystemOption)
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

include(GNUWarnings)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")

set(CMAKE_C_VISIBILITY_PRESET "hidden")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

enable_gnu_warning("all")
disable_gnu_warning("unknown-warning-option")
disable_gnu_warning("multichar")
# this one is all over JUCE code
disable_gnu_warning("class-memaccess")
# fatal warnings
enable_gnu_warning("error=return-type")

if(ADLplug_ASSERTIONS)
  add_definitions("-UNDEBUG")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  if(CMAKE_COMPILER_IS_GNUCXX)
    # on MinGW JUCE defines a _WINNT_VER which disables some needed definitions
    add_definitions("-DREPLACEFILE_IGNORE_ACL_ERRORS=0x4")
  endif()
endif()

message("Build VST3: ${ADLplug_VST3}")
message("Build LV2: ${ADLplug_LV2}")
message("Build AU: ${ADLplug_AU}")
message("Build AAX: ${ADLplug_AAX}")
message("Build Standalone: ${ADLplug_Standalone}")
message("Enable ASIO: ${ADLplug_ASIO}")

if(ADLplug_CHIP STREQUAL "OPL3")
  set(ADLplug_NAME "ADLplug")
  set(JUCE_PROJECT_DIR "${PROJECT_SOURCE_DIR}/project/adl")
elseif(ADLplug_CHIP STREQUAL "OPN2")
  set(ADLplug_NAME "OPNplug")
  set(JUCE_PROJECT_DIR "${PROJECT_SOURCE_DIR}/project/opn")
else()
  message(FATAL_ERROR "unsupported chip ${ADLplug_CHIP}")
endif()
include("${CMAKE_SOURCE_DIR}/JUCE-targets.cmake")

set(MACOSX_BUNDLE_ICON_FILE "${ADLplug_NAME}.icns")

add_library(ADLplug_core STATIC EXCLUDE_FROM_ALL
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
  "sources/ui/utility/key_maps.cc"
  "sources/pch/ADLplug_core.h")
target_include_directories(ADLplug_core PUBLIC "sources" "${JUCE_PROJECT_DIR}/JuceLibraryCode")
target_link_libraries(ADLplug_core PUBLIC
  juce_core juce_events juce_data_structures
  juce_audio_basics juce_audio_processors
  juce_graphics juce_gui_basics juce_gui_extra
  ADLMIDI_static)
set_property(TARGET ADLplug_core
  PROPERTY POSITION_INDEPENDENT_CODE ON)

if(ADLplug_CHIP STREQUAL "OPL3")
  target_compile_definitions(ADLplug_core PUBLIC "ADLPLUG_OPL3")

  target_sources(ADLplug_core PRIVATE
    "sources/opl3/parameter_block.cc"
    "sources/opl3/adl/chip_settings.cc"
    "sources/opl3/adl/instrument.cc"
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
  target_link_libraries(ADLplug_core PUBLIC wopl)

  target_link_libraries(ADLplug_core PUBLIC ADLMIDI_static)
elseif(ADLplug_CHIP STREQUAL "OPN2")
  target_compile_definitions(ADLplug_core PUBLIC "ADLPLUG_OPN2")

  target_sources(ADLplug_core PRIVATE
    "sources/opn2/parameter_block.cc"
    "sources/opn2/adl/chip_settings.cc"
    "sources/opn2/adl/instrument.cc"
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
  target_link_libraries(ADLplug_core PUBLIC wopn)

  target_link_libraries(ADLplug_core PUBLIC OPNMIDI_static)
endif()

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
target_link_libraries(ADLplug_core PUBLIC ADLplug_fmt)

add_library(simple-ini INTERFACE)
target_include_directories(simple-ini INTERFACE "thirdparty/simpleini")

target_link_libraries(ADLplug_core PUBLIC simple-ini)

if(ADLplug_PCH)
  include(PrecompiledHeader)
  add_precompiled_header(ADLplug_core "sources/pch/ADLplug_core.h"
    FORCEINCLUDE
    SOURCE_CXX "sources/pch/ADLplug_core.cc")
endif()

macro(add_plugin NAME)
  add_library("${NAME}" MODULE "sources/nothing.cc" ${ARGN})
  set_property(TARGET "${NAME}" PROPERTY PREFIX "")
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources("${NAME}" PRIVATE "${JUCE_PROJECT_DIR}/juce/Builds/VisualStudio2017/resources.rc")
  endif()
endmacro()

macro(add_standalone NAME)
  add_executable("${NAME}" WIN32 MACOSX_BUNDLE "sources/nothing.cc" ${ARGN})
  set_property(TARGET "${NAME}" PROPERTY PREFIX "")
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources("${NAME}" PRIVATE "${JUCE_PROJECT_DIR}/juce/Builds/VisualStudio2017/resources.rc")
  endif()
endmacro()

if(ADLplug_VST3)
  if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Windows" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
    message(FATAL_ERROR "VST3 is not supported on ${CMAKE_SYSTEM_NAME}")
  endif()
  add_plugin(ADLplug_VST3)
  target_link_libraries(ADLplug_VST3 PRIVATE juce_audio_plugin_client_VST3 ADLplug_core)
  set_target_properties(ADLplug_VST3 PROPERTIES
    OUTPUT_NAME "${ADLplug_NAME}" LIBRARY_OUTPUT_DIRECTORY "vst3")
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_target_properties(ADLplug_VST3 PROPERTIES
      PREFIX "" SUFFIX "" LIBRARY_OUTPUT_DIRECTORY "vst3/${ADLplug_NAME}.vst3/Contents/MacOS")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/vst3/${ADLplug_NAME}.vst3/Contents/")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/vst3/${ADLplug_NAME}.vst3/Contents/PkgInfo" "BNDL????")
    add_custom_target(ADLplug_VST3_InfoPlist ALL
      "${CMAKE_COMMAND}"
      "-DCONFIGURE_FILE_INPUT=${JUCE_PROJECT_DIR}/juce/Builds/MacOSX/Info-VST3.plist"
      "-DCONFIGURE_FILE_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/vst3/${ADLplug_NAME}.vst3/Contents/Info.plist"
      "-DEXECUTABLE_NAME=${ADLplug_NAME}"
      "-P" "${PROJECT_SOURCE_DIR}/cmake/ConfigureFile.cmake"
      DEPENDS "${JUCE_PROJECT_DIR}/juce/Builds/MacOSX/Info-VST3.plist")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/vst3/${ADLplug_NAME}.vst3/Contents/Resources/")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy
      "${JUCE_PROJECT_DIR}/juce/Builds/MacOSX/RecentFilesMenuTemplate.nib"
      "${CMAKE_CURRENT_BINARY_DIR}/vst3/${ADLplug_NAME}.vst3/Contents/Resources/RecentFilesMenuTemplate.nib")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink "/Library/Audio/Plug-Ins/VST3" "${CMAKE_CURRENT_BINARY_DIR}/vst3/VST3 Plug-Ins")
  endif()
endif()

if(ADLplug_LV2)
  add_plugin(ADLplug_LV2)
  target_link_libraries(ADLplug_LV2 PRIVATE juce_audio_plugin_client_LV2 ADLplug_core)
  set_target_properties(ADLplug_LV2 PROPERTIES
    OUTPUT_NAME "${ADLplug_NAME}" LIBRARY_OUTPUT_DIRECTORY "lv2/${ADLplug_NAME}.lv2")

  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(LV2_UI_CLASS "ui:WindowsUI")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(LV2_UI_CLASS "ui:CocoaUI")
  else()
    set(LV2_UI_CLASS "ui:X11UI")
  endif()
  set(LV2_BINARY_SUFFIX "${CMAKE_SHARED_MODULE_SUFFIX}")

  configure_file(
    "${PROJECT_SOURCE_DIR}/resources/lv2/${ADLplug_NAME}.lv2/manifest.ttl.in"
    "${PROJECT_BINARY_DIR}/lv2build/manifest.ttl"
    @ONLY)

  set(ADLplug_LV2_MANIFESTS
    "resources/lv2/${ADLplug_NAME}.lv2/${ADLplug_NAME}.ttl"
    "${PROJECT_BINARY_DIR}/lv2build/manifest.ttl")
  add_custom_target(ADLplug_LV2_manifests_copy ALL
    COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "${CMAKE_CURRENT_BINARY_DIR}/lv2/${ADLplug_NAME}.lv2/"
    COMMAND "${CMAKE_COMMAND}" "-E" "copy" ${ADLplug_LV2_MANIFESTS} "${CMAKE_CURRENT_BINARY_DIR}/lv2/${ADLplug_NAME}.lv2/"
    DEPENDS ${ADLplug_LV2_MANIFESTS}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "Windows"))
    install(TARGETS ADLplug_LV2 DESTINATION "${CMAKE_INSTALL_LIBDIR}/lv2/${ADLplug_NAME}.lv2")
    install(FILES ${ADLplug_LV2_MANIFESTS} DESTINATION "${CMAKE_INSTALL_LIBDIR}/lv2/${ADLplug_NAME}.lv2")
  endif()
endif()

if(ADLplug_AU)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(FATAL_ERROR "AU is not supported on ${CMAKE_SYSTEM_NAME}")
  endif()
  add_plugin(ADLplug_AU)
  target_link_libraries(ADLplug_AU PRIVATE juce_audio_plugin_client_AU ADLplug_core)
  set_target_properties(ADLplug_AU PROPERTIES
    OUTPUT_NAME "${ADLplug_NAME}" PREFIX "" SUFFIX "" LIBRARY_OUTPUT_DIRECTORY "au/${ADLplug_NAME}.component/Contents/MacOS")
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/au/${ADLplug_NAME}.component/Contents/")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/au/${ADLplug_NAME}.component/Contents/PkgInfo" "BNDL????")
  add_custom_target(ADLplug_AU_InfoPlist ALL
    "${CMAKE_COMMAND}"
    "-DCONFIGURE_FILE_INPUT=${JUCE_PROJECT_DIR}/juce/Builds/MacOSX/Info-AU.plist"
    "-DCONFIGURE_FILE_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/au/${ADLplug_NAME}.component/Contents/Info.plist"
    "-DEXECUTABLE_NAME=${ADLplug_NAME}"
    "-P" "${PROJECT_SOURCE_DIR}/cmake/ConfigureFile.cmake"
    DEPENDS "${JUCE_PROJECT_DIR}/juce/Builds/MacOSX/Info-AU.plist")

  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/au/${ADLplug_NAME}.component/Contents/Resources/")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E copy
    "${JUCE_PROJECT_DIR}/juce/Builds/MacOSX/RecentFilesMenuTemplate.nib"
    "${CMAKE_CURRENT_BINARY_DIR}/au/${ADLplug_NAME}.component/Contents/Resources/RecentFilesMenuTemplate.nib")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink "/Library/Audio/Plug-Ins/Components" "${CMAKE_CURRENT_BINARY_DIR}/au/Audio Units")
endif()

if(ADLplug_Standalone)
  add_standalone(ADLplug_Standalone "sources/nothing.cc")
  target_link_libraries(ADLplug_Standalone PRIVATE juce_audio_plugin_client_Standalone juce_audio_devices juce_audio_formats juce_audio_utils ADLplug_core)
  set_target_properties(ADLplug_Standalone PROPERTIES
    OUTPUT_NAME "${ADLplug_NAME}" RUNTIME_OUTPUT_DIRECTORY "standalone")
  install(TARGETS ADLplug_Standalone DESTINATION "${CMAKE_INSTALL_BINDIR}")
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(ADLplug_Standalone PRIVATE
      "resources/application/${ADLplug_NAME}.rc")
  endif()
endif()

if(ADLplug_Standalone)
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
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_custom_target(ADLplug_Standalone_icon_copy ALL
      "${CMAKE_COMMAND}" -E copy
      "${PROJECT_SOURCE_DIR}/resources/application/${ADLplug_NAME}.icns"
      "${CMAKE_CURRENT_BINARY_DIR}/standalone/${ADLplug_NAME}.app/Contents/Resources/${ADLplug_NAME}.icns"
      DEPENDS "${PROJECT_SOURCE_DIR}/resources/application/${ADLplug_NAME}.icns")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/standalone/${ADLplug_NAME}.app/Contents/Resources/")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy
      "${JUCE_PROJECT_DIR}/juce/Builds/MacOSX/RecentFilesMenuTemplate.nib"
      "${CMAKE_CURRENT_BINARY_DIR}/standalone/${ADLplug_NAME}.app/Contents/Resources/RecentFilesMenuTemplate.nib")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink "/Applications" "${CMAKE_CURRENT_BINARY_DIR}/standalone/Applications")
  endif()
endif()

include("CPackLists.txt")
