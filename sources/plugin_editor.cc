//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018 Jean Pierre Cimalando
// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: BSL-1.0 AND GPL-3.0-or-later
//
// This file comes from ADLplug and was modified for ADLplug-Next. The notice at
// the top is ADLplug's; the LICENSE it names was ADLplug's copy of the Boost
// Software License, now LICENSES/BSL-1.0.txt. The SPDX lines name the copyright
// holders and licenses in the machine-readable form of the REUSE specification:
// ADLplug's code is under the Boost Software License 1.0, and ADLplug-Next's
// changes are under the GNU General Public License, version 3 or any later
// version (LICENSES/GPL-3.0-or-later.txt).

#include "plugin_editor.h"
#include "plugin_processor.h"
#include "messages.h"
#include "configuration.h"
#include "ui/main_component.h"
#include "ui/look_and_feel.h"
#include "utility/functional_timer.h"
#include "utility/name_field.h"
#include <cassert>

AdlplugAudioProcessorEditor::AdlplugAudioProcessorEditor(AdlplugAudioProcessor &p, Parameter_Block &pb)
    : AudioProcessorEditor(&p), proc_(p)
{
    conf_ = std::make_unique<Configuration>();
    conf_->load_default();
    conf_->save_default();

    lnf_ = std::make_unique<Custom_Look_And_Feel>();
    LookAndFeel::setDefaultLookAndFeel(lnf_.get());

    tooltip_window_ = std::make_unique<TooltipWindow>(this);

    main_ = std::make_unique<Main_Component>(p, pb, *conf_);
    addAndMakeVisible(*main_);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize(main_->getWidth(), main_->getHeight());

    notification_timer_ = Functional_Timer::create([this] { process_notifications(); });
    notification_timer_->startTimer(10);

    main_->request_state_from_processor();
}

AdlplugAudioProcessorEditor::~AdlplugAudioProcessorEditor()
{
    // Another instance's editor may have installed its own look since.
    if (&LookAndFeel::getDefaultLookAndFeel() == lnf_.get())
        LookAndFeel::setDefaultLookAndFeel(nullptr);
}

//==============================================================================
void AdlplugAudioProcessorEditor::paint(Graphics &g)
{
    LookAndFeel &lnf = getLookAndFeel();

    // (Our component is opaque, so we must completely fill the background with a
    // solid colour)
    g.fillAll(lnf.findColour(ResizableWindow::backgroundColourId));
}

void AdlplugAudioProcessorEditor::resized()
{
    main_->setBounds(getLocalBounds());
}

void AdlplugAudioProcessorEditor::process_notifications()
{
    AdlplugAudioProcessor &proc = proc_;
    Main_Component &main = *main_;
    const std::shared_ptr<Simple_Fifo> queue = proc.message_queue_to_ui();

    if (!queue)
        return;

    while (const Buffered_Message msg = Messages::read(*queue)) {
        switch (static_cast<Fx_Message>(msg.header.tag)) {
        case Fx_Message::NotifyReady:
            main.request_state_from_processor();
            break;
        case Fx_Message::NotifyBankSlots:
            main.receive_bank_slots(Messages::body<Messages::Fx::NotifyBankSlots>(msg));
            break;
        case Fx_Message::NotifyGlobalParameters:
            main.receive_global_parameters(Messages::body<Messages::Fx::NotifyGlobalParameters>(msg).param);
            break;
        case Fx_Message::NotifyInstrument: {
            const auto body = Messages::body<Messages::Fx::NotifyInstrument>(msg);
            main.receive_instrument(body.bank, body.program, body.instrument);
            break;
        }
        case Fx_Message::NotifyChipSettings:
            main.receive_chip_settings(Messages::body<Messages::Fx::NotifyChipSettings>(msg).cs);
            break;
        case Fx_Message::NotifySelection: {
            const auto body = Messages::body<Messages::Fx::NotifySelection>(msg);
            main.receive_selection(body.part, body.bank, body.program);
            break;
        }
        case Fx_Message::NotifyActivePart:
            main.on_change_midi_channel(Messages::body<Messages::Fx::NotifyActivePart>(msg).part);
            break;
        case Fx_Message::NotifyBankTitle: {
            const auto body = Messages::body<Messages::Fx::NotifyBankTitle>(msg);
            main.on_change_bank_title(name_from_field(body.title), dontSendNotification);
            break;
        }
        default:
            assert(false);
            break;
        }
        Messages::finish_read(*queue, msg);
    }
}
