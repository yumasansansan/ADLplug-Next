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
  "unit/chip_settings_tests.cc"
  "unit/fifo_tests.cc"
  "unit/player_tests.cc"
  "unit/text_tests.cc"
  "unit/utility_tests.cc")
if(ADLplug_CHIP STREQUAL "OPL3")
  list(APPEND ADLplug_UNIT_TEST_SOURCES "unit/opl3_tests.cc")
endif()

# The plugin's own sources under test, compiled again here against JUCE's core
# and graphics modules. The messages (messages.h) need the graphics module,
# for the icons of the chip settings.
set(ADLplug_UNIT_TESTED_SOURCES
  "${PROJECT_SOURCE_DIR}/sources/resources.c"
  "${PROJECT_SOURCE_DIR}/sources/messages.cc"
  "${PROJECT_SOURCE_DIR}/sources/utility/pak.cc"
  "${PROJECT_SOURCE_DIR}/sources/utility/simple_fifo.cc"
  # The chip settings, which a project holds as properties, and the icons of the
  # emulators that they carry (ui/utility/image.cc).
  "${PROJECT_SOURCE_DIR}/sources/${ADLplug_CHIP_DIRECTORY}/adl/chip_settings.cc"
  "${PROJECT_SOURCE_DIR}/sources/ui/utility/image.cc"
  "${PROJECT_SOURCE_DIR}/sources/${ADLplug_CHIP_DIRECTORY}/adl/instrument.cc"
  "${PROJECT_SOURCE_DIR}/sources/${ADLplug_CHIP_DIRECTORY}/adl/instrument_properties.cc"
  # The player: it hands the System Exclusive messages to the library and, for
  # OPL3, sets the MT-32 defaults inside it.
  "${PROJECT_SOURCE_DIR}/sources/${ADLplug_CHIP_DIRECTORY}/adl/player.cc")

# Under the sanitizers, the tests get their environment from the top
# (adlplug_sanitizer_test_environment in CMakeLists.txt), and a test shows that
# the checks which say nothing when they find nothing are really looking: the
# programs below leak and race on purpose, and each test passes only when that
# is reported. Leaks: on Linux with the address sanitizer only, where leak
# detection is asked for. Races: wherever the thread sanitizer is built with.
# Values never written: wherever the memory sanitizer is, which is a build of the
# fuzz targets alone, so that one is run by the job that builds them rather than
# by a whole ctest (.github/workflows/ci.yml). Waiting where the sound cannot
# wait: wherever the realtime sanitizer is.
if(ADLplug_SANITIZERS)
  cmake_language(DEFER CALL adlplug_sanitizer_test_environment)

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND ADLplug_SANITIZERS MATCHES "(^|,)address(,|$)")
    add_executable(ADLplug_leak_canary sanitizers/leak_canary.c)
    adlplug_set_language_standard(ADLplug_leak_canary)
    adlplug_own_sources(sanitizers/leak_canary.c)
    add_test(NAME sanitizer.leak_detection COMMAND ADLplug_leak_canary)
    set_tests_properties(sanitizer.leak_detection PROPERTIES
      PASS_REGULAR_EXPRESSION "ERROR: LeakSanitizer: detected memory leaks")
  endif()

  if(ADLplug_SANITIZERS MATCHES "(^|,)thread(,|$)")
    add_executable(ADLplug_race_canary sanitizers/race_canary.c)
    adlplug_set_language_standard(ADLplug_race_canary)
    adlplug_own_sources(sanitizers/race_canary.c)
    add_test(NAME sanitizer.race_detection COMMAND ADLplug_race_canary)
    set_tests_properties(sanitizer.race_detection PROPERTIES
      PASS_REGULAR_EXPRESSION "WARNING: ThreadSanitizer: data race")
  endif()

  if(ADLplug_SANITIZERS MATCHES "(^|,)memory(,|$)")
    add_executable(ADLplug_memory_canary sanitizers/memory_canary.c)
    adlplug_set_language_standard(ADLplug_memory_canary)
    adlplug_own_sources(sanitizers/memory_canary.c)
    add_test(NAME sanitizer.memory_detection COMMAND ADLplug_memory_canary)
    set_tests_properties(sanitizer.memory_detection PROPERTIES
      PASS_REGULAR_EXPRESSION "WARNING: MemorySanitizer: use-of-uninitialized-value")
  endif()

  # C++ where the others are C: the attribute the sanitizer watches for is one
  # Clang acts on in C++ alone (realtime_canary.cc says so).
  if(ADLplug_SANITIZERS MATCHES "(^|,)realtime(,|$)")
    add_executable(ADLplug_realtime_canary sanitizers/realtime_canary.cc)
    adlplug_set_language_standard(ADLplug_realtime_canary)
    adlplug_own_sources(sanitizers/realtime_canary.cc)
    add_test(NAME sanitizer.realtime_detection COMMAND ADLplug_realtime_canary)
    set_tests_properties(sanitizer.realtime_detection PROPERTIES
      PASS_REGULAR_EXPRESSION "ERROR: RealtimeSanitizer")
  endif()
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
  juce::juce_graphics
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

