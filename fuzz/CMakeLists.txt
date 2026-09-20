# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
# The fuzz targets. Each target is a file <name>.cc that defines
# LLVMFuzzerTestOneInput, and is built two ways:
#  - with ADLplug_BUILD_TESTS, as ADLplug_fuzz_<name>_replay, the target and
#    replay.cc, which CTest runs as fuzz.<name> on the target's seed inputs and
#    on its regression inputs, on every system;
#  - with ADLplug_BUILD_FUZZERS, as ADLplug_fuzz_<name>, the target linked with
#    libFuzzer (-fsanitize=fuzzer), on Linux. It is meant to be built with the
#    address and undefined sanitizers as well, on top of a *-sanitize preset.
#
# A target compiles the sources it tests itself, the libraries' among them, so
# that the libFuzzer build instruments all of them for coverage; the libraries
# the plugin links are left as they are.
#
# Seed inputs are not copied here: the banks and instruments that come with the
# submodules are given where they are, each under its own license. Inputs that
# once failed go in regressions/<chip>/<name>/, with the commit that fixes them;
# they are found when CMake configures. Dictionaries are in dict/, for
# libFuzzer's -dict=.
#
# Beside each libFuzzer program, ADLplug_fuzz_<name>.args lists the arguments
# it runs with, one to a line: its dictionary, its seed inputs and its
# regression inputs. ci/fuzz.sh runs every target by these files.

if(ADLplug_SANITIZERS)
  cmake_language(DEFER CALL adlplug_sanitizer_test_environment)
endif()

if(ADLplug_CHIP STREQUAL "OPL3")
  set(ADLplug_FUZZ_CHIP "opl3")
  set(ADLplug_FUZZ_LIBRARY "${PROJECT_SOURCE_DIR}/thirdparty/libADLMIDI")
  set(ADLplug_FUZZ_FILE_SOURCE "${ADLplug_FUZZ_LIBRARY}/src/wopl/wopl_file.c")
  set(ADLplug_FUZZ_FILE_DICTIONARY "${CMAKE_CURRENT_SOURCE_DIR}/dict/wopl.dict")
  set(ADLplug_FUZZ_BANK_SEEDS
    "${ADLplug_FUZZ_LIBRARY}/fm_banks/wopl_files"
    "${ADLplug_FUZZ_LIBRARY}/fm_banks/ibk_files/Inst")
else()
  set(ADLplug_FUZZ_CHIP "opn2")
  set(ADLplug_FUZZ_LIBRARY "${PROJECT_SOURCE_DIR}/thirdparty/libOPNMIDI")
  set(ADLplug_FUZZ_FILE_SOURCE "${ADLplug_FUZZ_LIBRARY}/src/wopn/wopn_file.c")
  set(ADLplug_FUZZ_FILE_DICTIONARY "${CMAKE_CURRENT_SOURCE_DIR}/dict/wopn.dict")
  set(ADLplug_FUZZ_BANK_SEEDS
    "${ADLplug_FUZZ_LIBRARY}/fm_banks"
    "${PROJECT_SOURCE_DIR}/thirdparty/OPN2BankEditor/Bank_Examples")
endif()

# How large the input of the fuzz.<target>.big tests is. Sixty-four mebibytes is
# a thousand times what a fuzzer would make and small enough for the runners to
# hold three of them, one after another, with the sanitizers on top; by hand a
# program takes any size (--made 1G).
set(ADLplug_FUZZ_BIG_INPUT "64M")

