//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
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

#include "JuceHeader.h"
#include <memory>
class AdlplugAudioProcessor;
class Custom_Look_And_Feel;
class Main_Component;
struct Parameter_Block;
class Configuration;

//==============================================================================
/**
 */
class AdlplugAudioProcessorEditor : public AudioProcessorEditor {
public:
    AdlplugAudioProcessorEditor(AdlplugAudioProcessor &p, Parameter_Block &pb);
    ~AdlplugAudioProcessorEditor() override;

    //==========================================================================
    void paint(Graphics &g) override;
    void resized() override;

private:
    AdlplugAudioProcessor &proc_;
    // Declared first, destroyed last: the main component refers to it.
    std::unique_ptr<Configuration> conf_;
    std::unique_ptr<Custom_Look_And_Feel> lnf_;
    std::unique_ptr<Main_Component> main_;
    std::unique_ptr<TooltipWindow> tooltip_window_;
    // Declared last, destroyed first: it calls into the main component.
    std::unique_ptr<Timer> notification_timer_;

    void process_notifications();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdlplugAudioProcessorEditor)
};
