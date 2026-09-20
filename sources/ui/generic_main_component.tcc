//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018-2019, 2021 Jean Pierre Cimalando
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

#include "generic_main_component.h"
#include "plugin_processor.h"
#include "parameter_block.h"
#include "configuration.h"
#include "ui/components/new_program_editor.h"
#include "ui/components/program_name_editor.h"
#include "ui/components/midi_keyboard_ex.h"
#include "adl/wopx_file.h"
#include "bank_load.h"
#include "midi/insnames.h"
#include "utility/functional_timer.h"
#include "utility/name_field.h"
#include "utility/simple_fifo.h"
#include "utility/pak.h"
#include "resources.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <format>
#include <string>
#include <utility>

#if 1
#   define trace(fmt, ...)
#else
#   define trace(fmt, ...) std::fprintf(stderr, "[UI Main] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#endif

#if !defined(JUCE_LINUX)
static constexpr bool prefer_native_file_dialog = true;
#else
static constexpr bool prefer_native_file_dialog = false;
#endif

template <class T>
inline T *Generic_Main_Component<T>::self()
{
    return static_cast<T *>(this);
}

template <class T>
inline const T *Generic_Main_Component<T>::self() const
{
    return static_cast<const T *>(this);
}

template <class T>
Generic_Main_Component<T>::Generic_Main_Component(
    AdlplugAudioProcessor &proc, Parameter_Block &pb, Configuration &conf)
    : proc_(&proc), parameter_block_(&pb), conf_(&conf)
{
    Desktop::getInstance().addFocusChangeListener(this);
    setWantsKeyboardFocus(true);
    mouse_hover_listener_ = std::make_unique<Mouse_Hover_Listener>(*this);
    addMouseListener(mouse_hover_listener_.get(), true);
    midi_kb_state_.addListener(this);
    message_flush_timer_ = Functional_Timer::create([this] { flush_messages_to_processor(); });
    initialize_bank_directory();
}

template <class T>
Generic_Main_Component<T>::~Generic_Main_Component()
{
    // The dialogs act on this component; do not leave them behind.
    for (DialogWindow *dialog : {dlg_new_program_.getComponent(), dlg_edit_program_.getComponent(),
                                 dlg_about_.getComponent(), dlg_bank_information_.getComponent()})
        delete dialog;

    midi_kb_state_.removeListener(this);
    removeMouseListener(mouse_hover_listener_.get());
    Desktop::getInstance().removeFocusChangeListener(this);
}