# adlplug_fuzz_target(<name> [JUCE] OWN_SOURCES <file>... OTHER_SOURCES <file>...
#                     [LIBRARIES <target>...] [DEFINITIONS <name>=<value>...]
#                     SEEDS <directory>... [DICTIONARY <file>]
#                     [ARGUMENTS <argument>...] [LONG_RUNS_ONLY])
#
# JUCE makes the programs with juce_add_console_app() instead of
# add_executable(), for a target whose sources need JuceHeader.h and the JUCE
# modules: the plugin's processor, for one.
#
# ARGUMENTS go into the manifest before the inputs, for a target that asks
# something of libFuzzer that the others do not. They come after the arguments
# every target gets, so a target that names one of those again has the say.
#
# Every manifest says -timeout=60. An input is meant to be a few milliseconds of
# work, or a second or two for the targets that play a whole measurement, so a
# minute of one means something has gone wrong with it rather than that it is
# long. libFuzzer would otherwise work on it for twenty minutes, which is its
# own default, and then hand back a job that spent itself on the one input: that
# is how a rate of 16 Hz, which the resampler of both libraries never came back
# from, took a whole fuzzing job of CI (patches/libADLMIDI/0009-*, and
# fuzz/regressions/<chip>/host/sixteen-samples-a-second, which is the input).
#
# LONG_RUNS_ONLY marks a target whose every input is heavy enough that the
# fuzzing of a push would get through too few of them to be worth the minute.
# CMake writes an empty <target>.long beside the manifest, and ci/fuzz.sh leaves
# such a target out unless it is told to take the long ones as well. The replay
# test runs on every system either way.
function(adlplug_fuzz_target name)
  cmake_parse_arguments(PARSE_ARGV 1 arg "JUCE;LONG_RUNS_ONLY" "DICTIONARY"
    "OWN_SOURCES;OTHER_SOURCES;LIBRARIES;DEFINITIONS;SEEDS;ARGUMENTS")

  set(inputs ${arg_SEEDS})
  set(regressions "${CMAKE_CURRENT_SOURCE_DIR}/regressions/${ADLplug_FUZZ_CHIP}/${name}")
  if(IS_DIRECTORY "${regressions}")
    list(APPEND inputs "${regressions}")
  endif()

  set(configurations "")
  if(ADLplug_BUILD_TESTS)
    list(APPEND configurations "replay")
  endif()
  if(ADLplug_BUILD_FUZZERS)
    list(APPEND configurations "libfuzzer")
  endif()

  foreach(configuration IN LISTS configurations)
    if(configuration STREQUAL "replay")
      set(target "ADLplug_fuzz_${name}_replay")
      set(sources replay.cc "${name}.cc" ${arg_OWN_SOURCES} ${arg_OTHER_SOURCES})
    else()
      set(target "ADLplug_fuzz_${name}")
      set(sources "${name}.cc" ${arg_OWN_SOURCES} ${arg_OTHER_SOURCES})
    endif()

    if(arg_JUCE)
      # A JUCE program, for the sources that want JuceHeader.h and the modules.
      # The libFuzzer build has no main() of its own, which juce_add_console_app
      # does not mind: it only makes the target and gives it JUCE's settings.
      juce_add_console_app(${target})
      juce_generate_juce_header(${target})
      target_sources(${target} PRIVATE ${sources})
      get_target_property(generated ${target} JUCE_GENERATED_SOURCES_DIRECTORY)
      target_include_directories(${target} SYSTEM PRIVATE "${generated}")
      # JUCE would put the program under <target>_artefacts/<configuration>/;
      # every fuzz program belongs beside the .args files that name it, which is
      # where ci/fuzz.sh looks for it.
      set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
      foreach(configuration_name IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${target} PROPERTIES
          "RUNTIME_OUTPUT_DIRECTORY_${configuration_name}" "${CMAKE_CURRENT_BINARY_DIR}")
      endforeach()
    else()
      add_executable(${target} ${sources})
    endif()

    if(configuration STREQUAL "libfuzzer")
      target_compile_options(${target} PRIVATE -fsanitize=fuzzer)
      target_link_options(${target} PRIVATE -fsanitize=fuzzer)
      set(arguments "-timeout=60")
      if(arg_DICTIONARY)
        list(APPEND arguments "-dict=${arg_DICTIONARY}")
      endif()
      list(APPEND arguments ${arg_ARGUMENTS} ${inputs})
      list(JOIN arguments "\n" arguments)
      file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${target}.args" "${arguments}\n")
      if(arg_LONG_RUNS_ONLY)
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${target}.long" "")
      endif()
    endif()
    adlplug_set_language_standard(${target})
    target_compile_definitions(${target} PRIVATE
      "ADLPLUG_${ADLplug_CHIP}"
      ${arg_DEFINITIONS}
      # The library's code fills its fixed-width name fields with strncpy.
      $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>)
    if(arg_LIBRARIES)
      target_link_libraries(${target} PRIVATE ${arg_LIBRARIES})
    endif()
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/sources")
    target_include_directories(${target} SYSTEM PRIVATE
      "${ADLplug_FUZZ_LIBRARY}/src" "${ADLplug_FUZZ_LIBRARY}/include")
  endforeach()
  adlplug_own_sources(replay.cc "${name}.cc" ${arg_OWN_SOURCES})

  if(ADLplug_BUILD_TESTS)
    add_test(NAME "fuzz.${name}" COMMAND "ADLplug_fuzz_${name}_replay" ${inputs})
    set_tests_properties("fuzz.${name}" PROPERTIES TIMEOUT 900 LABELS fuzz)

    # What a target does with an input far larger than a fuzzer makes. libFuzzer
    # keeps to a few kilobytes, but the size of what comes from outside is not
    # ours to choose: a bank file is as large as it is, and a host hands over the
    # state of a project whole. The programs make the input up (replay.cc
    # --made), so nothing of this size is in the repository.
    add_test(NAME "fuzz.${name}.big"
      COMMAND "ADLplug_fuzz_${name}_replay" --made "${ADLplug_FUZZ_BIG_INPUT}")
    set_tests_properties("fuzz.${name}.big" PROPERTIES TIMEOUT 900 LABELS fuzz)
  endif()
