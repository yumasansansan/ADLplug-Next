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

//[Headers] You can add your own extra header files here...
#include "algorithms.h"
#include "ui/components/algorithm_component.h"
//[/Headers]

#include "algorithm_help.h"

#include <memory>


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
Algorithm_Help::Algorithm_Help ()
{
    //[Constructor_pre] You can add your own custom stuff here..
    //[/Constructor_pre]

    component = std::make_unique<Algorithm_Component> (Algorithms::algorithm_2fm);
    addAndMakeVisible (component.get());
    component->setName ("new component");

    component->setBounds (16, 32, 120, 104);

    component2 = std::make_unique<Algorithm_Component> (Algorithms::algorithm_2am);
    addAndMakeVisible (component2.get());
    component2->setName ("new component");

    component2->setBounds (168, 32, 120, 104);

    component3 = std::make_unique<Algorithm_Component> (Algorithms::algorithm_4fmfm);
    addAndMakeVisible (component3.get());
    component3->setName ("new component");

    component3->setBounds (16, 168, 120, 200);

    component4 = std::make_unique<Algorithm_Component> (Algorithms::algorithm_4amfm);
    addAndMakeVisible (component4.get());
    component4->setName ("new component");

    component4->setBounds (168, 168, 120, 200);

    component5 = std::make_unique<Algorithm_Component> (Algorithms::algorithm_4fmam);
    addAndMakeVisible (component5.get());
    component5->setName ("new component");

    component5->setBounds (320, 168, 120, 200);

    component6 = std::make_unique<Algorithm_Component> (Algorithms::algorithm_4amam);
    addAndMakeVisible (component6.get());
    component6->setName ("new component");

    component6->setBounds (472, 168, 160, 200);


    //[UserPreSize]
    //[/UserPreSize]

    setSize (650, 380);


    //[Constructor] You can add your own custom stuff here..
    //[/Constructor]
}

Algorithm_Help::~Algorithm_Help()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    component = nullptr;
    component2 = nullptr;
    component3 = nullptr;
    component4 = nullptr;
    component5 = nullptr;
    component6 = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void Algorithm_Help::paint (Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    {
        const int x = 16, y = 4, width = 120, height = 30;
        const String text (TRANS("FM"));
        const Colour fillColour = Colours::aliceblue;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (FontOptions (20.0f).withStyle ("Bold Italic"));
        g.drawText (text, x, y, width, height,
                    Justification::centred, true);
    }

    {
        const int x = 168, y = 4, width = 120, height = 30;
        const String text (TRANS("AM"));
        const Colour fillColour = Colours::aliceblue;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (FontOptions (20.0f).withStyle ("Bold Italic"));
        g.drawText (text, x, y, width, height,
                    Justification::centred, true);
    }

    {
        const int x = 16, y = 140, width = 120, height = 30;
        const String text (TRANS("FM-FM"));
        const Colour fillColour = Colours::aliceblue;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (FontOptions (20.0f).withStyle ("Bold Italic"));
        g.drawText (text, x, y, width, height,
                    Justification::centred, true);
    }

    {
        const int x = 168, y = 140, width = 120, height = 30;
        const String text (TRANS("AM-FM"));
        const Colour fillColour = Colours::aliceblue;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (FontOptions (20.0f).withStyle ("Bold Italic"));
        g.drawText (text, x, y, width, height,
                    Justification::centred, true);
    }

    {
        const int x = 320, y = 140, width = 120, height = 30;
        const String text (TRANS("FM-AM"));
        const Colour fillColour = Colours::aliceblue;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (FontOptions (20.0f).withStyle ("Bold Italic"));
        g.drawText (text, x, y, width, height,
                    Justification::centred, true);
    }

    {
        const int x = 472, y = 140, width = 160, height = 30;
        const String text (TRANS("AM-AM"));
        const Colour fillColour = Colours::aliceblue;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (FontOptions (20.0f).withStyle ("Bold Italic"));
        g.drawText (text, x, y, width, height,
                    Justification::centred, true);
    }

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void Algorithm_Help::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    //[UserResized] Add your own custom resize handling here..
    //[/UserResized]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...
//[/MiscUserCode]


//==============================================================================


//[EndFile] You can add extra defines here...
//[/EndFile]
