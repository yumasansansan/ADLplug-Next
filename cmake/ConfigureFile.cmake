# SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
# SPDX-License-Identifier: BSL-1.0
#
# This file comes from ADLplug, which gave it no notice of its own; it is under
# ADLplug's license, the Boost Software License 1.0 (LICENSES/BSL-1.0.txt). The
# SPDX lines name its copyright holder and license in the machine-readable form
# of the REUSE specification.
#
# ConfigureFile.cmake -- cmake -P script to run configure_file

if(NOT CONFIGURE_FILE_INPUT)
  message(FATAL_ERROR "required variable CONFIGURE_FILE_INPUT not given")
endif()
if(NOT CONFIGURE_FILE_OUTPUT)
  message(FATAL_ERROR "required variable CONFIGURE_FILE_OUTPUT not given")
endif()
configure_file("${CONFIGURE_FILE_INPUT}" "${CONFIGURE_FILE_OUTPUT}")