endfunction()

# The bank and instrument files that people open.
adlplug_fuzz_target(bank_file
  OWN_SOURCES "${PROJECT_SOURCE_DIR}/sources/${ADLplug_FUZZ_CHIP}/adl/instrument.cc"
  OTHER_SOURCES "${ADLplug_FUZZ_FILE_SOURCE}"
  SEEDS ${ADLplug_FUZZ_BANK_SEEDS}
  DICTIONARY "${ADLplug_FUZZ_FILE_DICTIONARY}")

# The MIDI that a host sends, and the synthesis it drives. This one plays, so
# it links the library instead of compiling the sources it tests: the emulator
# cores, the real-time API and the MIDI driver are most of it. The libFuzzer
# build gives the library coverage instrumentation below, so that the fuzzer
# can see inside it.
if(ADLplug_CHIP STREQUAL "OPL3")
  set(ADLplug_FUZZ_PLAY_LIBRARY ADLMIDI_static)
  set(ADLplug_FUZZ_PLAY_BANK "${ADLplug_FUZZ_LIBRARY}/fm_banks/wopl_files/GM-By-J.A.Nguyen-and-Wohlstand.wopl")
else()
  set(ADLplug_FUZZ_PLAY_LIBRARY OPNMIDI_static)
  set(ADLplug_FUZZ_PLAY_BANK "${ADLplug_FUZZ_LIBRARY}/fm_banks/xg.wopn")
endif()

# A seed for each emulator core of the build: it plays a note on that core and
# generates a moment of audio. The replay test then plays every core on every
# system, which is where the sanitizers see the cores that the render tests
# leave to Release builds. Every byte of a seed is below 128, so that CMake can
# write it; fuzz/midi_synth.cc says what they mean.
set(ADLplug_FUZZ_PLAY_SEEDS "${CMAKE_CURRENT_BINARY_DIR}/seeds/midi_synth")
file(MAKE_DIRECTORY "${ADLplug_FUZZ_PLAY_SEEDS}")
foreach(core IN LISTS ADLplug_CORES)
  string(REPLACE "|" ";" core_fields "${core}")
  list(GET core_fields 0 core_index)
  math(EXPR core_byte "${core_index} + 1")
  # The emulator, middle C at velocity 100 on the first channel, 544 frames.
  string(ASCII ${core_byte} 96 60 100 80 seed)
  file(WRITE "${ADLplug_FUZZ_PLAY_SEEDS}/core-${core_index}" "${seed}")
