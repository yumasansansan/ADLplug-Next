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
#include "ui/components/wave_label.h"
#include "ui/components/info_display.h"
#include "adl/instrument.h"
#include "parameter_block.h"
#include <format>
#include <cmath>
#include <memory>
//[/Headers]

#include "operator_editor.h"


//[MiscUserDefs] You can add your own user definitions and misc code here...
//[/MiscUserDefs]

//==============================================================================
Operator_Editor::Operator_Editor (unsigned op_id, Parameter_Block &pb)
{
    //[Constructor_pre] You can add your own custom stuff here..
    operator_id_ = op_id;
    parameter_block_ = &pb;
    //[/Constructor_pre]

    kn_attack = std::make_unique<Styled_Knob_Default> ();
    addAndMakeVisible (kn_attack.get());
    kn_attack->setName ("new component");

    kn_attack->setBounds (16, 3, 40, 40);

    kn_decay = std::make_unique<Styled_Knob_Default> ();
    addAndMakeVisible (kn_decay.get());
    kn_decay->setName ("new component");

    kn_decay->setBounds (64, 3, 40, 40);

    kn_sustain = std::make_unique<Styled_Knob_Default> ();
    addAndMakeVisible (kn_sustain.get());
    kn_sustain->setName ("new component");

    kn_sustain->setBounds (16, 48, 40, 40);

    kn_release = std::make_unique<Styled_Knob_Default> ();
    addAndMakeVisible (kn_release.get());
    kn_release->setName ("new component");

    kn_release->setBounds (64, 48, 40, 40);

    btn_ssgenable = std::make_unique<TextButton> ("new button");
    addAndMakeVisible (btn_ssgenable.get());
    btn_ssgenable->setButtonText (String());
    btn_ssgenable->addListener (this);
    btn_ssgenable->setColour (TextButton::buttonOnColourId, Colour (0xff42a2c8));

    btn_ssgenable->setBounds (4, 100, 15, 15);

    lbl_level = std::make_unique<Label> ("new label",
                                TRANS("Lv"));
    addAndMakeVisible (lbl_level.get());
    lbl_level->setFont (FontOptions (14.0f).withStyle ("Regular"));
    lbl_level->setJustificationType (Justification::centredLeft);
    lbl_level->setEditable (false, false, false);
    lbl_level->setColour (Label::textColourId, Colours::aliceblue);
    lbl_level->setColour (TextEditor::textColourId, Colours::black);
    lbl_level->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    lbl_level->setBounds (163, 0, 28, 16);

    label = std::make_unique<Label> ("new label",
                            TRANS("A"));
    addAndMakeVisible (label.get());
    label->setFont (FontOptions (15.0f).withStyle ("Regular"));
    label->setJustificationType (Justification::centredTop);
    label->setEditable (false, false, false);
    label->setColour (Label::textColourId, Colours::aliceblue);
    label->setColour (TextEditor::textColourId, Colours::black);
    label->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    label->setBounds (4, 0, 20, 16);

    label2 = std::make_unique<Label> ("new label",
                             TRANS("D"));
    addAndMakeVisible (label2.get());
    label2->setFont (FontOptions (15.0f).withStyle ("Regular"));
    label2->setJustificationType (Justification::centredTop);
    label2->setEditable (false, false, false);
    label2->setColour (Label::textColourId, Colours::aliceblue);
    label2->setColour (TextEditor::textColourId, Colours::black);
    label2->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    label2->setBounds (52, 0, 20, 16);

    label3 = std::make_unique<Label> ("new label",
                             TRANS("S"));
    addAndMakeVisible (label3.get());
    label3->setFont (FontOptions (15.0f).withStyle ("Regular"));
    label3->setJustificationType (Justification::centredTop);
    label3->setEditable (false, false, false);
    label3->setColour (Label::textColourId, Colours::aliceblue);
    label3->setColour (TextEditor::textColourId, Colours::black);
    label3->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    label3->setBounds (4, 48, 20, 16);

    label4 = std::make_unique<Label> ("new label",
                             TRANS("R"));
    addAndMakeVisible (label4.get());
    label4->setFont (FontOptions (15.0f).withStyle ("Regular"));
    label4->setJustificationType (Justification::centredTop);
    label4->setEditable (false, false, false);
    label4->setColour (Label::textColourId, Colours::aliceblue);
    label4->setColour (TextEditor::textColourId, Colours::black);
    label4->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    label4->setBounds (52, 48, 20, 16);

    lbl_fmul = std::make_unique<Label> ("new label",
                               TRANS("F*"));
    addAndMakeVisible (lbl_fmul.get());
    lbl_fmul->setFont (FontOptions (14.0f).withStyle ("Regular"));
    lbl_fmul->setJustificationType (Justification::centredLeft);
    lbl_fmul->setEditable (false, false, false);
    lbl_fmul->setColour (Label::textColourId, Colours::aliceblue);
    lbl_fmul->setColour (TextEditor::textColourId, Colours::black);
    lbl_fmul->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    lbl_fmul->setBounds (163, 16, 28, 16);

    lbl_rsl = std::make_unique<Label> ("new label",
                              TRANS("Rsl"));
    addAndMakeVisible (lbl_rsl.get());
    lbl_rsl->setFont (FontOptions (14.0f).withStyle ("Regular"));
    lbl_rsl->setJustificationType (Justification::centredLeft);
    lbl_rsl->setEditable (false, false, false);
    lbl_rsl->setColour (Label::textColourId, Colours::aliceblue);
    lbl_rsl->setColour (TextEditor::textColourId, Colours::black);
    lbl_rsl->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    lbl_rsl->setBounds (163, 32, 28, 16);

    kn_decay2 = std::make_unique<Styled_Knob_Default> ();
    addAndMakeVisible (kn_decay2.get());
    kn_decay2->setName ("new component");

    kn_decay2->setBounds (112, 3, 40, 40);

    label9 = std::make_unique<Label> ("new label",
                             TRANS("D"));
    addAndMakeVisible (label9.get());
    label9->setFont (FontOptions (15.0f).withStyle ("Regular"));
    label9->setJustificationType (Justification::centredTop);
    label9->setEditable (false, false, false);
    label9->setColour (Label::textColourId, Colours::aliceblue);
    label9->setColour (TextEditor::textColourId, Colours::black);
    label9->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    label9->setBounds (100, 0, 20, 16);

    btn_am = std::make_unique<TextButton> ("new button");
    addAndMakeVisible (btn_am.get());
    btn_am->setButtonText (TRANS("AM"));
    btn_am->addListener (this);
    btn_am->setColour (TextButton::buttonOnColourId, Colour (0xff42a2c8));

    btn_am->setBounds (112, 55, 40, 24);

    lbl_tune = std::make_unique<Label> ("new label",
                               TRANS("Detune"));
    addAndMakeVisible (lbl_tune.get());
    lbl_tune->setFont (FontOptions (14.0f).withStyle ("Regular"));
    lbl_tune->setJustificationType (Justification::centred);
    lbl_tune->setEditable (false, false, false);
    lbl_tune->setColour (Label::textColourId, Colours::aliceblue);
    lbl_tune->setColour (TextEditor::textColourId, Colours::black);
    lbl_tune->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    lbl_tune->setBounds (163, 48, 96, 16);

    label5 = std::make_unique<Label> ("new label",
                             TRANS("SSG-EG"));
    addAndMakeVisible (label5.get());
    label5->setFont (FontOptions (14.0f).withStyle ("Regular"));
    label5->setJustificationType (Justification::centredLeft);
    label5->setEditable (false, false, false);
    label5->setColour (Label::textColourId, Colours::aliceblue);
    label5->setColour (TextEditor::textColourId, Colours::black);
    label5->setColour (TextEditor::backgroundColourId, Colour (0x00000000));

    label5->setBounds (20, 99, 56, 16);

    btn_prev_ssgwave = std::make_unique<TextButton> ("new button");
    addAndMakeVisible (btn_prev_ssgwave.get());
    btn_prev_ssgwave->setButtonText (TRANS("<"));
    btn_prev_ssgwave->setConnectedEdges (Button::ConnectedOnRight);
    btn_prev_ssgwave->addListener (this);

    btn_prev_ssgwave->setBounds (82, 96, 23, 24);

    btn_next_ssgwave = std::make_unique<TextButton> ("new button");
    addAndMakeVisible (btn_next_ssgwave.get());
    btn_next_ssgwave->setButtonText (TRANS(">"));
    btn_next_ssgwave->setConnectedEdges (Button::ConnectedOnLeft);
    btn_next_ssgwave->addListener (this);

    btn_next_ssgwave->setBounds (211, 96, 23, 24);

    lbl_ssgwave = std::make_unique<Wave_Label> (ssgeg_waves_);
    addAndMakeVisible (lbl_ssgwave.get());
    lbl_ssgwave->setName ("new component");

    lbl_ssgwave->setBounds (105, 96, 106, 24);

    cb_detune = std::make_unique<ComboBox> ("new combo box");
    addAndMakeVisible (cb_detune.get());
    cb_detune->setEditableText (false);
    cb_detune->setJustificationType (Justification::centredLeft);
    cb_detune->setTextWhenNothingSelected (String());
    cb_detune->setTextWhenNoChoicesAvailable (TRANS("(no choices)"));
    cb_detune->addListener (this);

    cb_detune->setBounds (163, 65, 96, 24);

    sl_level = std::make_unique<Styled_Slider_DefaultSmall> ();
    addAndMakeVisible (sl_level.get());
    sl_level->setName ("new component");

    sl_level->setBounds (195, -2, 64, 20);

    sl_fmul = std::make_unique<Styled_Slider_DefaultSmall> ();
    addAndMakeVisible (sl_fmul.get());
    sl_fmul->setName ("new component");

    sl_fmul->setBounds (195, 14, 64, 20);

    sl_rsl = std::make_unique<Styled_Slider_DefaultSmall> ();
    addAndMakeVisible (sl_rsl.get());
    sl_rsl->setName ("new component");

    sl_rsl->setBounds (195, 30, 64, 20);


    //[UserPreSize]
    sl_level->add_listener(this);
    sl_level->set_range(0, 127);
    sl_level->set_max_increment(1);
    sl_fmul->add_listener(this);
    sl_fmul->set_range(0, 15);
    sl_fmul->set_max_increment(1);
    sl_rsl->add_listener(this);
    sl_rsl->set_range(0, 3);
    sl_rsl->set_max_increment(1);

    kn_attack->add_listener(this);
    kn_attack->set_max_increment(1);
    kn_decay->add_listener(this);
    kn_decay->set_max_increment(1);
    kn_decay2->add_listener(this);
    kn_decay2->set_max_increment(1);
    kn_sustain->add_listener(this);
    kn_sustain->set_max_increment(1);
    kn_release->add_listener(this);
    kn_release->set_max_increment(1);

    btn_am->setClickingTogglesState(true);
    btn_ssgenable->setClickingTogglesState(true);
    //[/UserPreSize]

    setSize (264, 128);


    //[Constructor] You can add your own custom stuff here..
    kn_attack->set_range(0, 31);
    kn_decay->set_range(0, 31);
    kn_decay2->set_range(0, 31);
    kn_sustain->set_range(0, 15);
    kn_release->set_range(0, 15);

    const char *detunes[8] = {
        "×1",
        "×(1+ε)",
        "×(1+2ε)",
        "×(1+3ε)",
        "×1",
        "×(1-ε)",
        "×(1-2ε)",
        "×(1-3ε)",
    };

    for (int i = 0; i < 8; ++i)
        cb_detune->addItem(std::format("{:d} : {:s}", i + 1, detunes[i]), i + 1);
    cb_detune->setScrollWheelEnabled(true);
    //[/Constructor]
}

