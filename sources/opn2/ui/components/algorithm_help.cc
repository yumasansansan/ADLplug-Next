/*
  ==============================================================================

  Originally emitted by the Projucer GUI editor (JUCE 5.x, (c) 2017 ROLI Ltd.).

  The .jucer project was retired during the JUCE 9 migration, so this file is
  now maintained by hand. The "//[...]" markers left behind are ordinary
  section comments and no longer carry any special meaning -- edit anywhere.

  Modified for ADLplug-Next. The modifications are distributed under the
  GNU GPL v3 or later (see the accompanying file LICENSE).

  ==============================================================================
*/

//[Headers] You can add your own extra header files here...
#include "algorithms.h"
#include "ui/components/algorithm_component.h"
#include "ui/utility/legacy_font.h"
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

    component = std::make_unique<Algorithm_Component> (Algorithms::algorithm_0);
    addAndMakeVisible (component.get());
    component->setName ("new component");

    component->setBounds (16, 32, 120, 152);

    component2 = std::make_unique<Algorithm_Component> (Algorithms::algorithm_1);
    addAndMakeVisible (component2.get());
    component2->setName ("new component");

    component2->setBounds (168, 32, 120, 152);

    component3 = std::make_unique<Algorithm_Component> (Algorithms::algorithm_4);
    addAndMakeVisible (component3.get());
    component3->setName ("new component");

    component3->setBounds (16, 216, 120, 90);

    component4 = std::make_unique<Algorithm_Component> (Algorithms::algorithm_5);
    addAndMakeVisible (component4.get());
    component4->setName ("new component");

    component4->setBounds (168, 216, 120, 90);

    component5 = std::make_unique<Algorithm_Component> (Algorithms::algorithm_6);
    addAndMakeVisible (component5.get());
    component5->setName ("new component");

    component5->setBounds (320, 216, 120, 90);

    component6 = std::make_unique<Algorithm_Component> (Algorithms::algorithm_7);
    addAndMakeVisible (component6.get());
    component6->setName ("new component");

    component6->setBounds (472, 216, 160, 90);

    component7 = std::make_unique<Algorithm_Component> (Algorithms::algorithm_2);
    addAndMakeVisible (component7.get());
    component7->setName ("new component");

    component7->setBounds (320, 32, 120, 152);

    component8 = std::make_unique<Algorithm_Component> (Algorithms::algorithm_3);
    addAndMakeVisible (component8.get());
    component8->setName ("new component");

    component8->setBounds (472, 32, 120, 152);


    //[UserPreSize]
    double s = 18.0;
    component->scale(s);
    component2->scale(s);
    component3->scale(s);
    component4->scale(s);
    component5->scale(s);
    component6->scale(s);
    component7->scale(s);
    component8->scale(s);
    //[/UserPreSize]

    setSize (650, 318);


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
    component7 = nullptr;
    component8 = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void Algorithm_Help::paint (Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    {
        int x = 16, y = 4, width = 120, height = 30;
        String text (TRANS("Algorithm 1"));
        Colour fillColour = Colours::aliceblue;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (legacy_font (20.0f).withStyle ("Bold Italic"));
        g.drawText (text, x, y, width, height,
                    Justification::centred, true);
    }

    {
        int x = 168, y = 4, width = 120, height = 30;
        String text (TRANS("Algorithm 2"));
        Colour fillColour = Colours::aliceblue;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (legacy_font (20.0f).withStyle ("Bold Italic"));
        g.drawText (text, x, y, width, height,
                    Justification::centred, true);
    }

    {
        int x = 16, y = 188, width = 120, height = 30;
        String text (TRANS("Algorithm 5"));
        Colour fillColour = Colours::aliceblue;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (legacy_font (20.0f).withStyle ("Bold Italic"));
        g.drawText (text, x, y, width, height,
                    Justification::centred, true);
    }

    {
        int x = 168, y = 188, width = 120, height = 30;
        String text (TRANS("Algorithm 6"));
        Colour fillColour = Colours::aliceblue;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (legacy_font (20.0f).withStyle ("Bold Italic"));
        g.drawText (text, x, y, width, height,
                    Justification::centred, true);
    }

    {
        int x = 320, y = 188, width = 120, height = 30;
        String text (TRANS("Algorithm 7"));
        Colour fillColour = Colours::aliceblue;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (legacy_font (20.0f).withStyle ("Bold Italic"));
        g.drawText (text, x, y, width, height,
                    Justification::centred, true);
    }

    {
        int x = 472, y = 188, width = 160, height = 30;
        String text (TRANS("Algorithm 8"));
        Colour fillColour = Colours::aliceblue;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (legacy_font (20.0f).withStyle ("Bold Italic"));
        g.drawText (text, x, y, width, height,
                    Justification::centred, true);
    }

    {
        int x = 320, y = 4, width = 120, height = 30;
        String text (TRANS("Algorithm 3"));
        Colour fillColour = Colours::aliceblue;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (legacy_font (20.0f).withStyle ("Bold Italic"));
        g.drawText (text, x, y, width, height,
                    Justification::centred, true);
    }

    {
        int x = 472, y = 4, width = 120, height = 30;
        String text (TRANS("Algorithm 4"));
        Colour fillColour = Colours::aliceblue;
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.setFont (legacy_font (20.0f).withStyle ("Bold Italic"));
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