endforeach()

adlplug_fuzz_target(midi_synth
  OWN_SOURCES "${PROJECT_SOURCE_DIR}/sources/${ADLplug_FUZZ_CHIP}/adl/player.cc"
  LIBRARIES ${ADLplug_FUZZ_PLAY_LIBRARY}
  DEFINITIONS "ADLPLUG_FUZZ_BANK=\"${ADLplug_FUZZ_PLAY_BANK}\""
  SEEDS "${ADLplug_FUZZ_PLAY_SEEDS}"
  DICTIONARY "${CMAKE_CURRENT_SOURCE_DIR}/dict/midi.dict")

if(ADLplug_BUILD_FUZZERS)
  # Coverage for the library, which is where a target that plays spends its
  # time; without it libFuzzer sees only the harness. The plugin of the same
  # build directory links the same library and is instrumented too, which costs
  # it speed it does not need here.
  #
  # The depth of the stack is the one coverage feature left out of it. That one
  # counts how deep an input went, in a thread-local __sancov_lowest_stack which
  # only some sanitizer runtimes resolve: a program with the address sanitizer
  # finds it, one with the thread sanitizer does not. The plugin then cannot be
  # loaded at all -- not by a host, and not by the program JUCE runs to write the
  # LV2 manifest, which is where such a build stops (twenty lines reproduce it:
  # the same shared object loads under -fsanitize=address and not under
  # -fsanitize=thread). A libFuzzer binary resolves it whichever sanitizer it
  # has, so a build that makes the fuzz targets and no plugin can keep the
  # feature: that is ADLplug_FUZZ_FULL_COVERAGE, which the long fuzzing sets
  # (ci/build.sh --fuzz-only). What libFuzzer steers by -- the counters, the
  # table of program counters, the comparisons and the indirect calls -- is there
  # either way.
  set(ADLplug_FUZZ_LIBRARY_COVERAGE -fsanitize=fuzzer-no-link)
  if(ADLplug_FUZZ_FULL_COVERAGE)
    message(STATUS
      "Fuzzing: the libraries carry the whole coverage, so build the fuzz targets alone")
  else()
    list(APPEND ADLplug_FUZZ_LIBRARY_COVERAGE -fno-sanitize-coverage=stack-depth)
  endif()
  target_compile_options(${ADLplug_FUZZ_PLAY_LIBRARY} PRIVATE ${ADLplug_FUZZ_LIBRARY_COVERAGE})
endif()

# The processor itself, for the two targets that read what comes from outside
# it: the state a project holds, and what a host does while the plugin is
# loaded. Both build the plugin's own sources with the interface left out:
# plugin_processor.cc knows nothing of the editor (hasEditor() and
# createEditor() are in plugin_editor.cc, and each target says instead that
# there is none), so neither the interface nor the modules it alone needs come
# in. With JUCE 9's headless audio processors module, that leaves a program
# without juce_gui_basics, which builds where the plugin cannot: a Linux
# without the X11 development packages, for one.
set(ADLplug_FUZZ_PROCESSOR_SOURCES
  "${PROJECT_SOURCE_DIR}/sources/plugin_processor.cc"
  "${PROJECT_SOURCE_DIR}/sources/plugin_state.cc"
  "${PROJECT_SOURCE_DIR}/sources/bank_manager.cc"
  "${PROJECT_SOURCE_DIR}/sources/worker.cc"
  "${PROJECT_SOURCE_DIR}/sources/messages.cc"
  "${PROJECT_SOURCE_DIR}/sources/utility/midi.cc"
  "${PROJECT_SOURCE_DIR}/sources/utility/simple_fifo.cc"
  "${PROJECT_SOURCE_DIR}/sources/utility/pak.cc"
  # The icons of the chip settings, which the messages carry (messages.h).
  "${PROJECT_SOURCE_DIR}/sources/ui/utility/image.cc"
  "${PROJECT_SOURCE_DIR}/sources/resources.c"
  "${PROJECT_SOURCE_DIR}/sources/${ADLplug_FUZZ_CHIP}/parameter_block.cc"
  "${PROJECT_SOURCE_DIR}/sources/${ADLplug_FUZZ_CHIP}/adl/chip_settings.cc"
  "${PROJECT_SOURCE_DIR}/sources/${ADLplug_FUZZ_CHIP}/adl/instrument.cc"
  "${PROJECT_SOURCE_DIR}/sources/${ADLplug_FUZZ_CHIP}/adl/instrument_properties.cc"
  "${PROJECT_SOURCE_DIR}/sources/${ADLplug_FUZZ_CHIP}/adl/player.cc"
  "${PROJECT_SOURCE_DIR}/sources/${ADLplug_FUZZ_CHIP}/adl/measurer/measurer.cpp")

