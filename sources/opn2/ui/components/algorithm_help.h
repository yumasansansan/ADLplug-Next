/*
  ==============================================================================

  Originally emitted by the Projucer GUI editor (JUCE 5.x, (c) 2017 ROLI Ltd.).

  The .jucer project was retired during the JUCE 9 migration, so this file is
  now maintained by hand. The "//[...]" markers left behind are ordinary
  section comments and no longer carry any special meaning -- edit anywhere.

  SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
  SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
  SPDX-License-Identifier: BSL-1.0 AND GPL-3.0-or-later

  This file comes from ADLplug and was modified for ADLplug-Next. ADLplug gave
  it no notice of its own; it was under ADLplug's license, the Boost Software
  License 1.0 (LICENSES/BSL-1.0.txt). The SPDX lines name the copyright holders
  and licenses in the machine-readable form of the REUSE specification:
  ADLplug's part is under the Boost Software License 1.0, and ADLplug-Next's
  changes are under the GNU General Public License, version 3 or any later
  version (LICENSES/GPL-3.0-or-later.txt).

  ==============================================================================
*/

#pragma once

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"
class Algorithm_Component;
//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Projucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class Algorithm_Help  : public Component
{
public:
    //==============================================================================
    Algorithm_Help ();
    ~Algorithm_Help() override;

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    //[/UserMethods]

    void paint (Graphics& g) override;
    void resized() override;



private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    //[/UserVariables]

    //==============================================================================
    std::unique_ptr<Algorithm_Component> component;
    std::unique_ptr<Algorithm_Component> component2;
    std::unique_ptr<Algorithm_Component> component3;
    std::unique_ptr<Algorithm_Component> component4;
    std::unique_ptr<Algorithm_Component> component5;
    std::unique_ptr<Algorithm_Component> component6;
    std::unique_ptr<Algorithm_Component> component7;
    std::unique_ptr<Algorithm_Component> component8;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Algorithm_Help)
};

//[EndFile] You can add extra defines here...
//[/EndFile]
