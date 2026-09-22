//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// SPDX-FileCopyrightText: 2018-2019 Jean Pierre Cimalando
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

#pragma once
#include "JuceHeader.h"
#include "messages.h"
#include "adl/instrument.h"
#include "adl/chip_settings.h"
#include "ui/components/info_display.h"
#include "ui/components/resampling_editor.h"
#include "ui/utility/key_maps.h"
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>
class AdlplugAudioProcessor;
struct Parameter_Block;
class Configuration;
struct Res_Data;

template <class T>
class Generic_Main_Component :
    public Component, public FocusChangeListener,
    public TextEditor::Listener,
    public MidiKeyboardStateListener,
    public Info_Display {
public:
    T *self();
    const T *self() const;

private:
    // The class that derives from this one builds it, and nothing else: one built
    // as a class of its own, or one derived from as an ordinary template, would
    // have no self() to call.
    Generic_Main_Component(AdlplugAudioProcessor &proc, Parameter_Block &pb, Configuration &conf);
    friend T;

public:
    ~Generic_Main_Component() override;

    void setup_generic_components();

    void request_state_from_processor();

    bool is_percussion_channel(unsigned channel) const;
    void send_rename_bank(Bank_Id bank, const String &name);
    void send_rename_program(Bank_Id bank, unsigned pgm, const String &name);
    void send_create_program(Bank_Id bank, unsigned pgm);

    Instrument *find_instrument(std::uint32_t program, Instrument *if_not_found);
    void reload_selected_instrument(NotificationType ntf);
    void send_selection_update();
    void receive_bank_slots(const Messages::Fx::NotifyBankSlots &msg);
    void receive_global_parameters(const Instrument_Global_Parameters &gp);
    void receive_instrument(Bank_Id bank, unsigned pgm, const Instrument &ins);
    void receive_chip_settings(const Chip_Settings &cs);
    void receive_resampling(const Resampling_Settings &settings, const Resampling_Status &status);
    void receive_selection(unsigned part, Bank_Id bank, std::uint8_t pgm);
    void update_instrument_choices();
    void set_program_selection(int selection, NotificationType ntf);
    static String program_selection_to_string(int selection);

    void handle_selected_program(int selection);
    void handle_edit_program();
    void handle_add_program();
    void finish_add_program(int selection);

    void create_image_overlay(Component &component, const Image &image, double ratio);

    void vu_update();
    void cpu_load_update();
    void midi_activity_update();
    void midi_keys_update();
    void parameters_update();

    void update_master_volume_label();

    void update_emulator_icon();
    // The menu of the chip's button: the emulators, and the ways of taking a
    // channel of the chip for a new note. What it answers is applied here rather
    // than by the caller, since both answers are parameters of this plugin.
    void build_chip_menu(PopupMenu &menu);
    void select_chip_setting_by_menu(std::function<void(int)> on_selected);
    void apply_chip_menu_choice(int selection);
    // How the chip's samples become the host's (sources/resampling_settings.h),
    // which belongs with the chip and is offered from the same menu.
    void open_resampling_editor();

    void handle_load_bank(Component *clicked);
    void finish_load_bank(int selection);
    void show_bank_information();
    void handle_save_bank(Component *clicked);
    void finish_save_bank(int selection);
    void confirm_overwrite(const File &file, std::function<void()> on_confirmed);
    void load_bank(const File &file, int format);
    void load_single_instrument(std::uint32_t program, const File &file, int format);
    // The loaders of WOPL and WOPN files take the file as writable memory,
    // although they only read it, so the file comes as bytes that may be
    // written rather than as constant ones cast to writable.
    void load_bank_mem(std::span<std::uint8_t> data, const String &bank_name, int format);
    void load_single_instrument_mem(std::uint32_t program, std::span<std::uint8_t> data, const String &bank_name, int format);
    void save_bank(const File &file);
    void save_single_instrument(std::uint32_t program, const File &file);

    void handle_change_keymap();
    void finish_change_keymap(int selection);
    void handle_change_octave(int diff);

    void set_int_parameter_with_delay(int delay_ms, AudioParameterInt &p, int v);

    double get_volume_knob_value() const;
    void set_volume_knob_value(double linval, NotificationType ntf);

    void initialize_bank_directory();
    void change_bank_directory(const File &directory);

    void on_change_bank_title(const String &title, NotificationType ntf);

    void textEditorTextChanged(TextEditor &editor) override;

    void handleNoteOn(MidiKeyboardState * /*state*/, int channel, int note, float velocity) override;
    void handleNoteOff(MidiKeyboardState * /*state*/, int channel, int note, float velocity) override;

    void focusGained(FocusChangeType cause) override;
    void globalFocusChanged(Component *component) override;

private:
    void display_info_now(const String &text) override;

    struct Volume_Limits {
        double linmin = 0;
        double linmax = 0;
        double dbmin = 0;
        double dbmax = 0;
    };
    static Volume_Limits master_volume_limits(const AudioParameterFloat &parameter);

    // A bank or instrument file to load, or nothing if the user was told why not.
    static std::optional<std::vector<std::uint8_t>> read_file_for_loading(const File &file, const char *error_title);
    static void write_file_for_saving(const File &file, std::span<const std::uint8_t> data, const char *error_title);

    // Ids in the menu which loads banks: the files of the collection follow
    // the three fixed entries.
    static constexpr int load_bank_file_id = 1;
    static constexpr int load_instrument_file_id = 2;
    static constexpr int bank_information_id = 3;
    static constexpr int load_collection_first_id = 4;

    // Ids in the menu of the chip's button: an emulator is its own number and one,
    // and a way of taking a channel is this and the number the parameter gives it.
    static constexpr int chan_alloc_first_id = 1000;
    static constexpr int resampling_id = 2000;

    struct Pending_Message {
        unsigned tag = 0;
        std::vector<std::byte> body;
    };
    void write_raw_to_processor(unsigned tag, std::span<const std::byte> body);
    void flush_messages_to_processor();

protected:
    // Sends a message to the processor. Its queue has a fixed size, and it is
    // emptied only while the host runs the processor: what does not fit waits
    // here and goes in order as room is made, instead of the message thread
    // blocking until the host resumes.
    template <Messages::Body M>
        requires std::same_as<std::remove_cv_t<decltype(M::tag)>, User_Message>
    void write_to_processor(const M &msg);
    void write_midi_to_processor(const MidiMessage &message);

    // The label in which a slider shows its value, if it has one.
    static Label *slider_text_box(Slider &slider);
    static Image image_from_resource(const Res_Data &data);
    // The bank of a pseudo-ID, which packs the MSB and the LSB in 14 bits.
    static Bank_Id bank_of_psid(std::uint32_t psid, bool percussive) noexcept;

    AdlplugAudioProcessor *proc_ = nullptr;
    Parameter_Block *parameter_block_ = nullptr;
    Configuration *conf_ = nullptr;

    unsigned midichannel_ = 0;
    std::array<std::uint32_t, 16> midiprogram_ {};

    struct Editor_Bank {
        char melodic_name[32] = {};
        char percussion_name[32] = {};
        PopupMenu ins_menu;
        std::array<Instrument, 256> ins;
    };
    std::map<std::uint32_t, Editor_Bank> instrument_map_;
    Instrument_Global_Parameters instrument_gparam_;
    Chip_Settings chip_settings_;
    // What the processor last said of the resampling, for the editor of it.
    Resampling_Settings resampling_;
    Resampling_Status resampling_status_;
    SharedResourcePointer<Emulator_Icons> emulator_icons_;

    std::map<String, std::unique_ptr<Timer>> parameters_delayed_;

    File bank_directory_;

    MidiKeyboardState midi_kb_state_;
    int midi_kb_octave_ = 6;
    Key_Layout last_key_layout_ = Key_Layout::Default;

    std::unique_ptr<Timer> vu_timer_;
    std::unique_ptr<Timer> cpu_load_timer_;
    std::unique_ptr<Timer> midi_activity_timer_;
    std::unique_ptr<Timer> midi_keys_timer_;
    std::unique_ptr<Timer> parameter_watch_timer_;

    std::deque<Pending_Message> pending_messages_;
    std::unique_ptr<Timer> message_flush_timer_;

    std::vector<std::unique_ptr<ImageComponent>> image_overlays_;

    Component::SafePointer<DialogWindow> dlg_new_program_;
    // FileChooser::launchAsync returns immediately, so the chooser has to
    // outlive the call that opened it.
    std::unique_ptr<FileChooser> file_chooser_;
    Component::SafePointer<DialogWindow> dlg_edit_program_;
    Component::SafePointer<DialogWindow> dlg_about_;
    Component::SafePointer<DialogWindow> dlg_bank_information_;
    Component::SafePointer<DialogWindow> dlg_resampling_;

    // Made in the constructor of this base, before the T around it exists, so
    // it keeps the base and turns it into the T only when an event comes: a
    // downcast of an object under construction is undefined behaviour.
    class Mouse_Hover_Listener : public MouseListener {
    public:
        explicit Mouse_Hover_Listener(Generic_Main_Component &owner);
        void mouseEnter(const MouseEvent &event) override;
    private:
        Generic_Main_Component &owner_;
    };
    std::unique_ptr<Mouse_Hover_Listener> mouse_hover_listener_;
};