# References are kept per system and instruction set: where the target has FMA
# instructions, Clang fuses multiply-adds, and the systems' maths libraries
# differ. Debug and Release builds share a key.
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(ADLplug_RENDER_SYSTEM "macos")
else()
  string(TOLOWER "${CMAKE_SYSTEM_NAME}" ADLplug_RENDER_SYSTEM)
endif()
# A Linux distribution other than Ubuntu has its own version of the maths
# library, so its references have a key of their own, as AlmaLinux has in CI.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  cmake_host_system_information(RESULT ADLplug_DISTRIBUTION QUERY DISTRIB_ID)
  if(ADLplug_DISTRIBUTION AND NOT ADLplug_DISTRIBUTION STREQUAL "ubuntu")
    string(TOLOWER "linux-${ADLplug_DISTRIBUTION}" ADLplug_RENDER_SYSTEM)
  endif()
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

# The parameters of upstream ADLplug 1 come first, in their order and with
# their IDs: projects saved with the plugins of upstream refer to them, and the
# VST3 plugin maps them for hosts that replace those plugins with it.
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

# The emulator cores, with the core that plays in place of each when a build
# leaves it out (cmake/Cores.cmake).
set(default_core ${ADLplug_DEFAULT_CORE})
set(cores ${ADLplug_CORES})

# The fallback renders last 2 seconds, after the warm-up of the main render
# test. The warm-up is when the plugin's worker thread measures the instruments,
# in real time; the render of an instrument measured later differs. A Debug
# build on a busy machine did not always finish within 1 second (render.fallback.1
# failed in CI, and 6 renders at once reproduced it), and does within 5.
set(fallback_sequence 2 5000)

set(fallback_cores "")
foreach(core IN LISTS cores)
  string(REPLACE "|" ";" core_fields "${core}")
  list(GET core_fields 0 core_index)
  list(GET core_fields 1 core_name)
  list(GET core_fields 2 core_option)
  list(GET core_fields 3 core_substitute)

  if(${core_option})
    # Every core of the build plays, and the plugin names the one asked for.
    # Release only: the low-level cores are too slow in Debug builds.
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
      add_test(NAME render.core.${core_index}
        COMMAND ADLplug_render "${plugin}" "${render_out}/core${core_index}.f32" 2 1000
          --emulator ${core_index} --require-emulator "${core_name}" --require-sound)
      set_tests_properties(render.core.${core_index} PROPERTIES TIMEOUT 600 LABELS core)
    endif()
    continue()
  endif()

  # A core that the build leaves out plays as the one in its place, or as the
  # default core when the build leaves that one out too: asking for it gives
  # the output of that core. The parameters keep the number asked for (plan
  # D54), so the outputs are compared rather than the names. Only builds that
  # leave cores out have these tests, such as those CI makes on a push to main.
  foreach(other IN LISTS cores)
    string(REPLACE "|" ";" other_fields "${other}")
    list(GET other_fields 0 other_index)
    list(GET other_fields 2 other_option)
    if(other_index EQUAL core_substitute AND NOT ${other_option})
      set(core_substitute ${default_core})
    endif()
  endforeach()
  if(NOT core_substitute IN_LIST fallback_cores)
    list(APPEND fallback_cores ${core_substitute})
    add_test(NAME render.fallback.to.${core_substitute}
      COMMAND ADLplug_render "${plugin}" "${render_out}/fallback-to-${core_substitute}.f32" ${fallback_sequence}
        --emulator ${core_substitute} --require-sound
        --save-hashes "${render_out}/fallback-to-${core_substitute}.hashes")
    set_tests_properties(render.fallback.to.${core_substitute} PROPERTIES
      FIXTURES_SETUP fallback-to-${core_substitute} TIMEOUT 600 LABELS fallback)
  endif()
  add_test(NAME render.fallback.${core_index}
    COMMAND ADLplug_render "${plugin}" "${render_out}/fallback${core_index}.f32" ${fallback_sequence}
      --emulator ${core_index} --expect-output-of "${render_out}/fallback-to-${core_substitute}.hashes")
  set_tests_properties(render.fallback.${core_index} PROPERTIES
    FIXTURES_REQUIRED fallback-to-${core_substitute} TIMEOUT 600 LABELS fallback)
endforeach()
