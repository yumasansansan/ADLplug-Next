//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

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
    AdlplugAudioProcessorEditor(AdlplugAudioProcessor &, Parameter_Block &);
    ~AdlplugAudioProcessorEditor() override;

    //==========================================================================
    void paint(Graphics &) override;
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
