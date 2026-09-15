# SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file is part of ADLplug-Next. The SPDX lines name its copyright holder
# and its license, in the machine-readable form of the REUSE specification: the
# GNU General Public License, version 3 or any later version
# (LICENSES/GPL-3.0-or-later.txt).
#
# Included by CMake at the end of the project() calls of libADLMIDI and
# libOPNMIDI (CMAKE_PROJECT_<PROJECT-NAME>_INCLUDE, set in cmake/ADLMIDI.cmake),
# in the directory of the library. It defers adlplug_restore_upstream_flags()
# to the end of that directory, after the library has changed its compiler and
# linker flags, so that the library is built with the flags of the rest of the
# build.

cmake_language(DEFER CALL adlplug_restore_upstream_flags)
