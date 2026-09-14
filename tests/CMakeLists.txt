# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
# The tests, registered with CTest when ADLplug_BUILD_TESTS is on:
#  - unit.<name>: each ADLPLUG_TEST in unit/, run by ADLplug_unit_tests;
#  - bankgen.greyzone: tools/bankgen with the banks of the grey zone;
#  - format.*: the compatibility that the VST3 plugin declares, and pluginval
#    and lv2lint validating the plugins, when they are installed;
#  - render.*: tools/render loading this build's VST3 plugin, playing the fixed
#    sequence and checking what comes out (these need ADLplug_BUILD_TOOLS).

# --------------------------------------------------------------------------
# Unit tests

if(ADLplug_CHIP STREQUAL "OPL3")
  set(ADLplug_CHIP_DIRECTORY "opl3")
else()
  set(ADLplug_CHIP_DIRECTORY "opn2")
endif()

set(ADLplug_UNIT_TEST_SOURCES
  "unit/main.cc"
  "unit/bank_tests.cc"
  "unit/fifo_tests.cc"
  "unit/text_tests.cc"
  "unit/utility_tests.cc")
if(ADLplug_CHIP STREQUAL "OPL3")
  list(APPEND ADLplug_UNIT_TEST_SOURCES "unit/opl3_tests.cc")
endif()

# The plugin's own sources under test, compiled again here against juce_core
# alone.
set(ADLplug_UNIT_TESTED_SOURCES
  "${PROJECT_SOURCE_DIR}/sources/resources.c"
  "${PROJECT_SOURCE_DIR}/sources/utility/pak.cc"
  "${PROJECT_SOURCE_DIR}/sources/utility/simple_fifo.cc"
  "${PROJECT_SOURCE_DIR}/sources/${ADLplug_CHIP_DIRECTORY}/adl/instrument.cc"
  "${PROJECT_SOURCE_DIR}/sources/${ADLplug_CHIP_DIRECTORY}/adl/instrument_properties.cc")
if(ADLplug_CHIP STREQUAL "OPL3")
  # The player, which sets the MT-32 defaults inside libADLMIDI.
  list(APPEND ADLplug_UNIT_TESTED_SOURCES "${PROJECT_SOURCE_DIR}/sources/opl3/adl/player.cc")
endif()

juce_add_console_app(ADLplug_unit_tests)
juce_generate_juce_header(ADLplug_unit_tests)
adlplug_set_language_standard(ADLplug_unit_tests)
get_target_property(ADLplug_UNIT_TESTS_LIBRARY_CODE ADLplug_unit_tests JUCE_GENERATED_SOURCES_DIRECTORY)
target_include_directories(ADLplug_unit_tests SYSTEM PRIVATE "${ADLplug_UNIT_TESTS_LIBRARY_CODE}")
target_include_directories(ADLplug_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/sources")
target_sources(ADLplug_unit_tests PRIVATE ${ADLplug_UNIT_TEST_SOURCES} ${ADLplug_UNIT_TESTED_SOURCES})
target_compile_definitions(ADLplug_unit_tests PRIVATE
  "ADLPLUG_${ADLplug_CHIP}"
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0)
if(ADLplug_CHIP STREQUAL "OPL3")
  target_link_libraries(ADLplug_unit_tests PRIVATE wopl ADLMIDI_static)
  target_include_directories(ADLplug_unit_tests SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/thirdparty/libADLMIDI/src")
else()
  target_link_libraries(ADLplug_unit_tests PRIVATE wopn OPNMIDI_static)
endif()
target_link_libraries(ADLplug_unit_tests PRIVATE
  juce::juce_core
  juce::juce_recommended_config_flags)
adlplug_own_sources(${ADLplug_UNIT_TEST_SOURCES} ${ADLplug_UNIT_TESTED_SOURCES})
# resources.c embeds the pack of banks generated for the plugin.
adlplug_embed_banks("${PROJECT_SOURCE_DIR}/sources/resources.c")
add_dependencies(ADLplug_unit_tests ADLplug_banks)

foreach(source IN LISTS ADLplug_UNIT_TEST_SOURCES)
  file(STRINGS "${source}" declarations REGEX "^ADLPLUG_TEST\\([A-Za-z0-9_]+\\)")
  foreach(declaration IN LISTS declarations)
    string(REGEX REPLACE "^ADLPLUG_TEST\\(([A-Za-z0-9_]+)\\).*$" "\\1" name "${declaration}")
    add_test(NAME "unit.${name}" COMMAND ADLplug_unit_tests "${name}")
  endforeach()
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ADLplug_UNIT_TEST_SOURCES})

