# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
# The resampler of MediaPerch (thirdparty/MediaPerch), which is the part of that
# project this plugin uses: a polyphase rational resampler, the filter design it
# is made of, and the transform the design measures itself with. Three
# translation units, which need nothing but the standard library.
#
# MediaPerch builds them as a module of its own -- a static library plus a
# wrapper for its module ABI -- with its own compiler flags. Here the three are
# compiled into a library of this build instead, with this build's flags and this
# build's standard, and everything else in that repository is left alone: the
# module wrapper, its ABI, and the rest of the framework. That is what the module
# layout is for, and it is why this takes a submodule rather than a copy: a fault
# found here is fixed there, once.
#
# This is not other people's code, and it is not treated as such: the same
# warning flags as this project's own sources (adlplug_own_sources), the same
# coverage, and the same clang-tidy checks (ci/tidy.sh names it beside sources/).
# The includes are not SYSTEM, so that what its headers say is heard.

set(ADLplug_MEDIAPERCH "${PROJECT_SOURCE_DIR}/thirdparty/MediaPerch")

if(NOT EXISTS "${ADLplug_MEDIAPERCH}/modules/dsp/resample/resample.cpp")
  message(FATAL_ERROR
    "thirdparty/MediaPerch is not a checked-out submodule; the submodules are "
    "part of the sources: git submodule update --init --recursive")
endif()

add_library(mediaperch-resample STATIC
  "${ADLplug_MEDIAPERCH}/modules/dsp/resample/resample.cpp"
  "${ADLplug_MEDIAPERCH}/modules/dsp/resample/design.cpp"
  "${ADLplug_MEDIAPERCH}/modules/shared/transform/transform.cpp")
adlplug_set_language_standard(mediaperch-resample)
target_include_directories(mediaperch-resample PUBLIC
  "${ADLplug_MEDIAPERCH}/modules/dsp/resample"
  "${ADLplug_MEDIAPERCH}/modules/shared/transform")
set_property(TARGET mediaperch-resample PROPERTY POSITION_INDEPENDENT_CODE ON)

adlplug_own_sources(
  "${ADLplug_MEDIAPERCH}/modules/dsp/resample/resample.cpp"
  "${ADLplug_MEDIAPERCH}/modules/dsp/resample/design.cpp"
  "${ADLplug_MEDIAPERCH}/modules/shared/transform/transform.cpp")