Operator_Editor::~Operator_Editor()
{
    //[Destructor_pre]. You can add your own custom destruction code here..
    //[/Destructor_pre]

    kn_attack = nullptr;
    kn_decay = nullptr;
    kn_sustain = nullptr;
    kn_release = nullptr;
    btn_ssgenable = nullptr;
    lbl_level = nullptr;
    label = nullptr;
    label2 = nullptr;
    label3 = nullptr;
    label4 = nullptr;
    lbl_fmul = nullptr;
    lbl_rsl = nullptr;
    kn_decay2 = nullptr;
    label9 = nullptr;
    btn_am = nullptr;
    lbl_tune = nullptr;
    label5 = nullptr;
    btn_prev_ssgwave = nullptr;
    btn_next_ssgwave = nullptr;
    lbl_ssgwave = nullptr;
    cb_detune = nullptr;
    sl_level = nullptr;
    sl_fmul = nullptr;
    sl_rsl = nullptr;


    //[Destructor]. You can add your own custom destruction code here..
    //[/Destructor]
}

//==============================================================================
void Operator_Editor::paint (Graphics& g)
{
    //[UserPrePaint] Add your own custom painting code here..
    //[/UserPrePaint]

    {
        int x = 104, y = 96, width = 108, height = 24;
        Colour strokeColour = Colour (0xff8e989b);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (strokeColour);
        g.drawRect (x, y, width, height, 1);

    }

    {
        float x = 0.0f, y = 0.0f, width = 264.0f, height = 128.0f;
        Colour fillColour = Colour (0x662e4c4d);
        //[UserPaintCustomArguments] Customize the painting arguments here..
        //[/UserPaintCustomArguments]
        g.setColour (fillColour);
        g.fillRoundedRectangle (x, y, width, height, 5.0f);
    }

    //[UserPaint] Add your own custom painting code here..
    //[/UserPaint]
}

