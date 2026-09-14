# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
#   cmake -DTTL=<bundle>/dsp.ttl -P cmake/LV2FloatLiterals.cmake
#
# JUCE's LV2 wrapper gives every parameter the range atom:Float, then writes
# its default, minimum and maximum, and the values of its scale points, with
# the stream operator for floats, which leaves a whole number without a
# decimal point (juce_audio_plugin_client_LV2.cpp). Turtle reads "63" as an
# integer, and lv2lint fails the parameter: "lv2:maximum not a float". This
# script rewrites such numbers as decimals, "63.0". Each is followed by spaces
# and the ";" or "." that ends its statement, or by the end of the line, never
# by a decimal point. Running the script again changes nothing.

if(NOT DEFINED TTL)
  message(FATAL_ERROR "LV2FloatLiterals.cmake: set TTL to the dsp.ttl to rewrite")
endif()

file(READ "${TTL}" text)
string(REGEX REPLACE
  "(lv2:default|lv2:minimum|lv2:maximum|rdf:value)([ \t]+)([-+]?[0-9]+)([ \t]+[;.]|[ \t]*;|[ \t\r]*\n)"
  "\\1\\2\\3.0\\4" rewritten "${text}")
if(NOT rewritten STREQUAL text)
  file(WRITE "${TTL}" "${rewritten}")
endif()
