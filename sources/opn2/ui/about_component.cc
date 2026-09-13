/*
  ==============================================================================

  Originally emitted by the Projucer GUI editor (JUCE 5.x, (c) 2017 ROLI Ltd.).

  The .jucer project was retired during the JUCE 9 migration, so this file is
  now maintained by hand. The "//[...]" markers left behind are ordinary
  section comments and no longer carry any special meaning -- edit anywhere.

  ==============================================================================
*/

//[Headers] You can add your own extra header files here...
#include "plugin_version.h"
//[/Headers]

#include "about_component.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
About_Component::About_Component ()
{
    //[Constructor_pre] You can add your own custom stuff here..
    //[/Constructor_pre]

    hyperlinkButton.reset (new HyperlinkButton (TRANS("Home page"),
                                                URL ("https://github.com/jpcima/ADLplug")));
    addAndMakeVisible (hyperlinkButton.get());
    hyperlinkButton->setTooltip (TRANS("https://github.com/jpcima/ADLplug"));
    hyperlinkButton->setButtonText (TRANS("Home page"));

    hyperlinkButton->setBounds (8, 56, 88, 24);

    label.reset (new Label ("new label",
                            CharPointer_UTF8 ("This program is free software developed by Jean Pierre Cimalando. \xc2\xa9 2018\n"
                            "Many thanks to people who make this program possible.")));
    addAndMakeVisible (label.get());
    label->setFont (Font (15.0f, Font::plain).withTypefaceStyle ("Regular"));
    label->setJustificationType (Justification::centredLeft);
    label->setEditable (false, false, false);
    label->setColour (Label::textColourId, Colours::aliceblue);
    label->setColour (TextEditor::textColourId, Colours::black);
    label->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    label->setBounds (8, 88, 488, 40);

    label2.reset (new Label ("new label",
                             CharPointer_UTF8 ("Vitaly Novichkov for the OPNMIDI library\n"
                             "Joel Yliluoma for the original ADLMIDI software\n"
                             "Alexey Khokholov for Nuked OPN2\n"
                             "MAMEDev and contributors for MAME YM2612\n"
                             "St\xc3\xa9phane Dallongeville and Shay Green for GENS OPN2")));
    addAndMakeVisible (label2.get());
    label2->setFont (Font (15.0f, Font::plain).withTypefaceStyle ("Regular"));
    label2->setJustificationType (Justification::centredLeft);
    label2->setEditable (false, false, false);
    label2->setColour (Label::textColourId, Colours::aliceblue);
    label2->setColour (TextEditor::textColourId, Colours::black);
    label2->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    label2->setBounds (8, 136, 488, 80);

    lbl_prog_version.reset (new Label ("new label",
                                       TRANS("Foobar 1.0")));
    addAndMakeVisible (lbl_prog_version.get());
    lbl_prog_version->setFont (Font (15.0f, Font::plain).withTypefaceStyle ("Bold"));
    lbl_prog_version->setJustificationType (Justification::centredLeft);
    lbl_prog_version->setEditable (false, false, false);
    lbl_prog_version->setColour (Label::textColourId, Colours::aliceblue);
    lbl_prog_version->setColour (TextEditor::textColourId, Colours::black);
    lbl_prog_version->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    lbl_prog_version->setBounds (8, 8, 150, 20);

    lbl_prog_version_extra.reset (new Label ("new label",
                                             TRANS("Final")));
    addAndMakeVisible (lbl_prog_version_extra.get());
    lbl_prog_version_extra->setFont (Font (15.0f, Font::plain).withTypefaceStyle ("Bold"));
    lbl_prog_version_extra->setJustificationType (Justification::centredLeft);
    lbl_prog_version_extra->setEditable (false, false, false);
    lbl_prog_version_extra->setColour (Label::textColourId, Colours::yellow);
    lbl_prog_version_extra->setColour (TextEditor::textColourId, Colours::black);
    lbl_prog_version_extra->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    lbl_prog_version_extra->setBounds (8, 32, 150, 20);


    //[UserPreSize]
    //[/UserPreSize]

    setSize (500, 228);


    //[Constructor] You can add your own custom stuff here..
    lbl_prog_version->setText(JucePlugin_Name " " ADLplug_Version, dontSendNotification);
#if ADLplug_VersionFinal
    lbl_prog_version_extra->setText("Final", dontSendNotification);
    lbl_prog_version_extra->setColour(Label::textColourId, Colour(0xf0, 0xf8, 0xff));
    lbl_prog_version_extra->setBounds(
        lbl_prog_version_extra->getBounds() +
        Point<int>(lbl_prog_version_extra->getBorderSize().getLeft(), 0));
#else
    lbl_prog_version_extra->setText(ADLplug_VersionExtra, dontSendNotification);
#endif
    //[/Constructor]
}

About_Component::~About_Component()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    hyperlinkButton = nullptr;
    label = nullptr;
    label2 = nullptr;
    lbl_prog_version = nullptr;
    lbl_prog_version_extra = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void About_Component::paint (Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    g.fillAll (Colour (0xff323e44));

    //[UserPaint] Add your own custom painting code here..
#if ADLplug_VersionFinal
    Label *lbl = lbl_prog_version_extra.get();
    Rectangle<float> bounds = lbl->getBounds().toFloat();
    float textw = GlyphArrangement::getStringWidth(lbl->getFont(), lbl->getText());
    Rectangle<float> rect =
        bounds.withWidth(textw + lbl->getBorderSize().getLeftAndRight());
    g.setColour(Colour(0x52, 0x94, 0x58));
    g.fillRoundedRectangle(rect, 2.0);
    g.setColour(Colour(0xf0, 0xf8, 0xff));
    g.drawRoundedRectangle(rect, 2.0, 1.0);
#endif
    //[/UserPaint]
}

void About_Component::resized()
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