void Operator_Editor::resized()
{
    //[UserPreResize] Add your own custom resize code here..
    //[/UserPreResize]

    //[UserResized] Add your own custom resize handling here..
    //[/UserResized]
}

void Operator_Editor::buttonClicked (Button* buttonThatWasClicked)
{
    //[UserbuttonClicked_Pre]
    Parameter_Block &pb = *parameter_block_;
    Parameter_Block::Part &part = pb.part[midichannel_];
    Parameter_Block::Operator &op = part.nth_operator(operator_id_);
    Button *btn = buttonThatWasClicked;
    //[/UserbuttonClicked_Pre]

    if (buttonThatWasClicked == btn_ssgenable.get())
    {
        //[UserButtonCode_btn_ssgenable] -- add your button handler code here..
        AudioParameterBool &p = *op.p_ssgenable;
        p.beginChangeGesture();
        p = btn->getToggleState();
        p.endChangeGesture();
        //[/UserButtonCode_btn_ssgenable]
    }
    else if (buttonThatWasClicked == btn_am.get())
    {
        //[UserButtonCode_btn_am] -- add your button handler code here..
        AudioParameterBool &p = *op.p_am;
        p.beginChangeGesture();
        p = btn->getToggleState();
        p.endChangeGesture();
        //[/UserButtonCode_btn_am]
    }
    else if (buttonThatWasClicked == btn_prev_ssgwave.get())
    {
        //[UserButtonCode_btn_prev_ssgwave] -- add your button handler code here..
        AudioParameterChoice &p = *op.p_ssgwave;
        p.beginChangeGesture();
        int wave = std::max(p.getIndex() - 1, 0);
        p = wave;
        p.endChangeGesture();
        lbl_ssgwave->set_wave(static_cast<unsigned>(wave), dontSendNotification);
        //[/UserButtonCode_btn_prev_ssgwave]
    }
    else if (buttonThatWasClicked == btn_next_ssgwave.get())
    {
        //[UserButtonCode_btn_next_ssgwave] -- add your button handler code here..
        AudioParameterChoice &p = *op.p_ssgwave;
        p.beginChangeGesture();
        int wave = std::min(p.getIndex() + 1, p.choices.size() - 1);
        p = wave;
        p.endChangeGesture();
        lbl_ssgwave->set_wave(static_cast<unsigned>(wave), dontSendNotification);
        //[/UserButtonCode_btn_next_ssgwave]
    }

    //[UserbuttonClicked_Post]
    if (display_info_for_component(btn))
        info_->expire_info_in();
    //[/UserbuttonClicked_Post]
}

