/*
  ==============================================================================

  Originally emitted by the Projucer GUI editor (JUCE 5.x, (c) 2017 ROLI Ltd.).

  The .jucer project was retired during the JUCE 9 migration, so this file is
  now maintained by hand. The "//[...]" markers left behind are ordinary
  section comments and no longer carry any special meaning -- edit anywhere.

  SPDX-FileCopyrightText: 2018-2019 Jean Pierre Cimalando
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
#include "plugin_version.h"
//[/Headers]

#include "about_component.h"

#include <memory>


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
About_Component::About_Component ()
{
    //[Constructor_pre] You can add your own custom stuff here..
    //[/Constructor_pre]

    hyperlinkButton = std::make_unique<HyperlinkButton> (TRANS("Home page"),
                                                URL ("https://github.com/yumasansansan/ADLplug-Next"));
    addAndMakeVisible (hyperlinkButton.get());
    hyperlinkButton->setTooltip (TRANS("https://github.com/yumasansansan/ADLplug-Next"));
    hyperlinkButton->setButtonText (TRANS("Home page"));

    hyperlinkButton->setBounds (8, 56, 88, 24);

    label = std::make_unique<Label> ("new label",
                            TRANS("This program is free software, developed by DyTect (Yuma Kakei)\n"
                            "from OPNplug by Jean Pierre Cimalando.\n"
                            "Many thanks to the people who make this program possible."));
    addAndMakeVisible (label.get());
    label->setFont (FontOptions (15.0f).withStyle ("Regular"));
    label->setJustificationType (Justification::centredLeft);
    label->setEditable (false, false, false);
    label->setColour (Label::textColourId, Colours::aliceblue);
    label->setColour (TextEditor::textColourId, Colours::black);
    label->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    label->setBounds (8, 88, 488, 60);

    label2 = std::make_unique<Label> ("new label",
                             CharPointer_UTF8 ("Vitaly Novichkov for the OPNMIDI library\n"
                             "Joel Yliluoma for the original ADLMIDI software\n"
                             "Alexey Khokholov (Nuke.YKT) for the Nuked cores\n"
                             "MAMEDev and contributors for MAME YM2612 and YM2608\n"
                             "St\xc3\xa9phane Dallongeville and Shay Green for GENS OPN2\n"
                             "cisc for fmgen, the Neko Project II Kai OPNA core\n"
                             "Aaron Giles for YMFM"));
    addAndMakeVisible (label2.get());
    label2->setFont (FontOptions (15.0f).withStyle ("Regular"));
    label2->setJustificationType (Justification::centredLeft);
    label2->setEditable (false, false, false);
    label2->setColour (Label::textColourId, Colours::aliceblue);
    label2->setColour (TextEditor::textColourId, Colours::black);
    label2->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    label2->setBounds (8, 156, 488, 112);

    lbl_prog_version = std::make_unique<Label> ("new label",
                                       TRANS("Foobar 1.0"));
    addAndMakeVisible (lbl_prog_version.get());
    lbl_prog_version->setFont (FontOptions (15.0f).withStyle ("Bold"));
    lbl_prog_version->setJustificationType (Justification::centredLeft);
    lbl_prog_version->setEditable (false, false, false);
    lbl_prog_version->setColour (Label::textColourId, Colours::aliceblue);
    lbl_prog_version->setColour (TextEditor::textColourId, Colours::black);
    lbl_prog_version->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    lbl_prog_version->setBounds (8, 8, 488, 20);

    lbl_prog_version_extra = std::make_unique<Label> ("new label",
                                             TRANS("Final"));
    addAndMakeVisible (lbl_prog_version_extra.get());
    lbl_prog_version_extra->setFont (FontOptions (15.0f).withStyle ("Bold"));
    lbl_prog_version_extra->setJustificationType (Justification::centredLeft);
    lbl_prog_version_extra->setEditable (false, false, false);
    lbl_prog_version_extra->setColour (Label::textColourId, Colours::yellow);
    lbl_prog_version_extra->setColour (TextEditor::textColourId, Colours::black);
    lbl_prog_version_extra->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    lbl_prog_version_extra->setBounds (8, 32, 240, 20);


    //[UserPreSize]
    //[/UserPreSize]

    setSize (500, 280);


    //[Constructor] You can add your own custom stuff here..
    lbl_prog_version->setText(JucePlugin_Name " " ADLplug_VersionDisplay, dontSendNotification);
#if ADLplug_VersionDevelopment
    lbl_prog_version_extra->setText("Development version", dontSendNotification);
#else
    lbl_prog_version_extra->setText("Release", dontSendNotification);
    lbl_prog_version_extra->setColour(Label::textColourId, Colour(0xf0, 0xf8, 0xff));
    lbl_prog_version_extra->setBounds(
        lbl_prog_version_extra->getBounds() +
        Point<int>(lbl_prog_version_extra->getBorderSize().getLeft(), 0));
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
#if !ADLplug_VersionDevelopment
    // A release shows its word in a badge.
    Label *lbl = lbl_prog_version_extra.get();
    Rectangle<float> bounds = lbl->getBounds().toFloat();
    float textw = GlyphArrangement::getStringWidth(lbl->getFont(), lbl->getText());
    const Rectangle<float> rect =
        bounds.withWidth(textw + static_cast<float>(lbl->getBorderSize().getLeftAndRight()));
    g.setColour(Colour(0x52, 0x94, 0x58));
    g.fillRoundedRectangle(rect, 2.0f);
    g.setColour(Colour(0xf0, 0xf8, 0xff));
    g.drawRoundedRectangle(rect, 2.0f, 1.0f);
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