set(ADLplug_FUZZ_PROCESSOR_LIBRARIES ${ADLplug_FUZZ_PLAY_LIBRARY}
  # The headless audio processors module of JUCE 9 is the same processor
  # without the interface, and it needs neither juce_gui_basics nor the X11
  # packages that module wants on Linux. The graphics module comes in for the
  # icons that the messages carry.
  juce::juce_audio_processors_headless
  juce::juce_graphics
  juce::juce_recommended_config_flags)

set(ADLplug_FUZZ_PROCESSOR_DEFINITIONS
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  # What the plugin's own target gives plugin_processor.cc: the names and the
  # codes that the VST3 compatibility and getName() are written with.
  "JucePlugin_Name=\"${ADLplug_NAME}\""
  "JucePlugin_PluginCode=0x${ADLplug_PLUGIN_CODE_HEX}"
  "ADLPLUG_VERSION_DISPLAY=\"${ADLplug_VERSION_DISPLAY}\""
  "ADLPLUG_UPSTREAM_NAME=\"${ADLplug_UPSTREAM_NAME}\""
  "ADLPLUG_UPSTREAM_MANUFACTURER=\"${ADLplug_UPSTREAM_MANUFACTURER}\""
  "ADLPLUG_UPSTREAM_VST2_CLASS=\"${ADLplug_UPSTREAM_VST2_CLASS}\"")

# A seed: the smallest state that carries one of everything the plugin looks
# for, so that a made-up input starts from a shape that means something. CMake
# writes it, since it is text; fuzz/dict/state.dict has the names.
set(ADLplug_FUZZ_STATE_SEEDS "${CMAKE_CURRENT_BINARY_DIR}/seeds/state")
file(MAKE_DIRECTORY "${ADLplug_FUZZ_STATE_SEEDS}")
if(ADLplug_CHIP STREQUAL "OPL3")
  set(ADLplug_FUZZ_STATE_GLOBAL
      "    <VALUE name=\"volume_model\" val=\"1\"/>\n"
      "    <VALUE name=\"deep_tremolo\" val=\"1\"/>\n"
      "    <VALUE name=\"deep_vibrato\" val=\"0\"/>\n"
      "    <VALUE name=\"mt32_defaults\" val=\"0\"/>\n")
  set(ADLplug_FUZZ_STATE_CHIP
      "    <VALUE name=\"emulator\" val=\"2\"/>\n"
      "    <VALUE name=\"chip_count\" val=\"2\"/>\n"
      "    <VALUE name=\"4op_count\" val=\"6\"/>\n")
else()
  set(ADLplug_FUZZ_STATE_GLOBAL
      "    <VALUE name=\"volume_model\" val=\"1\"/>\n"
      "    <VALUE name=\"lfo_enable\" val=\"1\"/>\n"
      "    <VALUE name=\"lfo_frequency\" val=\"3\"/>\n")
  set(ADLplug_FUZZ_STATE_CHIP
      "    <VALUE name=\"emulator\" val=\"0\"/>\n"
      "    <VALUE name=\"chip_count\" val=\"2\"/>\n"
      "    <VALUE name=\"chip_type\" val=\"0\"/>\n")
