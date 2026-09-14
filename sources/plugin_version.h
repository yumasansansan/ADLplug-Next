//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018-2019 Jean Pierre Cimalando
// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: BSL-1.0 AND GPL-3.0-or-later
//
// This file comes from ADLplug and was modified for ADLplug-Next. The notice at
// the top is ADLplug's; the LICENSE it names was ADLplug's copy of the Boost
// Software License, now LICENSES/BSL-1.0.txt. The SPDX lines name the copyright
// holders and licenses in the machine-readable form of the REUSE specification:
// ADLplug's code is under the Boost Software License 1.0, and ADLplug-Next's
// changes are under the GNU General Public License, version 3 or any later
// version (LICENSES/GPL-3.0-or-later.txt).

#pragma once
// JucePlugin_VersionString arrives as a compile definition from
// juce_add_plugin(); the Projucer AppConfig.h it used to come from is gone.
#include <JuceHeader.h>

#define ADLplug_Version JucePlugin_VersionString
#define ADLplug_VersionFinal 1

#if !ADLplug_VersionFinal
#   define ADLplug_VersionExtra "Beta 5"
#   define ADLplug_SemVer JucePlugin_VersionString "-beta.5"
#else
#   define ADLplug_VersionExtra ""
#   define ADLplug_SemVer JucePlugin_VersionString
#endif