void Operator_Editor::comboBoxChanged (ComboBox* comboBoxThatHasChanged)
{
    //[UsercomboBoxChanged_Pre]
    Parameter_Block &pb = *parameter_block_;
    Parameter_Block::Part &part = pb.part[midichannel_];
    Parameter_Block::Operator &op = part.nth_operator(operator_id_);
    //[/UsercomboBoxChanged_Pre]

    if (comboBoxThatHasChanged == cb_detune.get())
    {
        //[UserComboBoxCode_cb_detune] -- add your combo box handling code here..
        AudioParameterInt &p = *op.p_detune;
        p.beginChangeGesture();
        p = cb_detune->getSelectedId() - 1;
        p.endChangeGesture();
        //[/UserComboBoxCode_cb_detune]
    }

    //[UsercomboBoxChanged_Post]
    //[/UsercomboBoxChanged_Post]
}



//[MiscUserCode] You can add your own definitions of your custom methods or any other code here...
void Operator_Editor::set_operator_parameters(const Instrument &ins, unsigned op, NotificationType ntf)
{
    kn_attack->set_value(ins.attack(op), ntf);
    kn_decay->set_value(ins.decay1(op), ntf);
    kn_decay2->set_value(ins.decay2(op), ntf);
    kn_sustain->set_value(ins.sustain(op), ntf);
    kn_release->set_value(ins.release(op), ntf);

    sl_level->set_value(ins.level(op), ntf);
    sl_fmul->set_value(ins.fmul(op), ntf);
    sl_rsl->set_value(ins.ratescale(op), ntf);
    cb_detune->setSelectedId(ins.detune(op) + 1, ntf);

    btn_am->setToggleState(ins.am(op), ntf);
    btn_ssgenable->setToggleState(ins.ssgenable(op), ntf);
    lbl_ssgwave->set_wave(static_cast<unsigned>(ins.ssgwave(op)), ntf);
}