endif()
string(CONCAT ADLplug_FUZZ_STATE_SEED
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<ADLMIDI-state>\n"
  "  <bank>\n"
  "    <VALUE name=\"bank\" val=\"0\"/>\n"
  "    <VALUE name=\"name\" val=\"Seed\"/>\n"
  "  </bank>\n"
  "  <instrument>\n"
  "    <VALUE name=\"bank\" val=\"0\"/>\n"
  "    <VALUE name=\"program\" val=\"0\"/>\n"
  "    <VALUE name=\"name\" val=\"Seed\"/>\n"
  "    <VALUE name=\"blank\" val=\"0\"/>\n"
  "    <VALUE name=\"c1attack\" val=\"15\"/>\n"
  "    <VALUE name=\"c1level\" val=\"7\"/>\n"
  "    <VALUE name=\"m1attack\" val=\"15\"/>\n"
  "    <VALUE name=\"m1level\" val=\"0\"/>\n"
  "    <VALUE name=\"delay_on_ms\" val=\"100\"/>\n"
  "    <VALUE name=\"delay_off_ms\" val=\"100\"/>\n"
  "  </instrument>\n"
  "  <selection>\n"
  "    <VALUE name=\"part\" val=\"0\"/>\n"
  "    <VALUE name=\"bank\" val=\"0\"/>\n"
  "    <VALUE name=\"program\" val=\"0\"/>\n"
  "  </selection>\n"
  "  <chip>\n"
  ${ADLplug_FUZZ_STATE_CHIP}
  "  </chip>\n"
  "  <global>\n"
  ${ADLplug_FUZZ_STATE_GLOBAL}
  "  </global>\n"
  "  <common>\n"
  "    <VALUE name=\"bank_title\" val=\"Seed\"/>\n"
  "    <VALUE name=\"part\" val=\"0\"/>\n"
  "    <VALUE name=\"master_volume\" val=\"1\"/>\n"
  "  </common>\n"
  "</ADLMIDI-state>\n")
file(WRITE "${ADLplug_FUZZ_STATE_SEEDS}/small" "${ADLplug_FUZZ_STATE_SEED}")

adlplug_fuzz_target(state JUCE
  OWN_SOURCES ${ADLplug_FUZZ_PROCESSOR_SOURCES}
  LIBRARIES ${ADLplug_FUZZ_PROCESSOR_LIBRARIES}
  DEFINITIONS ${ADLplug_FUZZ_PROCESSOR_DEFINITIONS}
  SEEDS "${ADLplug_FUZZ_STATE_SEEDS}"
  DICTIONARY "${CMAKE_CURRENT_SOURCE_DIR}/dict/state.dict")

# What a host does while the plugin is loaded: the MIDI it puts in a buffer, the
# automation it writes to the parameters, and the blocks it asks for. The seed
# prepares the plugin, plays a note and asks for two blocks; every byte of it is
# below 128, so that CMake can write it, which leaves out the two kinds of
# record whose first bit is set -- fuzz/dict/host.dict has those, and one
# mutation reaches them.
set(ADLplug_FUZZ_HOST_SEEDS "${CMAKE_CURRENT_BINARY_DIR}/seeds/host")
file(MAKE_DIRECTORY "${ADLplug_FUZZ_HOST_SEEDS}")
string(ASCII 7 8 96 60 100 79 97 67 90 79 ADLplug_FUZZ_HOST_SEED)
file(WRITE "${ADLplug_FUZZ_HOST_SEEDS}/a-note-and-two-blocks" "${ADLplug_FUZZ_HOST_SEED}")

adlplug_fuzz_target(host JUCE
  OWN_SOURCES ${ADLplug_FUZZ_PROCESSOR_SOURCES}
  LIBRARIES ${ADLplug_FUZZ_PROCESSOR_LIBRARIES}
  DEFINITIONS ${ADLplug_FUZZ_PROCESSOR_DEFINITIONS}
  SEEDS "${ADLplug_FUZZ_HOST_SEEDS}"
  DICTIONARY "${CMAKE_CURRENT_SOURCE_DIR}/dict/host.dict")

