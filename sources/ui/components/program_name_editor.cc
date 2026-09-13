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
#include "midi/insnames.h"
#include <format>

#include <memory>
//[/Headers]

#include "program_name_editor.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
Program_Name_Editor::Program_Name_Editor ()
{
    //[Constructor_pre] You can add your own custom stuff here..
    //[/Constructor_pre]

    edt_pgm_name = std::make_unique<TextEditor> ("new text editor");
    addAndMakeVisible (edt_pgm_name.get());
    edt_pgm_name->setMultiLine (false);
    edt_pgm_name->setReturnKeyStartsNewLine (false);
    edt_pgm_name->setReadOnly (false);
    edt_pgm_name->setScrollbarsShown (true);
    edt_pgm_name->setCaretVisible (true);
    edt_pgm_name->setPopupMenuEnabled (true);
    edt_pgm_name->setText (String());

    edt_pgm_name->setBounds (88, 106, 200, 24);

    edt_bank_name = std::make_unique<TextEditor> ("new text editor");
    addAndMakeVisible (edt_bank_name.get());
    edt_bank_name->setMultiLine (false);
    edt_bank_name->setReturnKeyStartsNewLine (false);
    edt_bank_name->setReadOnly (false);
    edt_bank_name->setScrollbarsShown (true);
    edt_bank_name->setCaretVisible (true);
    edt_bank_name->setPopupMenuEnabled (true);
    edt_bank_name->setText (String());

    edt_bank_name->setBounds (88, 34, 200, 24);

    label = std::make_unique<Label> ("new label",
                            TRANS("Program"));
    addAndMakeVisible (label.get());
    label->setFont (FontOptions (15.0f).withStyle ("Regular"));
    label->setJustificationType (Justification::centredLeft);
    label->setEditable (false, false, false);
    label->setColour (Label::textColourId, Colours::aliceblue);
    label->setColour (TextEditor::textColourId, Colours::black);
    label->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    label->setBounds (8, 80, 64, 24);

    label2 = std::make_unique<Label> ("new label",
                             TRANS("Bank"));
    addAndMakeVisible (label2.get());
    label2->setFont (FontOptions (15.0f).withStyle ("Regular"));
    label2->setJustificationType (Justification::centredLeft);
    label2->setEditable (false, false, false);
    label2->setColour (Label::textColourId, Colours::aliceblue);
    label2->setColour (TextEditor::textColourId, Colours::black);
    label2->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    label2->setBounds (8, 8, 64, 24);

    edt_pgm_id = std::make_unique<TextEditor> ("new text editor");
    addAndMakeVisible (edt_pgm_id.get());
    edt_pgm_id->setMultiLine (false);
    edt_pgm_id->setReturnKeyStartsNewLine (false);
    edt_pgm_id->setReadOnly (true);
    edt_pgm_id->setScrollbarsShown (true);
    edt_pgm_id->setCaretVisible (false);
    edt_pgm_id->setPopupMenuEnabled (true);
    edt_pgm_id->setText (TRANS("M000"));

    edt_pgm_id->setBounds (88, 80, 60, 24);

    edt_bank_id = std::make_unique<TextEditor> ("new text editor");
    addAndMakeVisible (edt_bank_id.get());
    edt_bank_id->setMultiLine (false);
    edt_bank_id->setReturnKeyStartsNewLine (false);
    edt_bank_id->setReadOnly (true);
    edt_bank_id->setScrollbarsShown (true);
    edt_bank_id->setCaretVisible (false);
    edt_bank_id->setPopupMenuEnabled (true);
    edt_bank_id->setText (TRANS("000:000"));

    edt_bank_id->setBounds (88, 8, 60, 24);

    btn_ok = std::make_unique<TextButton> ("new button");
    addAndMakeVisible (btn_ok.get());
    btn_ok->setButtonText (TRANS("OK"));
    btn_ok->addListener (this);

    btn_ok->setBounds (130, 150, 70, 24);

    btn_cancel = std::make_unique<TextButton> ("new button");
    addAndMakeVisible (btn_cancel.get());
    btn_cancel->setButtonText (TRANS("Cancel"));
    btn_cancel->addListener (this);

    btn_cancel->setBounds (218, 150, 70, 24);


    //[UserPreSize]
#if defined(JUCE_MAC)
    {
        Rectangle<int> bounds_ok = btn_ok->getBounds();
        Rectangle<int> bounds_cancel = btn_cancel->getBounds();
        btn_ok->setBounds(bounds_cancel);
        btn_cancel->setBounds(bounds_ok);
    }
#endif

    edt_bank_id->setJustification(Justification::centred);
    edt_pgm_id->setJustification(Justification::centred);

    Colour label_color = findColour(TextEditor::backgroundColourId).contrasting(0.5f);
    edt_bank_name->setTextToShowWhenEmpty("<Untitled bank>", label_color);
    edt_pgm_name->setTextToShowWhenEmpty("<Untitled program>", label_color);
    //[/UserPreSize]

    setSize (300, 182);


    //[Constructor] You can add your own custom stuff here..
    //[/Constructor]
}

Program_Name_Editor::~Program_Name_Editor()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    edt_pgm_name = nullptr;
    edt_bank_name = nullptr;
    label = nullptr;
    label2 = nullptr;
    edt_pgm_id = nullptr;
    edt_bank_id = nullptr;
    btn_ok = nullptr;
    btn_cancel = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void Program_Name_Editor::paint (Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (Colour (0xff323e44));

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void Program_Name_Editor::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    //[UserResized] Add your own custom resize handling here..
    //[/UserResized]
}

void Program_Name_Editor::buttonClicked (Button* buttonThatWasClicked)
{
    //[UserbuttonClicked_Pre]
    //[/UserbuttonClicked_Pre]

    if (buttonThatWasClicked == btn_ok.get())
    {
        //[UserButtonCode_btn_ok] -- add your button handler code here..
        if (on_ok) {
            Result result;
            result.bank = bank_;
            result.pgm = pgm_;
            result.bank_name = edt_bank_name->getText();
            result.pgm_name = edt_pgm_name->getText();
            on_ok(result);
        }
        //[/UserButtonCode_btn_ok]
    }
    else if (buttonThatWasClicked == btn_cancel.get())
    {
        //[UserButtonCode_btn_cancel] -- add your button handler code here..
        if (on_cancel)
            on_cancel();
        //[/UserButtonCode_btn_cancel]
    }

    //[UserbuttonClicked_Post]
    //[/UserbuttonClicked_Post]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...
void Program_Name_Editor::set_program(
    Bank_Id bank, unsigned pgm, const String &bank_name, const String &pgm_name)
{
    bank_ = bank;
    pgm_ = pgm;

    edt_bank_id->setText(std::format("{:03d}:{:03d}", bank.msb, bank.lsb));
    edt_pgm_id->setText(std::format("{:c}{:03d}", bank.percussive ? 'P' : 'M', pgm));
    edt_bank_name->setText(bank_name);
    edt_pgm_name->setText(pgm_name);

    const Midi_Db &db = midi_db();
    const Midi_Program_Ex *ex = db.find_ex(bank.msb, bank.lsb, pgm + (bank.percussive ? 128 : 0));
    const char *name = ex ? ex->name : bank.percussive ? db.perc(pgm).name : db.inst(pgm);

    Colour label_color = findColour(TextEditor::backgroundColourId).contrasting(0.5f);
    edt_pgm_name->setTextToShowWhenEmpty(name, label_color);
}
//[/MiscUserCode]


//==============================================================================


//[EndFile] You can add extra defines here...
//[/EndFile]
