//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#pragma once
#include "plugin_state.h"
#include "dsp/dc_filter.h"
#include "dsp/vu_monitor.h"
#include "adl/instrument.h"
#include "adl/chip_settings.h"
#include "utility/processor_ex.h"
#include "utility/atomic_bit_set.h"
#include "JuceHeader.h"
#include <array>
#include <atomic>
#include <bitset>
#include <cstdint>
#include <memory>
#include <mutex>
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
    void panic_nonrt();

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(AudioBuffer<float> &, MidiBuffer &) override;
    void processBlockBypassed(AudioBuffer<float> &, MidiBuffer &) override;

    void process(float *outputs[], unsigned nframes, Midi_Input_Source &midi);

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

private:
    void create_player(unsigned sample_rate);
    void create_first_player(unsigned sample_rate);
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

    std::unique_ptr<Bank_Manager> bank_manager_;

    std::atomic<bool> ready_ {false};

    mutable std::mutex queue_lock_;
    std::shared_ptr<Simple_Fifo> mq_from_ui_;
    std::shared_ptr<Simple_Fifo> mq_to_ui_;

    std::unique_ptr<Simple_Fifo> mq_from_worker_;
    std::unique_ptr<Simple_Fifo> mq_to_worker_;

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

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdlplugAudioProcessor)
};