# How long an instrument sounds: the measurement the worker makes for every
# instrument the plugin is given. One input plays a hundred seconds of audio on
# a chip of its own, so the target says LONG_RUNS_ONLY and the fuzzing of a push
# passes it by; -max_len keeps the fuzzer inside the instrument, since the bytes
# past it are read by nobody. The seed is
# an instrument that sounds, and it is in the repository rather than written
# here: an instrument is mostly zeros, and CMake writes text.
# fuzz/dict/measurement.dict has the register bytes worth trying.
adlplug_fuzz_target(measurement LONG_RUNS_ONLY
  OWN_SOURCES "${PROJECT_SOURCE_DIR}/sources/${ADLplug_FUZZ_CHIP}/adl/instrument.cc"
              "${PROJECT_SOURCE_DIR}/sources/${ADLplug_FUZZ_CHIP}/adl/measurer/measurer.cpp"
  LIBRARIES ${ADLplug_FUZZ_PLAY_LIBRARY}
  SEEDS "${CMAKE_CURRENT_SOURCE_DIR}/seeds/${ADLplug_FUZZ_CHIP}/measurement"
  DICTIONARY "${CMAKE_CURRENT_SOURCE_DIR}/dict/measurement.dict"
  ARGUMENTS "-max_len=64")

# The messages the editor sends the plugin about its banks, one after another,
# and the bookkeeping they leave behind. The processor is the same one the
# state target builds, so the sources, the libraries and the definitions are
# the same lists. The seed makes a bank, puts two programs in it, names both
# and deletes one, asking for the layout of the banks in between; every byte
# of it is between 1 and 127, which is what CMake can write, so the records
# whose first bit is set are not in it -- fuzz/dict/bank_manager.dict has
# those, along with the numbers that are not banks. An input here takes longer
# than most: the parameters of a selected program ask the worker for a
# measurement, and a measurement can take a second or two.
set(ADLplug_FUZZ_BANKS_SEEDS "${CMAKE_CURRENT_BINARY_DIR}/seeds/bank_manager")
file(MAKE_DIRECTORY "${ADLplug_FUZZ_BANKS_SEEDS}")
string(ASCII 37 1 1 5 17 2 17 5 101 1 1 66 97 110 107 32 111 110 101 49 1 1 7 3
  117 1 1 7 80 114 111 103 32 111 110 101 65 1 1 5 1 16 4
  ADLplug_FUZZ_BANKS_SEED)
file(WRITE "${ADLplug_FUZZ_BANKS_SEEDS}/a-bank-of-two-programs" "${ADLplug_FUZZ_BANKS_SEED}")

adlplug_fuzz_target(bank_manager JUCE
  OWN_SOURCES ${ADLplug_FUZZ_PROCESSOR_SOURCES}
  LIBRARIES ${ADLplug_FUZZ_PROCESSOR_LIBRARIES}
  DEFINITIONS ${ADLplug_FUZZ_PROCESSOR_DEFINITIONS}
  SEEDS "${ADLplug_FUZZ_BANKS_SEEDS}"
  DICTIONARY "${CMAKE_CURRENT_SOURCE_DIR}/dict/bank_manager.dict")