void Operator_Editor::set_operator_enabled(bool b)
{
    if (b == operator_enabled_)
        return;

    operator_enabled_ = b;
    repaint();
}

void Operator_Editor::knob_value_changed(Knob *k)
{
    Parameter_Block &pb = *parameter_block_;
    Parameter_Block::Part &part = pb.part[midichannel_];
    Parameter_Block::Operator &op = part.nth_operator(operator_id_);

    if (k == sl_level.get()) {
        AudioParameterInt &p = *op.p_level;
        p = static_cast<int>(std::lround(k->value()));
    }
    else if (k == sl_fmul.get()) {
        AudioParameterInt &p = *op.p_fmul;
        p = static_cast<int>(std::lround(k->value()));
    }
    else if (k == sl_rsl.get()) {
        AudioParameterInt &p = *op.p_ratescale;
        p = static_cast<int>(std::lround(k->value()));
    }
    else if (k == kn_attack.get()) {
        AudioParameterInt &p = *op.p_attack;
        p = static_cast<int>(std::lround(k->value()));
    }
    else if (k == kn_decay.get()) {
        AudioParameterInt &p = *op.p_decay1;
        p = static_cast<int>(std::lround(k->value()));
    }
    else if (k == kn_decay2.get()) {
        AudioParameterInt &p = *op.p_decay2;
        p = static_cast<int>(std::lround(k->value()));
    }
    else if (k == kn_sustain.get()) {
        AudioParameterInt &p = *op.p_sustain;
        p = static_cast<int>(std::lround(k->value()));
    }
    else if (k == kn_release.get()) {
        AudioParameterInt &p = *op.p_release;
        p = static_cast<int>(std::lround(k->value()));
    }

    display_info_for_component(k);
}

void Operator_Editor::knob_drag_started(Knob *k)
{
    Parameter_Block &pb = *parameter_block_;
    Parameter_Block::Part &part = pb.part[midichannel_];
    Parameter_Block::Operator &op = part.nth_operator(operator_id_);

    if (k == sl_level.get()) {
        AudioParameterInt &p = *op.p_level;
        p.beginChangeGesture();
    }
    else if (k == sl_fmul.get()) {
        AudioParameterInt &p = *op.p_fmul;
        p.beginChangeGesture();
    }
    else if (k == sl_rsl.get()) {
        AudioParameterInt &p = *op.p_ratescale;
        p.beginChangeGesture();
    }
    else if (k == kn_attack.get()) {
        AudioParameterInt &p = *op.p_attack;
        p.beginChangeGesture();
    }
    else if (k == kn_decay.get()) {
        AudioParameterInt &p = *op.p_decay1;
        p.beginChangeGesture();
    }
    else if (k == kn_decay2.get()) {
        AudioParameterInt &p = *op.p_decay2;
        p.beginChangeGesture();
    }
    else if (k == kn_sustain.get()) {
        AudioParameterInt &p = *op.p_sustain;
        p.beginChangeGesture();
    }
    else if (k == kn_release.get()) {
        AudioParameterInt &p = *op.p_release;
        p.beginChangeGesture();
    }

    display_info_for_component(k);
}

