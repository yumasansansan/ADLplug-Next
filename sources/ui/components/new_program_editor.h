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

#pragma once

//[Headers]     -- You can add your own extra header files here --
#include "JuceHeader.h"
#include "adl/instrument.h"
//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Projucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class New_Program_Editor  : public Component,
                            public Button::Listener,
                            public ComboBox::Listener
{
public:
    //==============================================================================
    New_Program_Editor ();
    ~New_Program_Editor() override;

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    void set_current(const Bank_Id &id, unsigned pgm);

    struct Result {
        Bank_Id bank;
        unsigned pgm = 0;
    };

    std::function<void(const Result &result)> on_ok;
    std::function<void()> on_cancel;
    //[/UserMethods]

    void paint (Graphics& g) override;
    void resized() override;
    void buttonClicked (Button* buttonThatWasClicked) override;
    void comboBoxChanged (ComboBox* comboBoxThatHasChanged) override;



private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    //[/UserVariables]

    //==============================================================================
    std::unique_ptr<Label> label;
    std::unique_ptr<Label> label2;
    std::unique_ptr<TextButton> btn_ok;
    std::unique_ptr<TextButton> btn_cancel;
    std::unique_ptr<ComboBox> cb_pgm_kind;
    std::unique_ptr<TextEditor> edt_pgm_num;
    std::unique_ptr<TextEditor> edt_bank_msb;
    std::unique_ptr<TextEditor> edt_bank_lsb;
    std::unique_ptr<Label> label3;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (New_Program_Editor)
};

//[EndFile] You can add extra defines here...
//[/EndFile]