# The generator with the banks of the grey zone too, into a pack of its own.
# The build leaves them out by default, and they need what the other banks do
# not, such as the conversion of GYB and GEMS banks.
add_test(NAME bankgen.greyzone
  COMMAND ADLplug_bankgen ${ADLplug_BANKGEN_ARGS} --greyzone
    --pak "${CMAKE_CURRENT_BINARY_DIR}/greyzone/banks.pak"
    --notices "${CMAKE_CURRENT_BINARY_DIR}/greyzone/${ADLplug_NAME}-banks.txt")

# --------------------------------------------------------------------------
# Format tests

if(NOT TARGET ADLplug_VST3)
  return()
endif()

set(plugin "$<GENEX_EVAL:$<TARGET_PROPERTY:ADLplug_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")

# pluginval (https://github.com/Tracktion/pluginval) loads the VST3 plugin as a
# host does and runs its tests, at level 5, the one it recommends for
# compatibility with hosts.
find_program(ADLplug_PLUGINVAL pluginval DOC "pluginval, for the format tests")
if(ADLplug_PLUGINVAL)
  add_test(NAME format.vst3.pluginval
    COMMAND "${ADLplug_PLUGINVAL}" --strictness-level 5 --timeout-ms 300000
      --output-dir "${CMAKE_CURRENT_BINARY_DIR}/pluginval" --validate "${plugin}")
  set_tests_properties(format.vst3.pluginval PROPERTIES TIMEOUT 1800 LABELS "format;gui")
endif()

# lv2lint (https://git.open-music-kontrollers.ch/~hp/lv2lint) checks the LV2
# bundle against the specification.
find_program(ADLplug_LV2LINT lv2lint DOC "lv2lint, for the format tests")
if(ADLplug_LV2LINT AND TARGET ADLplug_LV2)
  add_test(NAME format.lv2.lv2lint
    COMMAND "${ADLplug_LV2LINT}"
      -I "$<GENEX_EVAL:$<TARGET_PROPERTY:ADLplug_LV2,JUCE_PLUGIN_ARTEFACT_FILE>>/" "${ADLplug_LV2URI}")
  set_tests_properties(format.lv2.lv2lint PROPERTIES TIMEOUT 600 LABELS format)
endif()

# The moduleinfo.json of the VST3 plugin lists the classes of upstream ADLplug
# among those that the plugin replaces, by their IDs as hosts read them (see
# JUCE_VST3_COMPATIBLE_CLASSES in CMakeLists.txt). JUCE writes the file when it
# builds the plugin, except on BSD systems.
if(NOT CMAKE_SYSTEM_NAME MATCHES "BSD$")
  add_test(NAME format.vst3.compatibility
    COMMAND "${CMAKE_COMMAND}" "-Dmoduleinfo=${plugin}/Contents/Resources/moduleinfo.json"
      "-Dnew=${ADLplug_VST3_CLASS}" "-Dold=${ADLplug_UPSTREAM_VST2_CLASS},${ADLplug_UPSTREAM_VST3_CLASS}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/format/vst3_compatibility.cmake")
  set_tests_properties(format.vst3.compatibility PROPERTIES TIMEOUT 60 LABELS format)
endif()

# --------------------------------------------------------------------------
# Render tests

if(NOT ADLplug_BUILD_TOOLS)
  return()
endif()

# References are kept per system and instruction set: where the target has
# FMA instructions, Clang fuses multiply-adds, and the systems' maths libraries
# differ (plan D42). Debug and Release builds share a key.
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(ADLplug_RENDER_SYSTEM "macos")
else()
  string(TOLOWER "${CMAKE_SYSTEM_NAME}" ADLplug_RENDER_SYSTEM)
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
  set(ADLplug_RENDER_KEY "${ADLplug_RENDER_SYSTEM}-x86_64-${ADLplug_ARCH}")
