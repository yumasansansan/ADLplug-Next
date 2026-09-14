# SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
# SPDX-License-Identifier: BSL-1.0
#
# This file comes from ADLplug, which gave it no notice of its own; it is under
# ADLplug's license, the Boost Software License 1.0 (LICENSES/BSL-1.0.txt). The
# SPDX lines name its copyright holder and license in the machine-readable form
# of the REUSE specification.
#
# Options which take different default values depending on system
#
#
# Usage example:
#   system_option(QUUX ON "Windows" OFF "Darwin" OFF)
#

macro(system_option NAME DESCRIPTION VALUE)
  set(_args ${ARGN})
  set(_value "${VALUE}")
  while(_args)
    list(GET _args 0 _system)
    list(REMOVE_AT _args 0)
    if(CMAKE_SYSTEM_NAME STREQUAL "${_system}")
      list(GET _args 0 _value)
    endif()
    list(REMOVE_AT _args 0)
    unset(_system)
  endwhile()
  option("${NAME}" "${DESCRIPTION}" "${_value}")
  unset(_args)
  unset(_value)
endmacro()