template <class T>
void Generic_Main_Component<T>::setup_generic_components()
{
    Configuration &conf = *conf_;
    const Parameter_Block &pb = *parameter_block_;

    set_default_info(self()->lbl_info->getText());

    self()->cb_program->setScrollWheelEnabled(true);

    self()->kn_mastervol->add_listener(self());
    self()->kn_mastervol->set_range(0, 1);

    const Volume_Limits limits = master_volume_limits(*pb.p_mastervol);
    self()->kn_mastervol->set_max_increment(1.0 / (limits.dbmax - limits.dbmin));

    self()->edt_bank_name->addListener(this);
    self()->edt_bank_name->setTextToShowWhenEmpty(
        TRANS("Bank name"), findColour(TextEditor::backgroundColourId).contrasting(0.5f));

    last_key_layout_ = load_key_configuration(*self()->midi_kb, conf);
    self()->midi_kb->setKeyPressBaseOctave(midi_kb_octave_);
    self()->midi_kb->setLowestVisibleKey(24);

    self()->btn_bank_load->setTooltip(TRANS("Load bank"));
    self()->btn_bank_save->setTooltip(TRANS("Save bank"));
    create_image_overlay(*self()->btn_bank_load, image_from_resource(Res::emoji_u1f4c2), 0.7);
    create_image_overlay(*self()->btn_bank_save, image_from_resource(Res::emoji_u1f4be), 0.7);

    create_image_overlay(*self()->btn_pgm_edit, image_from_resource(Res::emoji_u1f4dd), 0.7);
    create_image_overlay(*self()->btn_pgm_add, image_from_resource(Res::emoji_u2795), 0.7);

    static constexpr const char *note_names[12] =
        {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    // The key of a drum is a byte. libADLMIDI and libOPNMIDI play a key from
    // 128 on as the key 128 below it, which the menu names too; banks such as
    // Bisqwit's in libADLMIDI have those keys.
    for (int key = 0; key < 256; ++key) {
        const int note = key % 128;
        String text = note_names[note % 12] + String(note / 12 - 1);
        if (key >= 128)
            text += " (" + String(key) + ")";
        self()->cb_percussion_key->addItem(text, key + 1);
    }
    self()->cb_percussion_key->setSelectedId(69 + 1, dontSendNotification);
    self()->cb_percussion_key->setScrollWheelEnabled(true);

    create_image_overlay(*self()->btn_keymap, image_from_resource(Res::emoji_u2328), 0.7);

    vu_timer_ = Functional_Timer::create([this] { vu_update(); });
    vu_timer_->startTimer(30);

    cpu_load_timer_ = Functional_Timer::create([this] { cpu_load_update(); });
    cpu_load_timer_->startTimer(500);
    self()->lbl_cpu->setText("0%", dontSendNotification);

    midi_activity_timer_ = Functional_Timer::create([this] { midi_activity_update(); });
    midi_activity_timer_->startTimer(100);

    midi_keys_timer_ = Functional_Timer::create([this] { midi_keys_update(); });
    midi_keys_timer_->startTimer(40);

    parameter_watch_timer_ = Functional_Timer::create([this] { parameters_update(); });
    parameter_watch_timer_->startTimer(500);
}

template <class T>
void Generic_Main_Component<T>::request_state_from_processor()
{
    write_to_processor(Messages::User::RequestChipSettings{});
    write_to_processor(Messages::User::RequestFullBankState{});

    Messages::User::RequestSelections selections;
    selections.channel_mask.set();
    write_to_processor(selections);

    write_to_processor(Messages::User::RequestActivePart{});
    write_to_processor(Messages::User::RequestBankTitle{});
}

template <class T>
bool Generic_Main_Component<T>::is_percussion_channel(unsigned channel) const
{
    return channel == 9;
}

template <class T>
void Generic_Main_Component<T>::send_rename_bank(Bank_Id bank, const String &name)
{
    Messages::User::RenameBank msg;
    msg.bank = bank;
    msg.notify_back = true;
    copy_name_to_field(msg.name, name);
    write_to_processor(msg);
}

template <class T>
void Generic_Main_Component<T>::send_rename_program(Bank_Id bank, unsigned pgm, const String &name)
{
    Messages::User::RenameProgram msg;
    msg.bank = bank;
    msg.program = static_cast<std::uint8_t>(pgm & 127);
    msg.notify_back = true;
    copy_name_to_field(msg.name, name);
    write_to_processor(msg);
}

template <class T>
void Generic_Main_Component<T>::send_create_program(Bank_Id bank, unsigned pgm)
{
    Messages::User::CreateInstrument msg;
    msg.bank = bank;
    msg.program = static_cast<std::uint8_t>(pgm & 127);
    msg.notify_back = true;
    write_to_processor(msg);
}

template <class T>
Instrument *Generic_Main_Component<T>::find_instrument(std::uint32_t program, Instrument *if_not_found)
{
    const auto it = instrument_map_.find(program >> 8);
    if (it == instrument_map_.end())
        return if_not_found;
    return &it->second.ins[program & 255];
}

template <class T>
void Generic_Main_Component<T>::reload_selected_instrument(NotificationType ntf)
{
    const int selection = self()->cb_program->getSelectedId();

    trace("Reload selected instrument %s", program_selection_to_string(selection).toRawUTF8());

    Instrument ins_empty;
    const Instrument *ins = &ins_empty;
    int designated_note = -1;

    if (selection != 0) {
        const auto program = static_cast<std::uint32_t>(selection - 1);
        ins = find_instrument(program, &ins_empty);
        if ((program & 128) != 0)
            designated_note = static_cast<int>(program & 127);
    }
    self()->set_instrument_parameters(*ins, ntf);
    self()->midi_kb->designate_note(designated_note);
}

template <class T>
void Generic_Main_Component<T>::send_selection_update()
{
    const int selection = self()->cb_program->getSelectedId();

    std::uint32_t program = 0;
    if (selection != 0) {
        program = static_cast<std::uint32_t>(selection - 1);
        trace("Send selection update %s",
              program_selection_to_string(selection).toRawUTF8());
    }
    else {
        trace("Send selection update 0:0:0 because of empty selection");
    }

    Messages::User::SelectProgram msg;
    msg.part = midichannel_;
    msg.bank = bank_of_psid(program >> 8, (program & 128) != 0);
    msg.program = static_cast<std::uint8_t>(program & 127);
    write_to_processor(msg);
}

template <class T>
void Generic_Main_Component<T>::receive_bank_slots(const Messages::Fx::NotifyBankSlots &msg)
{
    const std::span<const Messages::Fx::NotifyBankSlots::Entry> entries(
        msg.entry, std::min<std::size_t>(msg.count, std::size(msg.entry)));
    bool update = false;
    auto &imap = instrument_map_;

    trace("Receive %zu bank slots", entries.size());

    // delete bank entries not in the slots
    for (auto it = imap.begin(); it != imap.end();) {
        const std::uint32_t psid = it->first;
        const bool found = std::ranges::any_of(entries, [psid](const auto &entry) { return entry.bank.pseudo_id() == psid; });
        if (found)
            ++it;
        else {
            it = imap.erase(it);
            update = true;
        }
    }

    // extract the names
    using Name = std::array<char, sizeof Messages::Fx::NotifyBankSlots::Entry::name>;
    std::map<Bank_Id, Name> bank_name_map;
    for (const auto &entry : entries) {
        if (entry.name[0] != '\0')
            std::ranges::copy(entry.name, bank_name_map[entry.bank].begin());
    }

    // enable or disable instruments according to slots
    for (const auto &entry : entries) {
        const bool percussive = entry.bank.percussive != 0;
        Editor_Bank &e_bank = imap[entry.bank.pseudo_id()];
        for (unsigned i = 0; i < 128; ++i) {
            Instrument &ins = e_bank.ins[i + (percussive ? 128 : 0)];
            const bool isblank = !entry.used.test(i);
            if (ins.blank() != isblank) {
                ins.blank(isblank);
                update = true;
            }
        }
        static constexpr Name name_empty {};
        const auto it = bank_name_map.find(entry.bank);
        const Name &name_src = (it != bank_name_map.end()) ? it->second : name_empty;
        const std::span<char, std::tuple_size_v<Name>> name_dst(percussive ? e_bank.percussion_name : e_bank.melodic_name);
        if (!std::ranges::equal(name_dst, name_src)) {
            std::ranges::copy(name_src, name_dst.begin());
            update = true;
        }
    }

    if (update) {
        trace("Refresh choices because of received slots");
        update_instrument_choices();
    }
}

template <class T>
void Generic_Main_Component<T>::receive_global_parameters(const Instrument_Global_Parameters &gp)
{
    trace("Receive global parameters");

    instrument_gparam_ = gp;
    self()->set_global_parameters(dontSendNotification);
}

template <class T>
void Generic_Main_Component<T>::receive_instrument(Bank_Id bank, unsigned pgm, const Instrument &ins)
{
    assert(pgm < 128);

    const unsigned insno = (pgm & 127) + (bank.percussive ? 128 : 0);
    const std::uint32_t psid = bank.pseudo_id();

    trace("Receive instrument %u:%u:%u", bank.msb, bank.lsb, insno);

    auto &instrument_map = instrument_map_;
    auto it = instrument_map.find(psid);
    bool update = false;
    if (it == instrument_map.end()) {
        if (ins.blank())
            return;
        it = instrument_map.emplace(psid, Editor_Bank()).first;
        it->second.ins[insno] = ins;
        update = true;
    }
    else {
        Instrument &current = it->second.ins[insno];
        update = !current.equal_instrument(ins) || std::strncmp(ins.name, current.name, 32) != 0;
        if (update)
            current = ins;
    }

    const bool empty_bank = std::ranges::all_of(
        it->second.ins, [](const Instrument &instrument) { return instrument.blank(); });
    if (empty_bank) {
        instrument_map.erase(it);
        update = true;
    }

    if (update) {
        trace("Refresh choices because of received instrument");
        update_instrument_choices();
    }
}

template <class T>
void Generic_Main_Component<T>::receive_chip_settings(const Chip_Settings &cs)
{
    trace("Receive chip settings");

    chip_settings_ = cs;
    self()->set_chip_settings(dontSendNotification);
}

template <class T>
void Generic_Main_Component<T>::receive_selection(unsigned part, Bank_Id bank, std::uint8_t pgm)
{
    if (part >= midiprogram_.size())
        return;

    const std::uint32_t selection = (bank.pseudo_id() << 8) | (pgm & 127u) | (bank.percussive ? 128u : 0u);
    midiprogram_[part] = selection;

    if (part == midichannel_) {
        set_program_selection(static_cast<int>(selection) + 1, dontSendNotification);
        reload_selected_instrument(dontSendNotification);
    }
}

template <class T>
void Generic_Main_Component<T>::update_instrument_choices()
{
    ComboBox &cb = *self()->cb_program;
    const int selection = cb.getSelectedId();
    cb.clear(dontSendNotification);
    PopupMenu *menu = cb.getRootMenu();
    const bool percussion_channel = is_percussion_channel(midichannel_);
    const Midi_Db &db = midi_db();

    for (auto &[psid, e_bank] : instrument_map_) {
        const unsigned msb = psid >> 7;
        const unsigned lsb = psid & 127;

        String bank_sid;
        if (e_bank.melodic_name[0] != '\0')
            bank_sid = std::format("{:03d}:{:03d} {}", msb, lsb, name_view(e_bank.melodic_name));
        else if (e_bank.percussion_name[0] != '\0')
            bank_sid = std::format("{:03d}:{:03d} {}", msb, lsb, name_view(e_bank.percussion_name));
        else
            bank_sid = std::format("{:03d}:{:03d} <Untitled bank>", msb, lsb);

        e_bank.ins_menu.clear();
        for (unsigned i = 0; i < 256; ++i) {
            const Instrument &ins = e_bank.ins[i];
            if (ins.blank() || percussion_channel != (i >= 128))
                continue;

            const char kind = (i >= 128) ? 'P' : 'M';
            String ins_sid;
            if (ins.name[0] != '\0')
                ins_sid = std::format("{:c}{:03d} {}", kind, i & 127, name_view(ins.name));
            else {
                const Midi_Program_Ex *ex = db.find_ex(msb, lsb, i);
                const char *name = ex ? ex->name : (i < 128) ? db.inst(i) : db.perc(i & 127).name;
                ins_sid = std::format("{:c}{:03d} {}", kind, i & 127, name);
            }

            const std::uint32_t program = (psid << 8) + i;
            e_bank.ins_menu.addItem(static_cast<int>(program) + 1, ins_sid);
        }

        menu->addSubMenu(bank_sid, e_bank.ins_menu);
    }

    set_program_selection(selection, dontSendNotification);
    reload_selected_instrument(dontSendNotification);
}

template <class T>
void Generic_Main_Component<T>::set_program_selection(int selection, NotificationType ntf)
{
    trace("Change program selection %s to %s",
          program_selection_to_string(self()->cb_program->getSelectedId()).toRawUTF8(),
          program_selection_to_string(selection).toRawUTF8());

    self()->cb_program->setSelectedId(selection, ntf);

    trace("New program selection %s",
          program_selection_to_string(self()->cb_program->getSelectedId()).toRawUTF8());
}

template <class T>
String Generic_Main_Component<T>::program_selection_to_string(int selection)
{
    if (selection == 0)
        return "(nil)";

    const auto program = static_cast<std::uint32_t>(selection - 1);
    const std::uint32_t psid = program >> 8;
    return std::format("{}:{}:{}", psid >> 7, psid & 127, program & 255);
}

template <class T>
void Generic_Main_Component<T>::handle_selected_program(int selection)
{
    trace("Select program by UI %s",
          program_selection_to_string(selection).toRawUTF8());

    if (selection != 0) {
        midiprogram_[midichannel_] = static_cast<std::uint32_t>(selection - 1);
        send_selection_update();
    }
    reload_selected_instrument(dontSendNotification);
}

template <class T>
void Generic_Main_Component<T>::handle_edit_program()
{
    if (dlg_edit_program_)
        return;

    const std::uint32_t program = midiprogram_[midichannel_];
    const std::uint32_t psid = program >> 8;
    const bool percussive = (program & 128) != 0;

    const auto it = instrument_map_.find(psid);
    if (it == instrument_map_.end())
        return;

    const Editor_Bank &e_bank = it->second;
    const Instrument &ins = e_bank.ins[program & 255];
    if (ins.blank())
        return;

    auto editor = std::make_unique<Program_Name_Editor>();
    editor->set_program(bank_of_psid(psid, percussive), program & 127,
                        name_from_field(percussive ? e_bank.percussion_name : e_bank.melodic_name),
                        name_from_field(ins.name));

    const Component::SafePointer<Generic_Main_Component<T>> safe(this);
    editor->on_ok = [safe](const Program_Name_Editor::Result &result) {
        if (safe == nullptr)
            return;
        safe->send_rename_bank(result.bank, result.bank_name);
        safe->send_rename_program(result.bank, result.pgm, result.pgm_name);
        if (DialogWindow *dialog = safe->dlg_edit_program_.getComponent())
            dialog->exitModalState(1);
    };
    editor->on_cancel = [safe]() {
        if (safe == nullptr)
            return;
        if (DialogWindow *dialog = safe->dlg_edit_program_.getComponent())
            dialog->exitModalState(0);
    };

    DialogWindow::LaunchOptions dlgopts;
    dlgopts.dialogTitle = "Edit program";
    dlgopts.componentToCentreAround = this;
    dlgopts.resizable = false;
    dlgopts.content.set(editor.release(), true);
    dlg_edit_program_ = dlgopts.launchAsync();
}

template <class T>
void Generic_Main_Component<T>::handle_add_program()
{
    PopupMenu menu;
    menu.addItem(1, "Add program");
    menu.addItem(2, "Delete program");
    menu.addSeparator();
    menu.addItem(3, "Delete bank");
    menu.addItem(4, "Delete all banks");

    const Component::SafePointer<Generic_Main_Component<T>> safe(this);
    menu.showMenuAsync(PopupMenu::Options().withParentComponent(this),
                       [safe](int selection) {
                           if (safe != nullptr)
                               safe->finish_add_program(selection);
                       });
}

template <class T>
void Generic_Main_Component<T>::finish_add_program(int selection)
{
    const std::uint32_t program = midiprogram_[midichannel_];
    const bool percussive = (program & 128) != 0;
    const Bank_Id bank = bank_of_psid(program >> 8, percussive);
    const auto pgm = static_cast<std::uint8_t>(program & 127);
    const Component::SafePointer<Generic_Main_Component<T>> safe(this);

    switch (selection) {
    case 1: {
        if (dlg_new_program_)
            return;

        auto editor = std::make_unique<New_Program_Editor>();
        editor->set_current(bank, pgm);

        editor->on_ok = [safe](const New_Program_Editor::Result &result) {
            if (safe == nullptr)
                return;
            safe->send_create_program(result.bank, result.pgm);
            if (DialogWindow *dialog = safe->dlg_new_program_.getComponent())
                dialog->exitModalState(1);
        };
        editor->on_cancel = [safe]() {
            if (safe == nullptr)
                return;
            if (DialogWindow *dialog = safe->dlg_new_program_.getComponent())
                dialog->exitModalState(0);
        };

        DialogWindow::LaunchOptions dlgopts;
        dlgopts.dialogTitle = "Add program";
        dlgopts.componentToCentreAround = this;
        dlgopts.resizable = false;
        dlgopts.content.set(editor.release(), true);
        dlg_new_program_ = dlgopts.launchAsync();
        break;
    }
    case 2: {
        Messages::User::DeleteInstrument msg;
        msg.bank = bank;
        msg.program = pgm;
        msg.notify_back = true;
        AlertWindow::showOkCancelBox(
            AlertWindow::QuestionIcon, "Delete program",
            std::format("Confirm deletion of program {:c}{:03d}?", percussive ? 'P' : 'M', pgm),
            {}, {}, this,
            ModalCallbackFunction::create([safe, msg](int result) {
                if (result == 1 && safe != nullptr)
                    safe->write_to_processor(msg);
            }));
        break;
    }
    case 3: {
        Messages::User::DeleteBank msg;
        msg.bank = bank;
        msg.notify_back = true;
        AlertWindow::showOkCancelBox(
            AlertWindow::QuestionIcon, "Delete bank",
            std::format("Confirm deletion of bank {:03d}:{:03d}?", bank.msb, bank.lsb),
            {}, {}, this,
            ModalCallbackFunction::create([safe, msg](int result) mutable {
                if (result != 1 || safe == nullptr)
                    return;
                // both the melodic and the percussion bank
                safe->write_to_processor(msg);
                msg.bank.percussive = !msg.bank.percussive;
                safe->write_to_processor(msg);
            }));
        break;
    }
    case 4: {
        Messages::User::ClearBanks msg;
        msg.notify_back = true;
        AlertWindow::showOkCancelBox(
            AlertWindow::QuestionIcon, "Delete all banks",
            "Confirm deletion of all banks?",
            {}, {}, this,
            ModalCallbackFunction::create([safe, msg](int result) {
                if (result == 1 && safe != nullptr)
                    safe->write_to_processor(msg);
            }));
        break;
    }
    default:
        break;
    }
}

template <class T>
void Generic_Main_Component<T>::create_image_overlay(Component &component, const Image &image, double ratio)
{
    auto overlay = std::make_unique<ImageComponent>();
    const Rectangle<int> bounds = component.getBounds();
    overlay->setBounds(bounds.withSizeKeepingCentre(
        static_cast<int>(ratio * bounds.getWidth()), static_cast<int>(ratio * bounds.getHeight())));
    overlay->setImage(image, RectanglePlacement::centred);
    overlay->setInterceptsMouseClicks(false, true);
    addAndMakeVisible(*overlay);
    image_overlays_.push_back(std::move(overlay));
}

template <class T>
void Generic_Main_Component<T>::vu_update()
{
    const AdlplugAudioProcessor &proc = *proc_;
    self()->vu_left->set_value(proc.vu_level(0));
    self()->vu_right->set_value(proc.vu_level(1));
}

template <class T>
void Generic_Main_Component<T>::cpu_load_update()
{
    const AdlplugAudioProcessor &proc = *proc_;
    const String text = String(static_cast<int>(100.0 * proc.cpu_load())) + "%";
    self()->lbl_cpu->setText(text, dontSendNotification);
}

template <class T>
void Generic_Main_Component<T>::midi_activity_update()
{
    const AdlplugAudioProcessor &proc = *proc_;
    auto &indicator = *self()->ind_midi_activity;
    const unsigned columns = indicator.columns();
    if (columns == 0)
        return;
    for (unsigned i = 0; i < 16; ++i)
        indicator.set_value(i / columns, i % columns, proc.midi_channel_note_count(i) > 0);
}

template <class T>
void Generic_Main_Component<T>::midi_keys_update()
{
    const AdlplugAudioProcessor &proc = *proc_;
    Midi_Keyboard_Ex &kb = *self()->midi_kb;
    const unsigned midichannel = midichannel_;
    for (unsigned note = 0; note < 128; ++note)
        kb.highlight_note(note, proc.midi_channel_note_active(midichannel, note) ? 127 : 0);
}

template <class T>
void Generic_Main_Component<T>::parameters_update()
{
    const Parameter_Block &pb = *parameter_block_;
    set_volume_knob_value(static_cast<double>(pb.p_mastervol->get()), dontSendNotification);
}

template <class T>
void Generic_Main_Component<T>::update_emulator_icon()
{
    const Emulator_Defaults &defaults = get_emulator_defaults();
    const std::vector<Image> &images = emulator_icons_->images;
    const unsigned emulator = chip_settings_.emulator;

    self()->btn_emulator->setImages(
        false, true, true,
        (emulator < images.size()) ? images[emulator] : Image(), 1, Colour(),
        Image(), 1, Colour(),
        Image(), 1, Colour());
    self()->btn_emulator->setTooltip(defaults.choices[static_cast<int>(emulator)]);
}

template <class T>
void Generic_Main_Component<T>::build_emulator_menu(PopupMenu &menu)
{
    const Emulator_Defaults &defaults = get_emulator_defaults();
    const std::vector<Image> &images = emulator_icons_->images;

    menu.clear();
    for (int i = 0; i < defaults.choices.size(); ++i) {
        const String &name = defaults.choices[i];
        if (name.isEmpty())
            continue;
        const auto index = static_cast<std::size_t>(i);
        menu.addItem(i + 1, name, true, false, (index < images.size()) ? images[index] : Image());
    }
}

template <class T>
void Generic_Main_Component<T>::select_emulator_by_menu(std::function<void(int)> on_selected)
{
    PopupMenu menu;
    build_emulator_menu(menu);
    menu.showMenuAsync(PopupMenu::Options()
                       .withParentComponent(this)
                       .withItemThatMustBeVisible(static_cast<int>(chip_settings_.emulator) + 1),
                       std::move(on_selected));
}

template <class T>
void Generic_Main_Component<T>::handle_load_bank(Component *clicked)
{
    PopupMenu menu;
    menu.addItem(load_bank_file_id, "Load bank file...");
    menu.addItem(load_instrument_file_id, "Load instrument file...");

    Pak_File_Reader pak;
    [[maybe_unused]] const bool pak_ok = pak.init_with_data(Res::banks_pak.data, Res::banks_pak.size);
    assert(pak_ok);

    if (pak.entry_count() > 0) {
        PopupMenu pak_submenu;
        for (std::size_t i = 0; i < pak.entry_count(); ++i)
            pak_submenu.addItem(load_collection_first_id + static_cast<int>(i), pak.name(i));
        menu.addSubMenu("Load from collection", pak_submenu);
    }

    // What the sources say of the bank of the collection that is loaded, found
    // by its title: a bank loaded from a file, or renamed, has nothing.
    menu.addSeparator();
    menu.addItem(bank_information_id, "Bank information...",
                 pak.find(self()->edt_bank_name->getText().toStdString()).has_value());

    const Component::SafePointer<Generic_Main_Component<T>> safe(this);
    menu.showMenuAsync(PopupMenu::Options().withTargetComponent(clicked),
                       [safe](int selection) {
                           if (safe != nullptr)
                               safe->finish_load_bank(selection);
                       });
}

template <class T>
void Generic_Main_Component<T>::finish_load_bank(int selection)
{
    if (selection == 0)
        return;

#if defined(ADLPLUG_OPL3)
    const char *bank_file_filter =
        "*."  WOPx_BANK_SUFFIX;
    const char *ins_file_filter =
        "*." WOPx_INST_SUFFIX ";"
        "*.sbi";
#elif defined(ADLPLUG_OPN2)
    const char *bank_file_filter =
        "*."  WOPx_BANK_SUFFIX;
    const char *ins_file_filter =
        "*." WOPx_INST_SUFFIX;
#endif

    constexpr int open_flags =
        FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles;

    const Component::SafePointer<Generic_Main_Component<T>> safe(this);

    if (selection == load_bank_file_id) {
        file_chooser_ = std::make_unique<FileChooser>(
            TRANS("Load bank..."), bank_directory_, bank_file_filter, prefer_native_file_dialog);
        file_chooser_->launchAsync(open_flags, [safe](const FileChooser &chooser) {
            const File file = chooser.getResult();
            if (safe == nullptr || file == File())
                return;
            safe->change_bank_directory(file.getParentDirectory());
            safe->load_bank(file, 0);
        });
    }
    else if (selection == load_instrument_file_id) {
        const int program_selection = self()->cb_program->getSelectedId();
        if (program_selection == 0) {
            AlertWindow::showMessageBoxAsync(
                AlertWindow::WarningIcon, TRANS("Load instrument..."), TRANS("Please select a program first."));
            return;
        }

        file_chooser_ = std::make_unique<FileChooser>(
            TRANS("Load instrument..."), bank_directory_, ins_file_filter, prefer_native_file_dialog);
        file_chooser_->launchAsync(open_flags, [safe, program_selection](const FileChooser &chooser) {
            const File file = chooser.getResult();
            if (safe == nullptr || file == File())
                return;
            safe->change_bank_directory(file.getParentDirectory());
            int format = 0;
#if defined(ADLPLUG_OPL3)
            if (file.hasFileExtension(".sbi"))
                format = 1;
#endif
            safe->load_single_instrument(static_cast<std::uint32_t>(program_selection - 1), file, format);
        });
    }
    else if (selection == bank_information_id) {
        show_bank_information();
    }
    else if (selection >= load_collection_first_id) {
        Pak_File_Reader pak;
        [[maybe_unused]] const bool pak_ok = pak.init_with_data(Res::banks_pak.data, Res::banks_pak.size);
        assert(pak_ok);

        const auto index = static_cast<std::size_t>(selection - load_collection_first_id);
        if (index >= pak.entry_count())
            return;
        std::vector<std::uint8_t> data = pak.extract(index);
        load_bank_mem(data, String(pak.name(index)), 0);
    }
}

template <class T>
void Generic_Main_Component<T>::show_bank_information()
{
    Pak_File_Reader pak;
    [[maybe_unused]] const bool pak_ok = pak.init_with_data(Res::banks_pak.data, Res::banks_pak.size);
    assert(pak_ok);
    const std::optional<std::size_t> index = pak.find(self()->edt_bank_name->getText().toStdString());
    if (!index)
        return;
    const std::string info = pak.info(*index);

    // One window at a time, on the bank that is loaded now.
    delete dlg_bank_information_.getComponent();

    auto text = std::make_unique<TextEditor>();
    text->setMultiLine(true, true);
    text->setReadOnly(true);
    text->setCaretVisible(false);
    text->setScrollbarsShown(true);
    text->setFont(FontOptions(Font::getDefaultMonospacedFontName(), 13.0f, Font::plain));
    text->setText(String::fromUTF8(info.data(), static_cast<int>(info.size())), false);
    text->setSize(680, 520);

    DialogWindow::LaunchOptions dlgopts;
    dlgopts.dialogTitle = String::fromUTF8(pak.name(*index).c_str());
    dlgopts.content.set(text.release(), true);
    dlgopts.componentToCentreAround = this;
    dlgopts.resizable = true;
    dlg_bank_information_ = dlgopts.launchAsync();
}

template <class T>
void Generic_Main_Component<T>::handle_save_bank(Component *clicked)
{
    PopupMenu menu;
    menu.addItem(1, "Save bank file...");
    menu.addItem(2, "Save instrument file...");

    const Component::SafePointer<Generic_Main_Component<T>> safe(this);
    menu.showMenuAsync(PopupMenu::Options().withTargetComponent(clicked),
                       [safe](int selection) {
                           if (safe != nullptr)
                               safe->finish_save_bank(selection);
                       });
}

// Native save dialogs warn about overwriting themselves, but the JUCE fallback
// does not, so ask once a name has been chosen.
template <class T>
void Generic_Main_Component<T>::confirm_overwrite(const File &file, std::function<void()> on_confirmed)
{
    if (!file.exists()) {
        on_confirmed();
        return;
    }

    const String title = TRANS("File already exists");
    const String message = TRANS("There's already a file called: ")
        + file.getFullPathName() + "\n\n" +
        TRANS("Are you sure you want to overwrite it?");

    AlertWindow::showOkCancelBox(
        AlertWindow::WarningIcon, title, message,
        TRANS("Overwrite"), TRANS("Cancel"), this,
        ModalCallbackFunction::create([callback = std::move(on_confirmed)](int result) {
            if (result == 1)
                callback();
        }));
}

template <class T>
void Generic_Main_Component<T>::finish_save_bank(int selection)
{
    if (selection == 0)
        return;

    const char *bank_file_filter = "*." WOPx_BANK_SUFFIX;
    const char *bank_file_extension = "." WOPx_BANK_SUFFIX;
    const char *ins_file_filter = "*." WOPx_INST_SUFFIX;
    const char *ins_file_extension = "." WOPx_INST_SUFFIX;

    constexpr int save_flags =
        FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles;

    const Component::SafePointer<Generic_Main_Component<T>> safe(this);

    if (selection == 1) {
        const File initial_file = bank_directory_.getChildFile(
            File::createLegalFileName(self()->edt_bank_name->getText()));
        file_chooser_ = std::make_unique<FileChooser>(
            TRANS("Save bank..."), initial_file, bank_file_filter, prefer_native_file_dialog);
        file_chooser_->launchAsync(save_flags, [safe, bank_file_extension](const FileChooser &chooser) {
            if (safe == nullptr || chooser.getResult() == File())
                return;
            const File file = chooser.getResult().withFileExtension(bank_file_extension);
            safe->confirm_overwrite(file, [safe, file]() {
                if (safe == nullptr)
                    return;
                safe->change_bank_directory(file.getParentDirectory());
                safe->save_bank(file);
            });
        });
    }
    else if (selection == 2) {
        const int program_selection = self()->cb_program->getSelectedId();
        if (program_selection == 0) {
            AlertWindow::showMessageBoxAsync(
                AlertWindow::WarningIcon, TRANS("Save instrument..."), TRANS("Please select a program first."));
            return;
        }

        file_chooser_ = std::make_unique<FileChooser>(
            TRANS("Save instrument..."), bank_directory_, ins_file_filter, prefer_native_file_dialog);
        file_chooser_->launchAsync(save_flags,
            [safe, program_selection, ins_file_extension](const FileChooser &chooser) {
                if (safe == nullptr || chooser.getResult() == File())
                    return;
                const File file = chooser.getResult().withFileExtension(ins_file_extension);
                safe->confirm_overwrite(file, [safe, program_selection, file]() {
                    if (safe == nullptr)
                        return;
                    safe->change_bank_directory(file.getParentDirectory());
                    safe->save_single_instrument(static_cast<std::uint32_t>(program_selection - 1), file);
                });
            });
    }
}

template <class T>
void Generic_Main_Component<T>::load_bank(const File &file, int format)
{
    trace("Load from " WOPx_BANK_FORMAT " file: %s", file.getFullPathName().toRawUTF8());

    if (std::optional<std::vector<std::uint8_t>> data = read_file_for_loading(file, "Error loading bank"))
        load_bank_mem(*data, file.getFileNameWithoutExtension(), format);
}

template <class T>
void Generic_Main_Component<T>::load_single_instrument(std::uint32_t program, const File &file, int format)
{
    trace("Load from " WOPx_INST_FORMAT " file: %s", file.getFullPathName().toRawUTF8());

    if (std::optional<std::vector<std::uint8_t>> data = read_file_for_loading(file, "Error loading instrument"))
        load_single_instrument_mem(program, *data, file.getFileNameWithoutExtension(), format);
}

template <class T>
void Generic_Main_Component<T>::load_bank_mem(std::span<std::uint8_t> data, const String &bank_name, int format)
{
    const char *error_title = "Error loading bank";

    switch (format) {
    default: {
        // Reading the bytes and sending what they say are bank_load.h's, so that
        // the path from a file to the bank manager is one piece of code, which a
        // fuzz target can hand bytes to as well.
        const std::optional<Bank_File_Contents> contents = read_bank_file(data);
        if (!contents) {
            AlertWindow::showMessageBoxAsync(
                AlertWindow::WarningIcon, error_title, "The input file is not in " WOPx_BANK_FORMAT " format.");
            return;
        }
#if defined(ADLPLUG_OPN2)
        if (contents->chip_type)
            *parameter_block_->p_chiptype = *contents->chip_type;
#endif
        send_bank_file([this](const auto &msg) { this->write_to_processor(msg); },
                       *contents, bank_name, midichannel_);
        break;
    }
    }
}

template <class T>
void Generic_Main_Component<T>::load_single_instrument_mem(std::uint32_t program, std::span<std::uint8_t> data, [[maybe_unused]] const String &bank_name, int format)
{
    const std::optional<Instrument> ins = read_instrument_file(data, format);
    if (!ins) {
#if defined(ADLPLUG_OPL3)
        const char *what = (format == 1) ? "The input file is not in SBI format."
                                         : "The input file is not in " WOPx_INST_FORMAT " format.";
#else
        const char *what = "The input file is not in " WOPx_INST_FORMAT " format.";
#endif
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Error loading instrument", what);
        return;
    }

    send_instrument_file([this](const auto &msg) { this->write_to_processor(msg); }, *ins,
                         bank_of_psid(program >> 8, (program & 128) != 0),
                         static_cast<std::uint8_t>(program & 127), midichannel_);
}

template <class T>
void Generic_Main_Component<T>::save_bank(const File &file)
{
    trace("Save to " WOPx_BANK_FORMAT " file: %s", file.getFullPathName().toRawUTF8());

    std::vector<WOPx::Bank> melo_array;
    std::vector<WOPx::Bank> drum_array;
    melo_array.reserve(instrument_map_.size());
    drum_array.reserve(instrument_map_.size());

    for (const auto &[psid, e_bank] : instrument_map_) {
        WOPx::Bank melo {};
        WOPx::Bank drum {};

        static_assert(sizeof e_bank.melodic_name <= sizeof melo.bank_name &&
                      sizeof e_bank.percussion_name <= sizeof drum.bank_name);
        std::memcpy(melo.bank_name, e_bank.melodic_name, sizeof e_bank.melodic_name);
        std::memcpy(drum.bank_name, e_bank.percussion_name, sizeof e_bank.percussion_name);

        melo.bank_midi_msb = drum.bank_midi_msb = static_cast<std::uint8_t>(psid >> 7);
        melo.bank_midi_lsb = drum.bank_midi_lsb = static_cast<std::uint8_t>(psid & 127);

        std::size_t melo_count = 0;
        std::size_t drum_count = 0;
        for (std::size_t i = 0; i < 256; ++i) {
            const WOPx::Instrument ins = e_bank.ins[i].to_wopl();
            const bool used = (ins.inst_flags & WOPx::Ins_IsBlank) == 0;
            if (i < 128) {
                melo.ins[i] = ins;
                if (used)
                    ++melo_count;
            }
            else {
                drum.ins[i - 128] = ins;
                if (used)
                    ++drum_count;
            }
        }

        if (melo_count > 0)
            melo_array.push_back(melo);
        if (drum_count > 0)
            drum_array.push_back(drum);
    }

    WOPx::BankFile wopl {};
    wopl.version = 0;

#if defined(ADLPLUG_OPL3)
    wopl.opl_flags = static_cast<std::uint8_t>(
        (instrument_gparam_.deep_tremolo ? WOPL_FLAG_DEEP_TREMOLO : 0) |
        (instrument_gparam_.deep_vibrato ? WOPL_FLAG_DEEP_VIBRATO : 0) |
        (instrument_gparam_.mt32_defaults ? WOPL_FLAG_MT32 : 0));
#elif defined(ADLPLUG_OPN2)
    wopl.lfo_freq = static_cast<std::uint8_t>(
        (instrument_gparam_.lfo_enable ? 8 : 0) |
        (instrument_gparam_.lfo_frequency & 7));
    wopl.chip_type = static_cast<std::uint8_t>(chip_settings_.chip_type);
#endif
    wopl.volume_model = static_cast<std::uint8_t>(instrument_gparam_.volume_model);

    wopl.banks_count_melodic = static_cast<std::uint16_t>(melo_array.size());
    wopl.banks_count_percussion = static_cast<std::uint16_t>(drum_array.size());
    wopl.banks_melodic = melo_array.data();
    wopl.banks_percussive = drum_array.data();

    const std::size_t filesize = WOPx::CalculateBankFileSize(&wopl, wopl.version);
    std::vector<std::uint8_t> filedata(filesize);
    const char *error_title = "Error saving bank";

    if (WOPx::SaveBankToMem(&wopl, filedata.data(), filesize, wopl.version, 0) != 0) {
        AlertWindow::showMessageBoxAsync(
            AlertWindow::WarningIcon, error_title, "The bank could not be converted to " WOPx_BANK_FORMAT ".");
        return;
    }

    write_file_for_saving(file, filedata, error_title);
}

template <class T>
void Generic_Main_Component<T>::save_single_instrument(std::uint32_t program, const File &file)
{
    trace("Save to " WOPx_INST_FORMAT " file: %s", file.getFullPathName().toRawUTF8());

    const auto it = instrument_map_.find(program >> 8);
    if (it == instrument_map_.end())
        return;

    WOPx::InstrumentFile opli {};
    opli.version = 0;
    opli.is_drum = ((program & 128) != 0) ? 1 : 0;
    opli.inst = it->second.ins[program & 255].to_wopl();

    const std::size_t filesize = WOPx::CalculateInstFileSize(&opli, opli.version);
    std::vector<std::uint8_t> filedata(filesize);
    const char *error_title = "Error saving instrument";

    if (WOPx::SaveInstToMem(&opli, filedata.data(), filesize, opli.version) != 0) {
        AlertWindow::showMessageBoxAsync(
            AlertWindow::WarningIcon, error_title, "The instrument could not be converted to " WOPx_INST_FORMAT ".");
        return;
    }

    write_file_for_saving(file, filedata, error_title);
}

template <class T>
void Generic_Main_Component<T>::handle_change_keymap()
{
    PopupMenu menu;
    build_key_layout_menu(menu, last_key_layout_);

    const Component::SafePointer<Generic_Main_Component<T>> safe(this);
    menu.showMenuAsync(PopupMenu::Options().withParentComponent(this),
                       [safe](int selection) {
                           if (safe != nullptr)
                               safe->finish_change_keymap(selection);
                       });
}

template <class T>
void Generic_Main_Component<T>::finish_change_keymap(int selection)
{
    Midi_Keyboard_Ex &kb = *self()->midi_kb;
    if (selection != 0)
        last_key_layout_ = set_key_layout(kb, static_cast<Key_Layout>(selection - 1), *conf_);
    kb.grabKeyboardFocus();
}

template <class T>
void Generic_Main_Component<T>::handle_change_octave(int diff)
{
    Midi_Keyboard_Ex &kb = *self()->midi_kb;
    const int octave = std::clamp(midi_kb_octave_ + diff, 0, 10);
    if (octave != midi_kb_octave_) {
        midi_kb_octave_ = octave;
        kb.setKeyPressBaseOctave(octave);
    }
    kb.grabKeyboardFocus();
}

template <class T>
void Generic_Main_Component<T>::set_int_parameter_with_delay(int delay_ms, AudioParameterInt &p, int v)
{
    const String &id = p.paramID;
    std::unique_ptr<Timer> &slot = parameters_delayed_[id];

    if (slot)
        trace("Cancel delayed parameter %s", id.toRawUTF8());
    trace("Schedule delayed parameter %s in %d ms", id.toRawUTF8(), delay_ms);

    slot = Functional_Timer::create1([&p, v](Timer *t) {
        t->stopTimer();
        trace("Set delayed parameter %s now", p.paramID.toRawUTF8());
        p = v;
    });
    slot->startTimer(delay_ms);
}

template <class T>
double Generic_Main_Component<T>::get_volume_knob_value() const
{
    const double knobval = self()->kn_mastervol->value();
    if (knobval <= 0.0)
        return 0.0;

    const Volume_Limits limits = master_volume_limits(*parameter_block_->p_mastervol);
    const double dbval = limits.dbmin + (limits.dbmax - limits.dbmin) * knobval;
    const double linval = std::pow(10.0, 0.05 * dbval);
    return std::clamp(linval, limits.linmin, limits.linmax);
}

template <class T>
void Generic_Main_Component<T>::set_volume_knob_value(double linval, NotificationType ntf)
{
    const Volume_Limits limits = master_volume_limits(*parameter_block_->p_mastervol);

    const double kval = (linval < limits.linmin) ? 0.0 :
        (20.0 * std::log10(linval) - limits.dbmin) / (limits.dbmax - limits.dbmin);

    auto &knob = *self()->kn_mastervol;
    const double old_kval = knob.value();
    knob.set_value(kval, ntf);
    if (old_kval != knob.value())
        update_master_volume_label();
}

template <class T>
void Generic_Main_Component<T>::initialize_bank_directory()
{
    const Configuration &conf = *conf_;

    File dir(conf.get_string("paths", "last-instrument-directory", ""));
    if (!dir.isDirectory())
        dir = File::getSpecialLocation(File::userDocumentsDirectory);
    if (!dir.isDirectory())
        dir = File::getSpecialLocation(File::userHomeDirectory);

    bank_directory_ = dir;
}

template <class T>
void Generic_Main_Component<T>::change_bank_directory(const File &directory)
{
    Configuration &conf = *conf_;

    bank_directory_ = directory;
    conf.set_string("paths", "last-instrument-directory", directory.getFullPathName().toRawUTF8());
    conf.save_default();
}

template <class T>
void Generic_Main_Component<T>::on_change_bank_title(const String &title, NotificationType ntf)
{
    self()->edt_bank_name->setText(title, ntf);
    self()->edt_bank_name->setCaretPosition(0);
}

template <class T>
void Generic_Main_Component<T>::update_master_volume_label()
{
    const double kval = self()->kn_mastervol->value();
    if (kval == 0.0) {
        self()->lbl_mastervol->setText(CharPointer_UTF8("-∞ dB"), dontSendNotification);
        return;
    }

    const Volume_Limits limits = master_volume_limits(*parameter_block_->p_mastervol);
    const double dbval = limits.dbmin + (limits.dbmax - limits.dbmin) * kval;
    const long displayval = std::lround(std::clamp(dbval, limits.dbmin, limits.dbmax));
    self()->lbl_mastervol->setText(std::format("{:+d} dB", displayval), dontSendNotification);
}

template <class T>
void Generic_Main_Component<T>::textEditorTextChanged(TextEditor &editor)
{
    if (&editor == self()->edt_bank_name.get()) {
        Messages::User::SetBankTitle msg;
        copy_name_to_field(msg.title, editor.getText());
        write_to_processor(msg);
    }
}

template <class T>
void Generic_Main_Component<T>::handleNoteOn(MidiKeyboardState *, int channel, int note, float velocity)
{
    write_midi_to_processor(MidiMessage::noteOn(channel, note, velocity));
}

template <class T>
void Generic_Main_Component<T>::handleNoteOff(MidiKeyboardState *, int channel, int note, float velocity)
{
    write_midi_to_processor(MidiMessage::noteOff(channel, note, velocity));
}

template <class T>
void Generic_Main_Component<T>::focusGained([[maybe_unused]] FocusChangeType cause)
{
    if (self()->midi_kb)
        self()->midi_kb->grabKeyboardFocus();
}

template <class T>
void Generic_Main_Component<T>::globalFocusChanged(Component *component)
{
    if (ComponentPeer *peer = getPeer(); peer && component == &peer->getComponent())
        grabKeyboardFocus();
}

template <class T>
void Generic_Main_Component<T>::display_info_now(const String &text)
{
    self()->lbl_info->setText(text, dontSendNotification);
}

template <class T>
auto Generic_Main_Component<T>::master_volume_limits(const AudioParameterFloat &parameter) -> Volume_Limits
{
    Volume_Limits limits;
    limits.linmin = 0.1;
    limits.dbmin = -20.0;
    limits.linmax = static_cast<double>(parameter.range.end);
    limits.dbmax = 20.0 * std::log10(limits.linmax);
    return limits;
}

template <class T>
std::optional<std::vector<std::uint8_t>> Generic_Main_Component<T>::read_file_for_loading(const File &file, const char *error_title)
{
    constexpr int64 max_length = 8 * 1024 * 1024;

    // A file which cannot be opened gives a stream which says so, whereas
    // File::createInputStream() gives none.
    FileInputStream stream(file);
    const int64 length = stream.failedToOpen() ? -1 : stream.getTotalLength();
    if (length < 0) {
        AlertWindow::showMessageBoxAsync(
            AlertWindow::WarningIcon, error_title, "The file could not be opened.");
        return std::nullopt;
    }

    if (length >= max_length) {
        AlertWindow::showMessageBoxAsync(
            AlertWindow::WarningIcon, error_title, "The selected file is too large to be valid.");
        return std::nullopt;
    }

    // A read may return fewer bytes than asked for, so it goes on until the
    // file is in or the stream gives nothing more.
    std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
    std::size_t done = 0;
    while (done < data.size()) {
        const std::span<std::uint8_t> rest = std::span(data).subspan(done);
        const int got = stream.read(rest.data(), static_cast<int>(rest.size()));
        if (got <= 0)
            break;
        done += static_cast<std::size_t>(got);
    }
    if (done != data.size()) {
        AlertWindow::showMessageBoxAsync(
            AlertWindow::WarningIcon, error_title, "The input operation has failed.");
        return std::nullopt;
    }

    return data;
}

// The data goes to a temporary file which then replaces the target, so a
// failure does not leave a truncated file behind.
template <class T>
void Generic_Main_Component<T>::write_file_for_saving(const File &file, std::span<const std::uint8_t> data, const char *error_title)
{
    if (!file.replaceWithData(data.data(), data.size())) {
        AlertWindow::showMessageBoxAsync(
            AlertWindow::WarningIcon, error_title, "The file could not be written.");
    }
}

template <class T>
template <Messages::Body M>
    requires std::same_as<std::remove_cv_t<decltype(M::tag)>, User_Message>
void Generic_Main_Component<T>::write_to_processor(const M &msg)
{
    write_raw_to_processor(std::to_underlying(M::tag), std::as_bytes(std::span(&msg, 1)));
}

template <class T>
void Generic_Main_Component<T>::write_midi_to_processor(const MidiMessage &message)
{
    const std::span<const std::uint8_t> data(message.getRawData(), static_cast<std::size_t>(message.getRawDataSize()));
    write_raw_to_processor(std::to_underlying(User_Message::Midi), std::as_bytes(data));
}

template <class T>
void Generic_Main_Component<T>::write_raw_to_processor(unsigned tag, std::span<const std::byte> body)
{
    pending_messages_.push_back(Pending_Message{tag, {body.begin(), body.end()}});
    flush_messages_to_processor();
}

template <class T>
void Generic_Main_Component<T>::flush_messages_to_processor()
{
    const std::shared_ptr<Simple_Fifo> queue = proc_->message_queue_for_ui();
    if (!queue) {
        // Released: the processor would not take the messages, and it asks
        // for the state again once it is prepared.
        pending_messages_.clear();
    }

    while (queue && !pending_messages_.empty()) {
        const Pending_Message &pending = pending_messages_.front();
        const Buffered_Message msg = Messages::write(*queue, pending.tag, static_cast<unsigned>(pending.body.size()));
        if (!msg)
            break;
        if (!pending.body.empty())
            std::memcpy(msg.body.data(), pending.body.data(), std::min(msg.body.size(), pending.body.size()));
        Messages::finish_write(*queue, msg);
        pending_messages_.pop_front();
    }

    if (pending_messages_.empty())
        message_flush_timer_->stopTimer();
    else if (!message_flush_timer_->isTimerRunning())
        message_flush_timer_->startTimer(10);
}

template <class T>
Label *Generic_Main_Component<T>::slider_text_box(Slider &slider)
{
    for (Component *child : slider.getChildren()) {
        if (auto *label = dynamic_cast<Label *>(child))
            return label;
    }
    return nullptr;
}

template <class T>
Image Generic_Main_Component<T>::image_from_resource(const Res_Data &data)
{
    return ImageCache::getFromMemory(data.data, static_cast<int>(data.size));
}

template <class T>
Bank_Id Generic_Main_Component<T>::bank_of_psid(std::uint32_t psid, bool percussive) noexcept
{
    return Bank_Id(static_cast<std::uint8_t>((psid >> 7) & 127), static_cast<std::uint8_t>(psid & 127), percussive);
}

template <class T>
Generic_Main_Component<T>::Mouse_Hover_Listener::Mouse_Hover_Listener(Generic_Main_Component &owner)
    : owner_(owner)
{
}

template <class T>
void Generic_Main_Component<T>::Mouse_Hover_Listener::mouseEnter(const MouseEvent &event)
{
    // By the time events come, the component is whole, and self() is a T.
    T *c = owner_.self();
    c->display_info_for_component(event.eventComponent);
    c->expire_info_in();
}
