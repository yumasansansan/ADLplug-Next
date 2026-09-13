//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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
