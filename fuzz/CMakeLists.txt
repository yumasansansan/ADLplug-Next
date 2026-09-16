# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
# The fuzz targets (plan D48). Each target is a file <name>.cc that defines
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

if(ADLplug_SANITIZERS)
  cmake_language(DEFER CALL adlplug_sanitizer_test_environment)
endif()

if(ADLplug_CHIP STREQUAL "OPL3")
  set(ADLplug_FUZZ_CHIP "opl3")
  set(ADLplug_FUZZ_LIBRARY "${PROJECT_SOURCE_DIR}/thirdparty/libADLMIDI")
  set(ADLplug_FUZZ_FILE_SOURCE "${ADLplug_FUZZ_LIBRARY}/src/wopl/wopl_file.c")
  set(ADLplug_FUZZ_BANK_SEEDS
    "${ADLplug_FUZZ_LIBRARY}/fm_banks/wopl_files"
    "${ADLplug_FUZZ_LIBRARY}/fm_banks/ibk_files/Inst")
else()
  set(ADLplug_FUZZ_CHIP "opn2")
  set(ADLplug_FUZZ_LIBRARY "${PROJECT_SOURCE_DIR}/thirdparty/libOPNMIDI")
  set(ADLplug_FUZZ_FILE_SOURCE "${ADLplug_FUZZ_LIBRARY}/src/wopn/wopn_file.c")
  set(ADLplug_FUZZ_BANK_SEEDS
    "${ADLplug_FUZZ_LIBRARY}/fm_banks"
    "${PROJECT_SOURCE_DIR}/thirdparty/OPN2BankEditor/Bank_Examples")
endif()

# adlplug_fuzz_target(<name> OWN_SOURCES <file>... OTHER_SOURCES <file>...
#                     SEEDS <directory>...)
function(adlplug_fuzz_target name)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "OWN_SOURCES;OTHER_SOURCES;SEEDS")

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
      add_executable(${target} replay.cc "${name}.cc" ${arg_OWN_SOURCES} ${arg_OTHER_SOURCES})
    else()
      set(target "ADLplug_fuzz_${name}")
      add_executable(${target} "${name}.cc" ${arg_OWN_SOURCES} ${arg_OTHER_SOURCES})
      target_compile_options(${target} PRIVATE -fsanitize=fuzzer)
      target_link_options(${target} PRIVATE -fsanitize=fuzzer)
    endif()
    adlplug_set_language_standard(${target})
    target_compile_definitions(${target} PRIVATE
      "ADLPLUG_${ADLplug_CHIP}"
      # The library's code fills its fixed-width name fields with strncpy.
      $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/sources")
    target_include_directories(${target} SYSTEM PRIVATE
      "${ADLplug_FUZZ_LIBRARY}/src" "${ADLplug_FUZZ_LIBRARY}/include")
  endforeach()
  adlplug_own_sources(replay.cc "${name}.cc" ${arg_OWN_SOURCES})

  if(ADLplug_BUILD_TESTS)
    set(inputs ${arg_SEEDS})
    set(regressions "${CMAKE_CURRENT_SOURCE_DIR}/regressions/${ADLplug_FUZZ_CHIP}/${name}")
    if(IS_DIRECTORY "${regressions}")
      list(APPEND inputs "${regressions}")
    endif()
    add_test(NAME "fuzz.${name}" COMMAND "ADLplug_fuzz_${name}_replay" ${inputs})
    set_tests_properties("fuzz.${name}" PROPERTIES TIMEOUT 900 LABELS fuzz)
  endif()
endfunction()

# F2: the bank and instrument files that people open.
adlplug_fuzz_target(bank_file
  OWN_SOURCES "${PROJECT_SOURCE_DIR}/sources/${ADLplug_FUZZ_CHIP}/adl/instrument.cc"
  OTHER_SOURCES "${ADLplug_FUZZ_FILE_SOURCE}"
  SEEDS ${ADLplug_FUZZ_BANK_SEEDS})
