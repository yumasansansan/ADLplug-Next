/*
  ==============================================================================

  Originally emitted by the Projucer GUI editor (JUCE 5.x, (c) 2017 ROLI Ltd.).

  The .jucer project was retired during the JUCE 9 migration, so this file is
  now maintained by hand. The "//[...]" markers left behind are ordinary
  section comments and no longer carry any special meaning -- edit anywhere.

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
    ~Algorithm_Help();

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