# The small readers: the pack of banks the plugin carries, the fields a name sits
# in, and the configuration file. None of them plays, and all three read bytes the
# plugin did not write. Configuration itself is not here: it includes the key map,
# which includes the keyboard of the editor, which wants the interface modules of
# JUCE that these targets do without (the processor's targets use the headless one
# for the same reason), so the configuration goes through SimpleIni the way
# Configuration::load_ini does. juce_core is all the rest of it needs: a name
# becomes a String, and a pack reads its content through JUCE's streams.
#
# The seeds: the pack that the build just made for the plugin, whole, since a
# fuzzer would never come upon the zlib stream inside one (fuzz/parsers.cc takes
# an input that begins as a pack does for a pack); and a configuration file, a
# field of bytes and a name, which are text and are written here.
set(ADLplug_FUZZ_PARSERS_SEEDS "${CMAKE_CURRENT_BINARY_DIR}/seeds/parsers")
file(MAKE_DIRECTORY "${ADLplug_FUZZ_PARSERS_SEEDS}")
add_custom_command(
  OUTPUT "${ADLplug_FUZZ_PARSERS_SEEDS}/a-pack-of-banks"
  COMMAND "${CMAKE_COMMAND}" -E copy "${ADLplug_BANKS_PAK}"
    "${ADLplug_FUZZ_PARSERS_SEEDS}/a-pack-of-banks"
  DEPENDS "${ADLplug_BANKS_PAK}"
  COMMENT "Taking the pack of banks as a fuzz seed"
  VERBATIM)
add_custom_target(ADLplug_fuzz_parsers_seeds
  DEPENDS "${ADLplug_FUZZ_PARSERS_SEEDS}/a-pack-of-banks")

# A configuration file as the plugin writes one, a field of sixteen bytes, and a
# name of six bytes for a field of four, which is a character cut in half.
string(ASCII 194 ADLplug_FUZZ_PARSERS_INI_RECORD)
file(WRITE "${ADLplug_FUZZ_PARSERS_SEEDS}/a-configuration-file"
  "${ADLplug_FUZZ_PARSERS_INI_RECORD}configuration-version = 1\n[paths]\nlast-instrument-directory = \n[piano]\nlayout = qwerty\nkeymap:qwerty = zsxdcvgbhnjm\n")
string(ASCII 79 66 97 110 107 32 111 110 101 32 105 110 32 97 32 102 105
  ADLplug_FUZZ_PARSERS_FIELD)
file(WRITE "${ADLplug_FUZZ_PARSERS_SEEDS}/a-field-of-bytes" "${ADLplug_FUZZ_PARSERS_FIELD}")
string(ASCII 134 66 97 110 107 32 49 4 ADLplug_FUZZ_PARSERS_NAME)
file(WRITE "${ADLplug_FUZZ_PARSERS_SEEDS}/a-name-for-a-field" "${ADLplug_FUZZ_PARSERS_NAME}")

# Nothing at all, which a reader is as entitled to as it is to bytes: a pack and
# a configuration file of no bytes and no address, the same two with an address
# they have not got but a length they claim, and a name of no bytes for a field
# of none.
string(ASCII 7 199 128 64 193 1 1 3 1 1 ADLplug_FUZZ_PARSERS_NOTHING)
file(WRITE "${ADLplug_FUZZ_PARSERS_SEEDS}/nothing-at-all" "${ADLplug_FUZZ_PARSERS_NOTHING}")

adlplug_fuzz_target(parsers JUCE
  OWN_SOURCES "${PROJECT_SOURCE_DIR}/sources/utility/pak.cc"
  LIBRARIES simple-ini juce::juce_core juce::juce_recommended_config_flags
  SEEDS "${ADLplug_FUZZ_PARSERS_SEEDS}"
  DICTIONARY "${CMAKE_CURRENT_SOURCE_DIR}/dict/parsers.dict")
foreach(target IN ITEMS ADLplug_fuzz_parsers_replay ADLplug_fuzz_parsers)
  if(TARGET ${target})
    add_dependencies(${target} ADLplug_fuzz_parsers_seeds)
  endif()
endforeach()

# resources.c embeds the pack of banks that the build makes for the plugin, and
# a source property belongs to the directory that compiles the file.
adlplug_embed_banks("${PROJECT_SOURCE_DIR}/sources/resources.c")
foreach(target IN ITEMS ADLplug_fuzz_state_replay ADLplug_fuzz_state
                        ADLplug_fuzz_host_replay ADLplug_fuzz_host)
  if(TARGET ${target})
    add_dependencies(${target} ADLplug_banks)
  endif()
endforeach()
