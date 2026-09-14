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
#include "adl/instrument.h"
//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Projucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class Program_Name_Editor  : public Component,
                             public Button::Listener
{
public:
    //==============================================================================
    Program_Name_Editor ();
    ~Program_Name_Editor() override;

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    void set_program(
        Bank_Id bank, unsigned pgm, const String &bank_name, const String &pgm_name);

    struct Result {
        Bank_Id bank;
        unsigned pgm = 0;
        String bank_name;
        String pgm_name;
    };

    std::function<void(const Result &result)> on_ok;
    std::function<void()> on_cancel;
    //[/UserMethods]

    void paint (Graphics& g) override;
    void resized() override;
    void buttonClicked (Button* buttonThatWasClicked) override;



private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    Bank_Id bank_;
    unsigned pgm_ = 0;
    //[/UserVariables]

    //==============================================================================
    std::unique_ptr<TextEditor> edt_pgm_name;
    std::unique_ptr<TextEditor> edt_bank_name;
    std::unique_ptr<Label> label;
    std::unique_ptr<Label> label2;
    std::unique_ptr<TextEditor> edt_pgm_id;
    std::unique_ptr<TextEditor> edt_bank_id;
    std::unique_ptr<TextButton> btn_ok;
    std::unique_ptr<TextButton> btn_cancel;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Program_Name_Editor)
};

//[EndFile] You can add extra defines here...
//[/EndFile]