else()
  string(TOLOWER "${ADLplug_RENDER_SYSTEM}-${CMAKE_SYSTEM_PROCESSOR}" ADLplug_RENDER_KEY)
endif()

set(ADLplug_RENDER_REFERENCES "${CMAKE_CURRENT_SOURCE_DIR}/render/references.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ADLplug_RENDER_REFERENCES}")
file(STRINGS "${ADLplug_RENDER_REFERENCES}" ADLplug_RENDER_REFERENCE
  REGEX "^${ADLplug_RENDER_KEY} ${ADLplug_NAME} ")
list(LENGTH ADLplug_RENDER_REFERENCE ADLplug_RENDER_REFERENCE_COUNT)
if(ADLplug_RENDER_REFERENCE_COUNT GREATER 1)
  message(FATAL_ERROR "tests/render/references.txt has more than one line for ${ADLplug_RENDER_KEY} ${ADLplug_NAME}")
endif()

set(render_out "${CMAKE_CURRENT_BINARY_DIR}/render")
file(MAKE_DIRECTORY "${render_out}")
# ci/test.sh reads these to print a line for references.txt.
file(WRITE "${render_out}/key" "${ADLplug_RENDER_KEY} ${ADLplug_NAME}\n")
file(WRITE "${render_out}/reference" "${ADLplug_RENDER_REFERENCE}")

set(sequence 20 5000)   # seconds, and milliseconds of warm-up
if(ADLplug_CHIP STREQUAL "OPL3")
  set(default_emulator "DOSBox 0.74-r4111 OPL3")
else()
  set(default_emulator "MAME YM2612")
endif()

# The render the other render tests are compared with. It checks only that the
# default emulator plays; the comparison with references.txt is a test of its
# own, so a changed reference does not hide the other results.
add_test(NAME render
  COMMAND ADLplug_render "${plugin}" "${render_out}/render.f32" ${sequence}
    --state "${render_out}/render.state" --save-hashes "${render_out}/render.hashes"
    --require-emulator "${default_emulator}" --require-sound)
set_tests_properties(render PROPERTIES FIXTURES_SETUP render TIMEOUT 900)

if(ADLplug_RENDER_REFERENCE)
  string(REPLACE " " ";" reference_fields "${ADLplug_RENDER_REFERENCE}")
  list(GET reference_fields 2 reference_output)
  list(GET reference_fields 3 reference_state)
  add_test(NAME render.reference
    COMMAND ADLplug_render --compare "${render_out}/render.hashes" "${reference_output}" "${reference_state}")
else()
  add_test(NAME render.reference
    COMMAND ADLplug_render --compare "${render_out}/render.hashes" - -)
  set_tests_properties(render.reference PROPERTIES DISABLED TRUE)
endif()
set_tests_properties(render.reference PROPERTIES FIXTURES_REQUIRED render)

# Opening the editor after rendering changes neither the output nor the state.
add_test(NAME render.editor
  COMMAND ADLplug_render "${plugin}" "${render_out}/editor.f32" ${sequence} --editor
    --expect-output-of "${render_out}/render.hashes" --expect-state-of "${render_out}/render.hashes")
# The state saved after the warm-up comes back the same after being restored.
add_test(NAME render.restore
  COMMAND ADLplug_render "${plugin}" "${render_out}/restore.f32" ${sequence}
    --restore "${render_out}/render.state" --expect-state-of "${render_out}/render.hashes")
set_tests_properties(render.editor render.restore PROPERTIES FIXTURES_REQUIRED render TIMEOUT 900)
set_tests_properties(render.editor PROPERTIES LABELS gui)

# Parameters changed while the plugin is released keep their values when it is
# prepared again, and so does the state: Audio Unit hosts expect it, and auval
# checks it on macOS.
add_test(NAME render.prepare_again
  COMMAND ADLplug_render "${plugin}" "${render_out}/prepare_again.f32" 2 1000 --prepare-again)