void Operator_Editor::knob_drag_ended(Knob *k)
{
    Parameter_Block &pb = *parameter_block_;
    Parameter_Block::Part &part = pb.part[midichannel_];
    Parameter_Block::Operator &op = part.nth_operator(operator_id_);

    if (k == sl_level.get()) {
        AudioParameterInt &p = *op.p_level;
        p.endChangeGesture();
    }
    else if (k == sl_fmul.get()) {
        AudioParameterInt &p = *op.p_fmul;
        p.endChangeGesture();
    }
    else if (k == sl_rsl.get()) {
        AudioParameterInt &p = *op.p_ratescale;
        p.endChangeGesture();
    }
    else if (k == kn_attack.get()) {
        AudioParameterInt &p = *op.p_attack;
        p.endChangeGesture();
    }
    else if (k == kn_decay.get()) {
        AudioParameterInt &p = *op.p_decay1;
        p.endChangeGesture();
    }
    else if (k == kn_decay2.get()) {
        AudioParameterInt &p = *op.p_decay2;
        p.endChangeGesture();
    }
    else if (k == kn_sustain.get()) {
        AudioParameterInt &p = *op.p_sustain;
        p.endChangeGesture();
    }
    else if (k == kn_release.get()) {
        AudioParameterInt &p = *op.p_release;
        p.endChangeGesture();
    }

    info_->expire_info_in();
}

void Operator_Editor::paintOverChildren(Graphics &g)
{
    if (!operator_enabled_) {
        Rectangle<int> bounds = getLocalBounds();
        g.setColour(Colour(0x66777777));
        g.fillRoundedRectangle(bounds.toFloat(), 7.0f);
    }
}

bool Operator_Editor::display_info_for_component(Component *c)
{
    String param;
    int val = 0;
    const char *prefixes[4] = {"Op1 ", "Op3 ", "Op2 ", "Op4 "};
    String prefix = prefixes[operator_id_];

    // The value that a knob shows, as the parameter has it.
    const auto value_of = [](const Knob &knob) { return static_cast<int>(std::lround(knob.value())); };

    if (c == sl_level.get()) {
        param = prefix + "Level";
        val = value_of(*sl_level);
    }
    else if (c == sl_fmul.get()) {
        param = prefix + "Frequency multiplier";
        val = value_of(*sl_fmul);
    }
    else if (c == sl_rsl.get()) {
        param = prefix + "Rate scale level";
        val = value_of(*sl_rsl);
    }
    else if (c == kn_attack.get()) {
        param = prefix + "Attack";
        val = value_of(*kn_attack);
    }
    else if (c == kn_decay.get()) {
        param = prefix + "Primary Decay";
        val = value_of(*kn_decay);
    }
    else if (c == kn_decay2.get()) {
        param = prefix + "Secondary Decay";
        val = value_of(*kn_decay2);
    }
    else if (c == kn_sustain.get()) {
        param = prefix + "Sustain";
        val = value_of(*kn_sustain);
    }
    else if (c == kn_release.get()) {
        param = prefix + "Release";
        val = value_of(*kn_release);
    }
    else if (c == btn_next_ssgwave.get() || c == btn_prev_ssgwave.get()) {
        param = prefix + "SSG-EG Wave";
        val = static_cast<int>(lbl_ssgwave->wave());
    }

    if (param.isEmpty())
        return false;

    info_->display_info(param + " = " + String(val));
    return true;
}
//[/MiscUserCode]


//==============================================================================


//[EndFile] You can add extra defines here...
//[/EndFile]
