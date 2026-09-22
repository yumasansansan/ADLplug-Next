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
#include "plugin_state.h"
#include "dsp/dc_filter.h"
#include "dsp/vu_monitor.h"
#include "adl/instrument.h"
#include "adl/chip_settings.h"
#include "resampling_settings.h"
#include "utility/processor_ex.h"
#include "utility/atomic_bit_set.h"
#include "utility/chip_resampler.h"
#include "JuceHeader.h"
#include <array>
#include <atomic>
#include <bitset>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
class Player;
class Bank_Manager;
class Simple_Fifo;
class Midi_Input_Source;
class Worker;
struct Parameter_Block;
struct Buffered_Message;

//==============================================================================
class AdlplugAudioProcessor : public AudioProcessorEx,
                              public AudioProcessorParameter::Listener {
public:
    //==========================================================================
    AdlplugAudioProcessor();
    ~AdlplugAudioProcessor() override;

    //==========================================================================
    void prepareToPlay(double sample_rate, int block_size) override;
    void releaseResources() override;

    // Between prepareToPlay() and releaseResources().
    bool is_playback_ready() const noexcept
        { return ready_.load(); }

    std::unique_lock<std::mutex> acquire_player_nonrt();
    // With the player lock held: silences the player and reconfigures its chips.
    void set_chip_settings_nonrt(const Chip_Settings &cs);
    void set_resampling_nonrt(const Resampling_Settings &settings);
    void panic_nonrt();

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    // What the host calls on its audio thread, where a moment's wait is a gap in
    // the sound: nothing under these may take a lock, allocate, free, read a file
    // or throw. The attribute says so, and the realtime sanitizer holds them to it
    // (ADLplug_SANITIZERS=realtime). It costs nothing in a build without that
    // sanitizer, so it is here to be read as much as to be checked. The definition
    // needs no repeat of it: the attribute belongs to the function's type.
    void processBlock(AudioBuffer<float> &buffer, MidiBuffer &midi_messages)
        [[clang::nonblocking]] override;
    void processBlockBypassed(AudioBuffer<float> &buffer, MidiBuffer &midi_messages)
        [[clang::nonblocking]] override;

    void process(float *outputs[], unsigned nframes, Midi_Input_Source &midi) [[clang::nonblocking]];

private:
    void process_messages(bool under_lock);
    void process_parameter_changes();
    void apply_parameter_changes();
    void load_instrument_from_parameters(unsigned part_number);
    void load_global_parameters_from_parameters();
    void process_notifications();

public:
    struct Message_Handler_Context;
    bool handle_midi(const std::uint8_t *data, unsigned len);
    bool handle_message(const Buffered_Message &msg, Message_Handler_Context &ctx);
    void begin_handling_messages([[maybe_unused]] Message_Handler_Context &ctx) {}
    void finish_handling_messages(Message_Handler_Context &ctx);

    void set_instrument_parameters_notifying_host(unsigned part_number);

    void send_program_change_from_selection(unsigned part);

    //==========================================================================
    AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    // For the editor, which may run at the same time as prepareToPlay() and
    // releaseResources() replace the queues.
    std::shared_ptr<Simple_Fifo> message_queue_for_ui() const
        { const std::scoped_lock lock(queue_lock_); return mq_from_ui_; }
    std::shared_ptr<Simple_Fifo> message_queue_to_ui() const
        { const std::scoped_lock lock(queue_lock_); return mq_to_ui_; }

    // For the audio thread and the worker, between prepareToPlay() and
    // releaseResources().
    Simple_Fifo &message_queue_to_ui_rt() const { return *mq_to_ui_; }
    Simple_Fifo &message_queue_for_worker() const { return *mq_from_worker_; }
    Simple_Fifo &message_queue_to_worker() const { return *mq_to_worker_; }

    Parameter_Block &parameter_block() const { return *parameter_block_; }

    // The bookkeeping of the banks: which slots hold which banks, which of their
    // programs are in use, and what they are called. The editor sees it only
    // through the notifications the processor sends, and the tests compare the
    // two. It is made with the player, so only a prepared processor has one (a
    // host saving or restoring a state makes one as well).
    Bank_Manager &bank_manager() const { return *bank_manager_; }

    void mark_for_notification(unsigned changebit)
        { to_notify_.set(changebit); }
    bool unmark_for_notification(unsigned changebit)
        { return to_notify_.reset(changebit); }

private:
    void mark_parameter_as_changed(unsigned changebit)
        { pr_changed_.set(changebit); }
    bool unmark_parameter_as_changed(unsigned changebit)
        { return pr_changed_.reset(changebit); }

public:
    Worker *worker() const
        { return worker_.get(); }

    // Written by the audio thread, read by the editor.
    double vu_level(unsigned channel) const noexcept
        { return (channel < 2) ? lv_current_[channel].load(std::memory_order_relaxed) : 0.0; }
    double cpu_load() const noexcept
        { return cpu_load_.load(std::memory_order_relaxed); }
    unsigned midi_channel_note_count(unsigned channel) const noexcept
        { return (channel < 16) ? midi_channel_note_count_[channel].load(std::memory_order_relaxed) : 0; }
    bool midi_channel_note_active(unsigned channel, unsigned note) const noexcept
        { return channel < 16 && note < 128 && midi_channel_note_active_[channel].test(note, std::memory_order_relaxed); }

    //==========================================================================
    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==========================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const String getProgramName(int index) override;
    void changeProgramName(int index, const String &new_name) override;

    //==========================================================================
    void getStateInformation(MemoryBlock &data) override;
    void setStateInformation(const void *data, int size) override;

    // Maps the parameters of the VST2 and VST3 plugins of upstream ADLplug, which
    // hosts may replace with this one (JUCE_VST3_COMPATIBLE_CLASSES).
    VST3ClientExtensions *getVST3ClientExtensions() override;

private:
    void create_player(unsigned sample_rate);
    void create_first_player(unsigned sample_rate);
    void recreate_player_keeping_state(unsigned sample_rate);
    [[nodiscard]] bool player_matches_settings() const;
    void request_resampling();
    void write_state(MemoryBlock &data);
    void read_state(const XmlElement &root);
    void mark_state_for_notification();

protected:
    //==========================================================================
    void parameterValueChanged([[maybe_unused]] int index, [[maybe_unused]] float value) override {}
    void parameterGestureChanged([[maybe_unused]] int index, [[maybe_unused]] bool is_starting) override {}
    void parameterValueChangedEx(std::uint32_t tag) override;

private:
    // Made by the first prepareToPlay(), or before it when the host restores a
    // state, or saves one after a parameter has changed. Each prepareToPlay()
    // replaces it with one for its sample rate; releaseResources() keeps it.
    std::unique_ptr<Player> player_;

    // The chip's samples become the host's here rather than in the library
    // (sources/utility/chip_resampler.h). The filter is designed when the player
    // is made, for the chip's rate and the host's; where it cannot be designed --
    // a ratio that does not reduce, which is every host rate against the OPN's --
    // the player is given the host's rate instead and the library interpolates as
    // it did before, and `resampling_why_` is what it said about that.
    Chip_Resampler resampler_;
    // The settings the user chose, the ones the player was made with, and what
    // became of those: a state or the editor changes the first, and the player
    // is made again when they differ from the second (player_matches_settings).
    Resampling_Settings resampling_;
    Resampling_Settings resampling_in_use_;
    Resampling_Status resampling_status_;
    std::string resampling_why_;
    // The chip's rate the player was made for, which a chip type can change.
    unsigned chip_rate_in_use_ = 0;
    // A change the editor asked for that the worker's queue had no room for yet;
    // the audio thread offers it again on the next block.
    std::optional<Resampling_Settings> resampling_to_request_;
    // What the host asked for, which a player made without a prepareToPlay()
    // (a chip type that changes the chip's rate) has to be made for again.
    unsigned host_sample_rate_ = 0;

    std::unique_ptr<Bank_Manager> bank_manager_;

    std::atomic<bool> ready_ {false};

    mutable std::mutex queue_lock_;
    std::shared_ptr<Simple_Fifo> mq_from_ui_;
    std::shared_ptr<Simple_Fifo> mq_to_ui_;

    std::unique_ptr<Simple_Fifo> mq_from_worker_;
    std::unique_ptr<Simple_Fifo> mq_to_worker_;

    // The channel a host did not give. The plugin plays in two and says so, but
    // the buffer is the host's: one that hands over a single channel gets the two
    // mixed into it, played through this one. Prepared with the block the host
    // said it would ask for.
    std::vector<float> spare_channel_;

    std::array<Dc_Filter, 2> dc_filter_;
    std::array<Vu_Monitor, 2> vu_monitor_;
    std::array<std::atomic<double>, 2> lv_current_ {};
    std::atomic<double> cpu_load_ {0.0};

    Atomic_Bit_Set<Cb_Count> pr_changed_;
    Atomic_Bit_Set<Cb_Count> to_notify_;

    // Whether any parameter has changed since the processor was made.
    std::atomic<bool> parameter_changed_ {false};

    std::unique_ptr<Parameter_Block> parameter_block_;

    struct Selection {
        Bank_Id bank {0, 0, false};
        std::uint8_t program = 0;
    };
    std::array<Selection, 16> selection_;

    std::bitset<16> midi_channel_mask_;
    std::array<std::atomic<unsigned>, 16> midi_channel_note_count_ {};
    std::array<Atomic_Bit_Set<128>, 16> midi_channel_note_active_;
    std::array<std::uint8_t, 16> midi_bank_msb_ {};
    std::array<std::uint8_t, 16> midi_bank_lsb_ {};

    unsigned active_part_ = 0;

    static constexpr unsigned bank_title_size_max = 64;
    char bank_title_[bank_title_size_max + 1] {};

    std::mutex player_lock_;

    std::unique_ptr<Worker> worker_;

    class Vst3_Extensions final : public VST3ClientExtensions {
    public:
        explicit Vst3_Extensions(const AudioProcessor &processor) noexcept
            : processor_(processor) {}
        [[nodiscard]] std::map<std::uint32_t, String>
        getCompatibleParameterIds(const VST3Interface::Id &compatible_class) const override;

    private:
        const AudioProcessor &processor_;
    };
    Vst3_Extensions vst3_extensions_ {*this};

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdlplugAudioProcessor)
};

// What a plugin wrapper asks for, and plugin_processor.cc gives it. JUCE
// declares it in juce_audio_plugin_client, which only a plugin build has; a
// test or a fuzz target that builds the processor by itself needs the
// declaration from somewhere, and here it is.
// A build without the plugin client has nowhere else to read it from, as above.
// NOLINTNEXTLINE(readability-redundant-declaration)
AudioProcessor *JUCE_CALLTYPE createPluginFilter();