set_tests_properties(render.prepare_again PROPERTIES TIMEOUT 300)

# The parameters of upstream ADLplug 1 come first, in their order and with their
# IDs: projects saved with the plugins of upstream refer to them, and the VST3
# plugin maps them for hosts that replace those plugins with it (plan D02).
add_test(NAME render.upstream_parameters
  COMMAND ADLplug_render "${plugin}" "${render_out}/upstream_parameters.f32" 1 0
    --upstream-parameters "${ADLplug_CHIP}")
set_tests_properties(render.upstream_parameters PROPERTIES TIMEOUT 300)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  # Only Windows can capture the hosted editor so far (tools/render).
  add_test(NAME render.snapshot
    COMMAND ADLplug_render "${plugin}" "${render_out}/snapshot.f32" 2 1000
      --snapshot "${render_out}/${ADLplug_NAME}.png")
  set_tests_properties(render.snapshot PROPERTIES TIMEOUT 300 LABELS gui)
endif()

# Every emulator core plays, and the plugin names the one asked for. Release
# only: the low-level cores are too slow in Debug builds. Each entry is the
# emulator number, its name and the build option that includes it.
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  if(ADLplug_CHIP STREQUAL "OPL3")
    set(cores
      "0|Nuked OPL3 (v 1.8)|USE_NUKED_EMULATOR"
      "1|Nuked OPL3 Fast (by tgies)|USE_NUKED_EMULATOR"
      "2|DOSBox 0.74-r4111 OPL3|USE_DOSBOX_EMULATOR"
      "3|Opal OPL3|USE_OPAL_EMULATOR"
      "4|Java 1.0.6 OPL3|USE_JAVA_EMULATOR"
      "5|ESFMu|USE_ESFMU_EMULATOR"
      "6|MAME OPL2|USE_MAME_EMULATOR"
      "7|YMFM OPL2|USE_YMFM_EMULATOR"
      "8|YMFM OPL3|USE_YMFM_EMULATOR"
      "9|YM3812-LLE OPL2|USE_NUKED_OPL2_LLE_EMULATOR"
      "10|YMF262-LLE OPL3|USE_NUKED_OPL3_LLE_EMULATOR"
      "11|Nuked OPL2 Lite|USE_NUKED_EMULATOR"
      "12|Nuked CQM|USE_NUKED_EMULATOR"
      "13|DOSBox 0.74-r4111 OPL2|USE_DOSBOX_EMULATOR")
  else()
    set(cores
      "0|MAME YM2612|USE_MAME_EMULATOR"
      "1|Nuked OPN2 (3438)|USE_NUKED_EMULATOR"
      "2|GENS/GS II OPN2|USE_GENS_EMULATOR"
      "3|YMFM OPN2|USE_YMFM_EMULATOR"
      "4|Neko Project II Kai OPNA|USE_NP2_EMULATOR"
      "5|MAME YM2608|USE_MAME_2608_EMULATOR"
      "6|YMFM OPNA|USE_YMFM_EMULATOR"
      "8|Nuked OPN2 (2612)|USE_NUKED_EMULATOR"
      "9|YM2612-LLE OPN2|USE_NUKED_OPN2_LLE_EMULATOR"
      "10|YM2608-LLE OPNA|USE_NUKED_OPNA_LLE_EMULATOR"
      "11|YM3438-LLE OPN2|USE_NUKED_OPN2_LLE_EMULATOR"
      "12|YMF276-LLE OPN2|USE_NUKED_OPN2_LLE_EMULATOR")
  endif()
  foreach(core IN LISTS cores)
    string(REPLACE "|" ";" core_fields "${core}")
    list(GET core_fields 0 core_index)
    list(GET core_fields 1 core_name)
    list(GET core_fields 2 core_option)
    if(${core_option})
      add_test(NAME render.core.${core_index}
        COMMAND ADLplug_render "${plugin}" "${render_out}/core${core_index}.f32" 2 1000
          --emulator ${core_index} --require-emulator "${core_name}" --require-sound)
      set_tests_properties(render.core.${core_index} PROPERTIES TIMEOUT 600 LABELS core)
    endif()
  endforeach()
endif()
