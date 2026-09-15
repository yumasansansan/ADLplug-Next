# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   cmake -DRENDER=<tool> -DPLUGIN=<plugin.vst3> -DCORES=<n:seconds,...>
#         -DPROFDATA=<llvm-profdata> -DPROFILE=<file> -DWORK=<directory>
#         -P cmake/PGOTrain.cmake
#
# Trains profile-guided optimisation (cmake/PGO.cmake): the instrumented plugin
# renders the fixed sequence of tools/render with each core, for the given
# seconds after a warm-up of 5 seconds, and the profiles of the renders are
# merged into PROFILE. The renders run again only when the render tool, the
# plugin or the cores differ from those of the last training; otherwise the
# profile is left as it is, so that nothing is compiled again. A render that
# fails, or plays silence, stops the build.

foreach(variable IN ITEMS RENDER PLUGIN CORES PROFDATA PROFILE WORK)
  if(NOT DEFINED ${variable})
    message(FATAL_ERROR "PGOTrain.cmake needs -D${variable}")
  endif()
endforeach()

# What the training depends on: the programs, and the cores with their seconds.
file(GLOB_RECURSE plugin_files LIST_DIRECTORIES FALSE "${PLUGIN}/*")
list(SORT plugin_files)
set(fingerprint "${CORES}")
foreach(file IN ITEMS "${RENDER}" ${plugin_files})
  file(SHA256 "${file}" hash)
  string(APPEND fingerprint "\n${hash}")
endforeach()
string(SHA256 fingerprint "${fingerprint}")
set(stamp "${WORK}/fingerprint")
if(EXISTS "${PROFILE}" AND EXISTS "${stamp}")
  file(READ "${stamp}" last)
  if(last STREQUAL fingerprint)
    message(STATUS "PGO: the profile is up to date")
    return()
  endif()
endif()

file(REMOVE_RECURSE "${WORK}/raw")
file(MAKE_DIRECTORY "${WORK}/raw")
set(ENV{LLVM_PROFILE_FILE} "${WORK}/raw/%p-%m.profraw")
string(TIMESTAMP start "%s")
string(REPLACE "," ";" cores "${CORES}")
foreach(core IN LISTS cores)
  string(REPLACE ":" ";" fields "${core}")
  list(GET fields 0 number)
  list(GET fields 1 seconds)
  message(STATUS "PGO: rendering ${seconds} s with core ${number}")
  execute_process(
    COMMAND "${RENDER}" "${PLUGIN}" "${WORK}/render.f32" "${seconds}" 5000
      --emulator "${number}" --require-sound
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "PGO: the render with core ${number} failed (${result}):\n${output}")
  endif()
endforeach()
unset(ENV{LLVM_PROFILE_FILE})
file(REMOVE "${WORK}/render.f32")

file(GLOB raw "${WORK}/raw/*.profraw")
if(NOT raw)
  message(FATAL_ERROR "PGO: the renders wrote no profiles to ${WORK}/raw")
endif()
execute_process(
  COMMAND "${PROFDATA}" merge -o "${PROFILE}.new" ${raw}
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE output)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "PGO: llvm-profdata could not merge the profiles:\n${output}")
endif()
file(RENAME "${PROFILE}.new" "${PROFILE}")
file(WRITE "${stamp}" "${fingerprint}")
string(TIMESTAMP end "%s")
math(EXPR elapsed "${end} - ${start}")
list(LENGTH raw count)
message(STATUS "PGO: trained with ${count} renders in ${elapsed} s")
