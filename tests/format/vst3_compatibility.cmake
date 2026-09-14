# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
# The test format.vst3.compatibility (tests/CMakeLists.txt): the moduleinfo.json
# of the VST3 plugin lists the classes of upstream ADLplug among those that the
# component of the plugin replaces, by their IDs as hosts read them, in the
# notation of class IDs (see JUCE_VST3_COMPATIBLE_CLASSES in CMakeLists.txt).
#
#   cmake -Dmoduleinfo=<file> -Dnew=<ID> -Dold=<ID>,<ID>... -P vst3_compatibility.cmake

cmake_minimum_required(VERSION 3.25)

foreach(variable IN ITEMS moduleinfo new old)
  if(NOT DEFINED ${variable})
    message(FATAL_ERROR "-D${variable}= is missing")
  endif()
endforeach()
if(NOT EXISTS "${moduleinfo}")
  message(FATAL_ERROR "${moduleinfo} does not exist")
endif()

file(READ "${moduleinfo}" json)
# moduleinfo.json is JSON5; string(JSON) needs the commas after the last
# elements of objects and arrays gone.
string(REGEX REPLACE ",([ \t\r\n]*)\\]" "\\1]" json "${json}")
string(REGEX REPLACE ",([ \t\r\n]*)}" "\\1}" json "${json}")

string(JSON entries ERROR_VARIABLE error LENGTH "${json}" Compatibility)
if(error)
  message(FATAL_ERROR "${moduleinfo} lists no compatibility: ${error}")
endif()

string(TOUPPER "${new}" new)
set(found FALSE)
set(listed "")
if(entries GREATER 0)
  math(EXPR last_entry "${entries} - 1")
  foreach(entry RANGE ${last_entry})
    string(JSON entry_new GET "${json}" Compatibility ${entry} New)
    string(TOUPPER "${entry_new}" entry_new)
    if(NOT entry_new STREQUAL new)
      continue()
    endif()
    set(found TRUE)
    string(JSON count LENGTH "${json}" Compatibility ${entry} Old)
    if(count GREATER 0)
      math(EXPR last_old "${count} - 1")
      foreach(index RANGE ${last_old})
        string(JSON id GET "${json}" Compatibility ${entry} Old ${index})
        string(TOUPPER "${id}" id)
        list(APPEND listed "${id}")
      endforeach()
    endif()
  endforeach()
endif()
if(NOT found)
  message(FATAL_ERROR "${moduleinfo} lists nothing that the class ${new} replaces")
endif()
list(JOIN listed ", " shown)
message(STATUS "The class ${new} replaces ${shown}")

string(REPLACE "," ";" old "${old}")
set(missing "")
foreach(id IN LISTS old)
  string(TOUPPER "${id}" id)
  if(NOT id IN_LIST listed)
    list(APPEND missing "${id}")
  endif()
endforeach()
if(missing)
  list(JOIN missing ", " missing)
  message(FATAL_ERROR "${moduleinfo} does not list the classes ${missing}")
endif()
