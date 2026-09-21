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

#include "adl/player.h"
#include "utility/midi.h"
#include "utility/name_field.h"
#include "utility/simple_fifo.h"
#include "utility/pak.h"
#include "bank_manager.h"
#include "parameter_block.h"
#include "messages.h"
#include "definitions.h"
#include "plugin_processor.h"
#include "worker.h"
#include "resources.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// Copies text into a buffer of `size` bytes, truncated, terminated and zero-filled.
void copy_text(char *buffer, std::size_t size, std::string_view text) noexcept
{
    const std::size_t length = std::min(text.size(), size - 1);
    std::memcpy(buffer, text.data(), length);
    std::memset(buffer + length, 0, size - length);
}

// The sample rate of a player made before prepareToPlay(). It only holds the
// state until prepareToPlay() replaces it.
constexpr unsigned state_only_sample_rate = 44100;

// The rate the plugin runs at, for the rate a host gives it. The rate is the
// host's to choose and nothing checks it on the way in: a host that is only
// looking the plugin over, or that has not settled its audio, can pass none at
// all or a number that is no rate. Everything downstream takes it as a positive
// number -- the library is given the rate, the filters divide by it, and the
// chips are made for it -- so a number that is not one becomes the rate a player
// gets before a host has said anything.
//
// A rate that is a rate is passed on as it is, and where the numbers cannot carry
// it, the nearest they can: a device that runs faster or slower than any that
// exists today is still a device, and a plugin that ran at a rate of its own
// choosing instead would play at the wrong pitch and the wrong speed -- the
// further from the rate it was given, the worse. The plugin hands the rate over
// as a count of samples a second, an unsigned that the library takes as a long,
// so the rates it can carry run from one to the largest number both of those
// hold. Half a sample a second becomes one, which is out by a factor of two;
// the plugin's own rate instead would be out by a factor of ninety thousand.
//
// A number that is no rate at all -- none, less than none, or not a number -- is
// a host that has not said what it will ask for, and the plugin keeps its own
// until the host does.
constexpr double sample_rate_min = 1.0;
constexpr double sample_rate_max = static_cast<double>(
    std::min<std::uint64_t>(std::numeric_limits<unsigned>::max(),
                            static_cast<std::uint64_t>(std::numeric_limits<long>::max())));

unsigned playable_sample_rate(double rate) noexcept
{
    if (!(rate > 0.0))
        return state_only_sample_rate;
    return static_cast<unsigned>(std::clamp(rate, sample_rate_min, sample_rate_max));
}

}  // namespace

//==============================================================================
AdlplugAudioProcessor::AdlplugAudioProcessor()
    : AudioProcessorEx(BusesProperties().withOutput("Output", AudioChannelSet::stereo(), true)),
      parameter_block_(std::make_unique<Parameter_Block>())
{
    parameter_block_->setup_parameters(*this);

    for (AudioProcessorParameter *p : getParameters())
        p->addListener(this);
}

AdlplugAudioProcessor::~AdlplugAudioProcessor()
{
    if (worker_)
        worker_->stop_worker();
}

VST3ClientExtensions *AdlplugAudioProcessor::getVST3ClientExtensions()
{
    return &vst3_extensions_;
}

// The plugins of upstream had the parameters of ADLplug 1, in the same order
// and with the same IDs. Its VST2 plugin numbered them by their index, and its
// VST3 plugin by a hash of the ID, as JUCE's wrapper still does. The parameters
// added since then have higher version hints, and come after those. On Windows
// each class of upstream comes twice, by its ID in memory and by its notation
// (CMakeLists.txt).
std::map<std::uint32_t, String> AdlplugAudioProcessor::Vst3_Extensions::getCompatibleParameterIds(
    const VST3Interface::Id &compatible_class) const
{
    const bool vst2 = compatible_class == VST3Interface::vst2PluginId(JucePlugin_PluginCode, ADLPLUG_UPSTREAM_NAME) ||
                      compatible_class == VST3Interface::hexStringToId(ADLPLUG_UPSTREAM_VST2_CLASS);
    std::map<std::uint32_t, String> ids;
    std::uint32_t index = 0;
    for (const AudioProcessorParameter *parameter : processor_.getParameters()) {
        const auto *with_id = dynamic_cast<const AudioProcessorParameterWithID *>(parameter);
        if (with_id == nullptr)
            continue;
        if (!vst2)
            ids[VST3ClientExtensions::convertJuceParameterId(with_id->paramID)] = with_id->paramID;
        else if (with_id->getVersionHint() == parameter_version_hint)
            ids[index++] = with_id->paramID;
    }
    return ids;
}

//==============================================================================
const String AdlplugAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AdlplugAudioProcessor::acceptsMidi() const
{
    return true;
}

bool AdlplugAudioProcessor::producesMidi() const
{
    return false;
}

bool AdlplugAudioProcessor::isMidiEffect() const
{
    return false;
}

double AdlplugAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AdlplugAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0
    // programs, so this should be at least 1, even if you're not
    // really implementing programs.
}

int AdlplugAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AdlplugAudioProcessor::setCurrentProgram([[maybe_unused]] int index)
{
}

const String AdlplugAudioProcessor::getProgramName([[maybe_unused]] int index)
{
    return {};
}

void AdlplugAudioProcessor::changeProgramName([[maybe_unused]] int index, [[maybe_unused]] const String &new_name)
{
}

//==============================================================================
void AdlplugAudioProcessor::prepareToPlay(double given_sample_rate, int block_size)
{
    const double sample_rate = playable_sample_rate(given_sample_rate);

    ready_.store(false);

    // The channel to play the other half into, for a host that gives the plugin
    // one channel where it asked for two. As long as the block the host says it
    // will ask for, which is what it asks for.
    spare_channel_.assign(static_cast<std::size_t>(std::max(0, block_size)), 0.0f);

    // Stop the worker before replacing the queues it reads and writes.
    if (worker_) {
        worker_->stop_worker();
        worker_.reset();
    }

    {
        constexpr unsigned queue_capacity = 32 * 1024;
        const std::scoped_lock lock(queue_lock_);
        mq_to_ui_ = std::make_shared<Simple_Fifo>(queue_capacity);
        mq_from_ui_ = std::make_shared<Simple_Fifo>(queue_capacity);
        mq_to_worker_ = std::make_unique<Simple_Fifo>(queue_capacity);
        mq_from_worker_ = std::make_unique<Simple_Fifo>(queue_capacity);
    }

    worker_ = std::make_unique<Worker>(*this);
    worker_->start_worker();

    for (std::size_t i = 0; i < 2; ++i) {
        dc_filter_[i].cutoff(5.0 / sample_rate);
        vu_monitor_[i].release(0.5 * sample_rate);
    }

    midi_channel_mask_.set();

    for (unsigned i = 0; i < 16; ++i) {
        midi_channel_note_count_[i].store(0);
        midi_channel_note_active_[i].reset_all();
    }

    {
        const std::scoped_lock lock(player_lock_);
        const auto rate = static_cast<unsigned>(sample_rate);

        if (!player_) {
            create_first_player(rate);
        }
        else {
            // A player runs at the rate it was made for, so a new one takes
            // over the old one's state, with the parameter changes the old one
            // has not taken yet. The parameters are not set from the new
            // player: parts that select the same program would all get the
            // values of the part applied last, and hosts expect a parameter to
            // keep the value they gave it (auval checks that).
            apply_parameter_changes();
            MemoryBlock state;
            write_state(state);
            create_player(rate);
            if (const std::unique_ptr<XmlElement> root = getXmlFromBinary(state.getData(), static_cast<int>(state.getSize())))
                read_state(*root);
        }

        mark_state_for_notification();
    }

    ready_.store(true);

    [[maybe_unused]] const bool sent = Messages::send<Messages::Fx::NotifyReady>(*mq_to_ui_, [](auto &) {});
    assert(sent);
}

void AdlplugAudioProcessor::releaseResources()
{
    if (worker_) {
        worker_->stop_worker();
        worker_.reset();
    }

    ready_.store(false);

    // The player stays, with its state: the host may save or restore the
    // state, or change parameters, before it prepares the processor again.
    const std::scoped_lock lock(queue_lock_);
    mq_from_ui_.reset();
    mq_to_ui_.reset();
    mq_from_worker_.reset();
    mq_to_worker_.reset();
}

std::unique_lock<std::mutex> AdlplugAudioProcessor::acquire_player_nonrt()
{
    return std::unique_lock<std::mutex>(player_lock_);
}

void AdlplugAudioProcessor::set_chip_settings_nonrt(const Chip_Settings &cs)
{
    Player &pl = *player_;
    pl.panic();
    set_player_chip_settings(pl, cs);
}

void AdlplugAudioProcessor::panic_nonrt()
{
    player_->panic();
}

bool AdlplugAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

struct AdlplugAudioProcessor::Message_Handler_Context
{
    bool under_lock = false;
};

void AdlplugAudioProcessor::process(float *outputs[], unsigned nframes, Midi_Input_Source &midi)
{
    float *left = outputs[0];
    float *right = outputs[1];

    std::unique_lock<std::mutex> lock(player_lock_, std::try_to_lock);
    process_messages(lock.owns_lock());

    if (!lock.owns_lock()) {
        // can't use the player while non-rt modifies it
        std::fill_n(left, nframes, 0.0f);
        std::fill_n(right, nframes, 0.0f);
        return;
    }

    process_parameter_changes();
    process_notifications();

    Player &pl = *player_;
    const Parameter_Block &pb = *parameter_block_;
    const ScopedNoDenormals no_denormals;

    const int64 time_before_generate = Time::getHighResolutionTicks();
    for (unsigned iframe = 0; iframe != nframes;) {
        const unsigned segment_nframes = std::min(nframes - iframe, midi_interval_max);
        const bool final_segment = iframe + segment_nframes == nframes;

        // handle events from MIDI
        Midi_Input_Message msg;
        while ((msg = midi.peek_next_event()) &&
               (final_segment || msg.time < static_cast<int>(iframe) ||
                msg.time - static_cast<int>(iframe) < static_cast<int>(midi_interval_max / 2))) {
            handle_midi(msg.data, msg.size);
            midi.get_next_event();
        }

        pl.generate(&left[iframe], &right[iframe], segment_nframes, 1);
        iframe += segment_nframes;
    }
    const int64 time_after_generate = Time::getHighResolutionTicks();
    lock.unlock();

    Dc_Filter &dclf = dc_filter_[0];
    Dc_Filter &dcrf = dc_filter_[1];
    Vu_Monitor &lvu = vu_monitor_[0];
    Vu_Monitor &rvu = vu_monitor_[1];
    double lv_current[2] {};
    const double master_volume = static_cast<double>(pb.p_mastervol->get());
    const double output_gain = Player::output_gain() * master_volume;

    // Only the buffers are single precision.
    for (unsigned i = 0; i < nframes; ++i) {
        double left_sample = static_cast<double>(left[i]) * output_gain;
        double right_sample = static_cast<double>(right[i]) * output_gain;
        // filter out the DC component
        left_sample = dclf.process(left_sample);
        right_sample = dcrf.process(right_sample);
        left[i] = static_cast<float>(left_sample);
        right[i] = static_cast<float>(right_sample);
        lv_current[0] = lvu.process(left_sample);
        lv_current[1] = rvu.process(right_sample);
    }

    lv_current_[0].store(lv_current[0], std::memory_order_relaxed);
    lv_current_[1].store(lv_current[1], std::memory_order_relaxed);

    if (nframes > 0) {
        const double generate_duration = Time::highResolutionTicksToSeconds(time_after_generate - time_before_generate);
        const double buffer_duration = nframes / getSampleRate();
        cpu_load_.store(generate_duration / buffer_duration, std::memory_order_relaxed);
    }
}

void AdlplugAudioProcessor::process_messages(bool under_lock)
{
    Message_Handler_Context ctx;
    ctx.under_lock = under_lock;

    begin_handling_messages(ctx);

    // handle events from GUI
    Simple_Fifo &mq_from_ui = *mq_from_ui_;
    while (const Buffered_Message msg = Messages::read(mq_from_ui)) {
        if (!handle_message(msg, ctx))
            break;
        Messages::finish_read(mq_from_ui, msg);
    }

    // handle events from worker
    Simple_Fifo &mq_from_worker = *mq_from_worker_;
    while (const Buffered_Message msg = Messages::read(mq_from_worker)) {
        if (!handle_message(msg, ctx))
            break;
        Messages::finish_read(mq_from_worker, msg);
    }

    finish_handling_messages(ctx);
}

void AdlplugAudioProcessor::process_parameter_changes()
{
    if (unmark_parameter_as_changed(Cb_ChipSettings)) {
        // The parameters keep the chip settings as they were given; compare
        // what the player would run for them.
        const Chip_Settings cs = parameter_block_->chip_settings();
        if (playable_chip_settings(cs) != get_player_chip_settings(*player_)) {
            if (Messages::send<Messages::Fx::RequestChipSettings>(*mq_to_worker_, [&cs](auto &body) { body.cs = cs; }))
                worker_->postSemaphore();
            else
                mark_parameter_as_changed(Cb_ChipSettings);  // do later
        }
    }

    apply_parameter_changes();
}

// Passes changes of the instrument and global parameters on to the player: on
// the audio thread for each block, and before the state is saved or the player
// replaced. The player lock is held.
void AdlplugAudioProcessor::apply_parameter_changes()
{
    for (unsigned p = 0; p < 16; ++p) {
        if (unmark_parameter_as_changed(Cb_Instrument1 + p))
            load_instrument_from_parameters(p);
    }

    if (unmark_parameter_as_changed(Cb_GlobalParameters))
        load_global_parameters_from_parameters();
}

void AdlplugAudioProcessor::load_instrument_from_parameters(unsigned part_number)
{
    // The parameters hold most of an instrument. The rest, such as the
    // rhythm-mode drum type of OPL3 percussion, stays as the program has it.
    Bank_Manager &bm = *bank_manager_;
    const Selection &sel = selection_[part_number];
    Instrument current;
    bm.find_program(sel.bank, sel.program, current);
    const Instrument ins = parameter_block_->part[part_number].instrument(current);
    bm.load_program(
        sel.bank, sel.program, ins,
        Bank_Manager::LP_Notify | Bank_Manager::LP_NeedMeasurement | Bank_Manager::LP_KeepName);
}

void AdlplugAudioProcessor::load_global_parameters_from_parameters()
{
    Player &pl = *player_;
    const Instrument_Global_Parameters gp = parameter_block_->global_parameters();
    if (gp != get_player_global_parameters(pl)) {
        set_player_global_parameters(pl, gp);
        mark_for_notification(Cb_GlobalParameters);
    }
}

// A notification that does not fit in the queue stays marked for the next cycle.
void AdlplugAudioProcessor::process_notifications()
{
    const Player &pl = *player_;
    Simple_Fifo &queue = *mq_to_ui_;

    if (unmark_for_notification(Cb_ChipSettings)) {
        if (!Messages::send<Messages::Fx::NotifyChipSettings>(queue, [&pl](auto &body) {
                body.cs = get_player_chip_settings(pl);
            }))
            mark_for_notification(Cb_ChipSettings);
    }

    if (unmark_for_notification(Cb_GlobalParameters)) {
        if (!Messages::send<Messages::Fx::NotifyGlobalParameters>(queue, [&pl](auto &body) {
                body.param = get_player_global_parameters(pl);
            }))
            mark_for_notification(Cb_GlobalParameters);
    }

    if (unmark_for_notification(Cb_ActivePart)) {
        if (!Messages::send<Messages::Fx::NotifyActivePart>(queue, [this](auto &body) {
                body.part = active_part_;
            }))
            mark_for_notification(Cb_ActivePart);
    }

    if (unmark_for_notification(Cb_BankTitle)) {
        if (!Messages::send<Messages::Fx::NotifyBankTitle>(queue, [this](auto &body) {
                static_assert(sizeof body.title <= sizeof bank_title_);
                std::memcpy(body.title, bank_title_, sizeof body.title);
            }))
            mark_for_notification(Cb_BankTitle);
    }

    for (unsigned part = 0; part < 16; ++part) {
        if (unmark_for_notification(Cb_Selection1 + part)) {
            if (!Messages::send<Messages::Fx::NotifySelection>(queue, [this, part](auto &body) {
                    body.part = part;
                    body.bank = selection_[part].bank;
                    body.program = selection_[part].program;
                }))
                mark_for_notification(Cb_Selection1 + part);
        }
    }
}

bool AdlplugAudioProcessor::handle_midi(const std::uint8_t *data, unsigned len)
{
    const unsigned status = (len > 0) ? data[0] : 0;

    if (status == 0xf0) {
        // A System Exclusive message the host sends. When the library acts on
        // it, it may have reset the channels (GM, GS or XG), which stops every
        // note; which of the messages it was is not reported, so the notes the
        // editor shows start again from zero and the next note puts them right.
        // The instrument of each part is the plugin's own and no reset touches
        // it.
        if (player_->play_sysex(data, len)) {
            for (unsigned part = 0; part < 16; ++part) {
                midi_channel_note_count_[part].store(0, std::memory_order_relaxed);
                midi_channel_note_active_[part].reset_all(std::memory_order_relaxed);
            }
        }
        return true;
    }

    player_->play_midi(data, len);

    const unsigned channel = status & 0x0f;

    if ((status & 0xf0) != 0xf0 && !midi_channel_mask_[channel])
        return true;

    switch (status & 0xf0) {
    case 0x90:
        if (len < 3)
            break;
        if (data[2] > 0) {
            if (!midi_channel_note_active_[channel].set(data[1] & 0x7fu))
                midi_channel_note_count_[channel].fetch_add(1, std::memory_order_relaxed);
            break;
        }
        [[fallthrough]];  // a note-on with velocity 0 is a note-off
    case 0x80:
        if (len < 3)
            break;
        if (midi_channel_note_active_[channel].reset(data[1] & 0x7fu))
            midi_channel_note_count_[channel].fetch_sub(1, std::memory_order_relaxed);
        break;
    case 0xb0:
        if (len < 3)
            break;
        switch (data[1]) {
        case 0:
            midi_bank_msb_[channel] = data[2];
            break;
        case 32:
            midi_bank_lsb_[channel] = data[2];
            break;
        case 120:
        case 123:
            midi_channel_note_count_[channel].store(0, std::memory_order_relaxed);
            midi_channel_note_active_[channel].reset_all(std::memory_order_relaxed);
            break;
        default:
            break;
        }
        break;
    case 0xc0: {
        if (len < 2)
            break;
        const bool is_drum = channel == 9;
        if (!is_drum) {
            Selection &sel = selection_[channel];
            sel.program = static_cast<std::uint8_t>(data[1] & 0x7f);
            sel.bank.percussive = 0u;
            sel.bank.msb = midi_bank_msb_[channel];
            sel.bank.lsb = midi_bank_lsb_[channel];
        }
        else {
            //--- TODO percussion banks/XG banks?
            // selection_[channel].bank.percussive = true;
            // selection_[channel].bank.msb = 0;
            // selection_[channel].bank.lsb = data[1];
        }
        mark_for_notification(Cb_Selection1 + channel);
        set_instrument_parameters_notifying_host(channel);
        break;
    }
    default:
        break;
    }

    return true;
}

bool AdlplugAudioProcessor::handle_message(const Buffered_Message &msg, Message_Handler_Context &ctx)
{
    // While another thread holds the player lock, messages wait in their
    // queues; MIDI from the editor too, as it goes to the player.
    if (!ctx.under_lock)
        return false;

    const unsigned tag = msg.header.tag;

    if (tag == std::to_underlying(User_Message::Midi))
        return handle_midi(msg.body.data(), msg.header.size);

    Player &pl = *player_;
    Bank_Manager &bm = *bank_manager_;

    switch (tag) {
    case std::to_underlying(User_Message::RequestBankSlots):
        bm.mark_slots_for_notification();
        break;
    case std::to_underlying(User_Message::RequestFullBankState):
        mark_for_notification(Cb_GlobalParameters);
        bm.mark_everything_for_notification();
        break;
    case std::to_underlying(User_Message::RequestChipSettings):
        mark_for_notification(Cb_ChipSettings);
        break;
    case std::to_underlying(User_Message::RequestSelections): {
        const auto body = Messages::body<Messages::User::RequestSelections>(msg);
        for (unsigned p = 0; p < 16; ++p) {
            if (body.channel_mask.test(p))
                mark_for_notification(Cb_Selection1 + p);
        }
        break;
    }
    case std::to_underlying(User_Message::RequestActivePart):
        mark_for_notification(Cb_ActivePart);
        break;
    case std::to_underlying(User_Message::RequestBankTitle):
        mark_for_notification(Cb_BankTitle);
        break;
    case std::to_underlying(User_Message::ClearBanks): {
        const auto body = Messages::body<Messages::User::ClearBanks>(msg);
        bm.clear_banks(body.notify_back);
        break;
    }
    case std::to_underlying(User_Message::LoadGlobalParameters): {
        const auto body = Messages::body<Messages::User::LoadGlobalParameters>(msg);
        if (body.param != get_player_global_parameters(pl)) {
            set_player_global_parameters(pl, body.param);
            if (body.notify_back)
                mark_for_notification(Cb_GlobalParameters);
        }
        break;
    }
    case std::to_underlying(User_Message::LoadInstrument): {
        const auto body = Messages::body<Messages::User::LoadInstrument>(msg);
        const unsigned flags =
            (body.need_measurement ? Bank_Manager::LP_NeedMeasurement : 0u) |
            (body.notify_back ? Bank_Manager::LP_Notify : 0u);
        if (bm.load_program(body.bank, body.program, body.instrument, flags) && body.part < selection_.size()) {
            const Selection &sel = selection_[body.part];
            if (body.bank == sel.bank && body.program == sel.program)
                set_instrument_parameters_notifying_host(body.part);
        }
        break;
    }
    case std::to_underlying(User_Message::CreateInstrument): {
        const auto body = Messages::body<Messages::User::CreateInstrument>(msg);
        const unsigned flags = Bank_Manager::LP_NoReplaceExisting |
            (body.notify_back ? Bank_Manager::LP_Notify : 0u);
        Instrument ins;
        ins.blank(false);
        bm.load_program(body.bank, body.program, ins, flags);
        break;
    }
    case std::to_underlying(User_Message::DeleteInstrument): {
        const auto body = Messages::body<Messages::User::DeleteInstrument>(msg);
        bm.delete_program(body.bank, body.program, body.notify_back ? Bank_Manager::LP_Notify : 0u);
        break;
    }
    case std::to_underlying(User_Message::DeleteBank): {
        const auto body = Messages::body<Messages::User::DeleteBank>(msg);
        bm.delete_bank(body.bank, body.notify_back ? Bank_Manager::LP_Notify : 0u);
        break;
    }
    case std::to_underlying(User_Message::RenameBank): {
        const auto body = Messages::body<Messages::User::RenameBank>(msg);
        bm.rename_bank(body.bank, body.name, body.notify_back);
        break;
    }
    case std::to_underlying(User_Message::RenameProgram): {
        const auto body = Messages::body<Messages::User::RenameProgram>(msg);
        bm.rename_program(body.bank, body.program, body.name, body.notify_back);
        break;
    }
    case std::to_underlying(User_Message::SelectProgram): {
        const auto body = Messages::body<Messages::User::SelectProgram>(msg);
        // A program the plugin has no place for is no selection: a selection goes
        // into the state of a project, and reading a state takes only a program
        // that is one, so a selection that was never a program would be a project
        // that changes by being opened and saved.
        if (body.part >= selection_.size() || body.program >= Bank_Manager::program_count)
            break;
        Selection &sel = selection_[body.part];
        if (sel.bank != body.bank || sel.program != body.program) {
            sel.bank = body.bank;
            sel.program = body.program;
            send_program_change_from_selection(body.part);
            set_instrument_parameters_notifying_host(body.part);
        }
        break;
    }
    case std::to_underlying(User_Message::SetActivePart): {
        const auto body = Messages::body<Messages::User::SetActivePart>(msg);
        if (active_part_ == body.part || body.part >= 16)
            break;
        active_part_ = body.part;
        mark_for_notification(Cb_ActivePart);
        break;
    }
    case std::to_underlying(User_Message::SetBankTitle): {
        const auto body = Messages::body<Messages::User::SetBankTitle>(msg);
        // The title comes in as bytes and a project keeps it as text, so it is
        // made text here and kept as the text that fits, for the reason the
        // names of banks and programs are (bank_manager.cc, assign_name). The
        // terminator goes after its last byte.
        static_assert(sizeof body.title == bank_title_size_max && sizeof bank_title_ == bank_title_size_max + 1);
        name_from_field(std::span(body.title, sizeof body.title))
            .copyToUTF8(bank_title_, bank_title_size_max + 1);
        break;
    }
#if defined(ADLPLUG_OPL3)
    case std::to_underlying(User_Message::SelectOptimal4Ops): {
        const Parameter_Block &pb = *parameter_block_;
        pl.panic();
        pl.set_num_4ops(~0u);
        *pb.p_n4op = static_cast<int>(pl.num_4ops());
        mark_for_notification(Cb_ChipSettings);
        break;
    }
#endif
    case std::to_underlying(Worker_Message::MeasurementResult): {
        const auto body = Messages::body<Messages::Worker::MeasurementResult>(msg);
        bm.load_measurement(body.bank, body.program, body.instrument, body.ms_sound_kon, body.ms_sound_koff, true);
        break;
    }
    default:
        assert(false);
        break;
    }

    return true;
}

void AdlplugAudioProcessor::finish_handling_messages(Message_Handler_Context &ctx)
{
    // The bank manager belongs to whoever holds the player lock.
    if (!ctx.under_lock)
        return;

    bank_manager_->send_notifications();
    bank_manager_->send_measurement_requests();
}

void AdlplugAudioProcessor::set_instrument_parameters_notifying_host(unsigned part_number)
{
    Instrument ins;
    const Selection &sel = selection_[part_number];

    if (!bank_manager_->find_program(sel.bank, sel.program, ins))
        return;

    parameter_block_->part[part_number].set_instrument(ins);
}

void AdlplugAudioProcessor::send_program_change_from_selection(unsigned part)
{
    const bool is_drum = part == 9;
    const Selection sel = selection_[part];

    if (is_drum != (sel.bank.percussive != 0))
        return;

    Player &pl = *player_;
    const auto status = [part](unsigned kind) { return static_cast<std::uint8_t>(kind | part); };
    if (!is_drum) {
        // melodic bank change
        const std::uint8_t bank_msb[3] {status(0xb0), 0, sel.bank.msb};
        pl.play_midi(bank_msb, 3);
        const std::uint8_t bank_lsb[3] {status(0xb0), 32, sel.bank.lsb};
        pl.play_midi(bank_lsb, 3);
        // melodic program change
        const std::uint8_t program[2] {status(0xc0), sel.program};
        pl.play_midi(program, 2);
    }
    else {
        // percussion bank change LSB only
        const std::uint8_t program[2] {status(0xc0), sel.bank.lsb};
        pl.play_midi(program, 2);
    }
}

void AdlplugAudioProcessor::processBlock(AudioBuffer<float> &buffer,
                                         MidiBuffer &midi_messages)
{
    // The plugin is a stereo one and says so (isBusesLayoutSupported), but the
    // buffer is the host's. With no channel at all there is nowhere to play.
    const int channels = buffer.getNumChannels();
    if (channels < 1)
        return;

    const auto nframes = static_cast<unsigned>(buffer.getNumSamples());

    Midi_Input_Source::Buffer_Cursor midi_cursor {.current = midi_messages.begin(), .end = midi_messages.end()};
    Midi_Input_Source midi_source(midi_cursor);

    if (channels >= 2) {
        float *outputs[2] {buffer.getWritePointer(0), buffer.getWritePointer(1)};
        process(outputs, nframes, midi_source);
    }
    else {
        // One channel: the plugin plays its two, the second into a channel of its
        // own, and mixes them into the one it was given -- half of each, so that
        // what was in the middle keeps its loudness. The channel of its own is as
        // long as the block the host said it would ask for; a host that asks for
        // more than that gets it in pieces rather than nothing, since growing a
        // buffer here is not a thing to do while the audio waits.
        const unsigned piece = static_cast<unsigned>(spare_channel_.size());
        if (piece == 0) {
            buffer.clear();
            return;
        }

        float *const mono = buffer.getWritePointer(0);
        for (unsigned at = 0; at < nframes; at += piece) {
            const unsigned now = std::min(piece, nframes - at);
            float *outputs[2] {mono + at, spare_channel_.data()};
            process(outputs, now, midi_source);
            for (unsigned i = 0; i < now; ++i)
                mono[at + i] = 0.5f * (mono[at + i] + spare_channel_[i]);
        }
    }

    // What the plugin does not play in, it leaves silent: a host asks that of a
    // plugin for every output channel it hands over.
    for (int channel = 2; channel < channels; ++channel)
        buffer.clear(channel, 0, static_cast<int>(nframes));
}

void AdlplugAudioProcessor::processBlockBypassed(AudioBuffer<float> &buffer, MidiBuffer &midi_messages)
{
    {
        // The lock goes when the scope does. Unlocking it by hand would throw
        // when the try had not got it -- which is whenever another thread holds
        // the player, as the worker does while it measures an instrument or
        // changes the chips -- and nothing catches that.
        const std::unique_lock<std::mutex> lock(player_lock_, std::try_to_lock);
        process_messages(lock.owns_lock());
    }

    cpu_load_.store(0.0, std::memory_order_relaxed);

    // JUCE's own bypass clears a channel for every output the plugin has, which
    // is not how many the buffer may hold: a host that hands over fewer would
    // have it clear a channel that is not there. What the buffer does have is
    // cleared here instead, since a synthesiser that is bypassed has nothing to
    // pass through.
    if (buffer.getNumChannels() < getTotalNumOutputChannels()) {
        buffer.clear();
        return;
    }

    AudioProcessor::processBlockBypassed(buffer, midi_messages);
}

// hasEditor() and createEditor() are in plugin_editor.cc, where the editor is:
// this file is then the processor alone, and a test or a fuzz target can build
// it without the interface and the JUCE modules the interface needs.

//==============================================================================
void AdlplugAudioProcessor::getStateInformation(MemoryBlock &data)
{
    const std::scoped_lock lock(player_lock_);

    // Without a player, and while no parameter has changed, the state is the
    // default one, which an empty block stands for.
    if (!player_ && !parameter_changed_.load()) {
        data.reset();
        return;
    }

    if (!player_)
        create_first_player(state_only_sample_rate);

    // The host may save before the audio thread has passed changes on.
    apply_parameter_changes();
    write_state(data);
}

void AdlplugAudioProcessor::setStateInformation(const void *data, int size)
{
    // What the host hands over is the host's: no state at all is a pair that says
    // nothing, and a size that promises bytes which are not there is one nothing
    // can read. JUCE reads the first bytes of a state to see whether it is one,
    // and that is a read of the pointer.
    if (data == nullptr || size <= 0)
        return;

    const std::scoped_lock lock(player_lock_);

    const std::unique_ptr<XmlElement> root = getXmlFromBinary(data, size);
    if (!root || root->getTagName() != "ADLMIDI-state")
        return;

    // The state replaces the parameter changes that came before it.
    for (unsigned p = 0; p < 16; ++p)
        unmark_parameter_as_changed(Cb_Instrument1 + p);
    unmark_parameter_as_changed(Cb_GlobalParameters);

    if (!player_)
        create_first_player(state_only_sample_rate);
    read_state(*root);

    // make the host aware of changed parameters
    parameter_block_->set_global_parameters(get_player_global_parameters(*player_));
    for (unsigned p = 0; p < 16; ++p)
        set_instrument_parameters_notifying_host(p);
}

// Replaces the player with one in the default state: the default bank and
// selections, and the chip settings of the parameters. The player lock is held.
void AdlplugAudioProcessor::create_player(unsigned sample_rate)
{
    Pak_File_Reader pak;
    [[maybe_unused]] const bool pak_ok = pak.init_with_data(Res::banks_pak.data, Res::banks_pak.size);
    assert(pak_ok);
    std::vector<std::uint8_t> default_wopl = pak.extract(0);
    assert(!default_wopl.empty());

    auto pl = std::make_unique<Player>();
    pl->init(sample_rate);
    pl->reserve_banks(bank_reserve_size);
    pl->set_soft_pan_enabled(true);
    set_player_chip_settings(*pl, parameter_block_->chip_settings());

    auto bm = std::make_unique<Bank_Manager>(*this, *pl, default_wopl);

    // The old bank manager refers to the old player, so it goes first.
    bank_manager_ = std::move(bm);
    player_ = std::move(pl);

    for (unsigned p = 0; p < 16; ++p) {
        const bool percussive = p == 9;
        selection_[p] = Selection{.bank = Bank_Id(0, 0, percussive),
                                  .program = static_cast<std::uint8_t>(percussive ? 35 : 0)};
    }

    active_part_ = 0;
    copy_text(bank_title_, sizeof bank_title_, pak.name(0));
}

// Makes the first player and settles the parameters with it. Until then the
// parameters have their defaults, which describe the default state, or values
// that the host or the editor gave them. Those go to the player; the other
// instrument and global parameters take the player's values. The player lock
// is held.
void AdlplugAudioProcessor::create_first_player(unsigned sample_rate)
{
    create_player(sample_rate);

    for (unsigned p = 0; p < 16; ++p) {
        if (unmark_parameter_as_changed(Cb_Instrument1 + p))
            load_instrument_from_parameters(p);
        else
            set_instrument_parameters_notifying_host(p);
    }

    if (unmark_parameter_as_changed(Cb_GlobalParameters))
        load_global_parameters_from_parameters();
    else
        parameter_block_->set_global_parameters(get_player_global_parameters(*player_));
}

// Writes the state of the player, with the chip settings and the master volume
// of the parameters. The player lock is held.
void AdlplugAudioProcessor::write_state(MemoryBlock &data)
{
    Player &pl = *player_;
    const Parameter_Block &pb = *parameter_block_;
    const Bank_Manager &bm = *bank_manager_;

    XmlElement root("ADLMIDI-state");

    // A slot whose programs are all blank is not a bank the plugin shows or
    // keeps: the editor's list of banks leaves it out, and the next bank to be
    // loaded takes the slot (Bank_Manager::emit_slots, find_empty_slot).
    // Writing it here would put a bank into the state that reading the state
    // cannot bring back, since a bank comes back with the instruments in it;
    // the project would then change by being opened and saved, which
    // fuzz/state.cc found.
    const auto is_a_bank = [](const Bank_Manager::Bank_Info &info)
        { return static_cast<bool>(info) && info.used.any(); };

    for (const Bank_Manager::Bank_Info &info : bm.bank_infos()) {
        if (!is_a_bank(info))
            continue;
        PropertySet bank_set;
        bank_set.setValue("bank", static_cast<int>(info.id.to_integer()));
        bank_set.setValue("name", name_from_field(info.bank_name));
        root.addChildElement(bank_set.createXml("bank").release());
    }

    for (const Bank_Manager::Bank_Info &info : bm.bank_infos()) {
        if (!is_a_bank(info))
            continue;
        Instrument ins;
        for (unsigned p_i = 0; p_i < 128; ++p_i) {
            if (!info.used.test(p_i))
                continue;
            pl.ensure_get_instrument(info.bank, p_i, ins);
            PropertySet ins_set = ins.to_properties();
            ins_set.setValue("bank", static_cast<int>(info.id.to_integer()));
            ins_set.setValue("program", static_cast<int>(p_i));
            ins_set.setValue("name", name_from_field(info.program_name(p_i)));
            root.addChildElement(ins_set.createXml("instrument").release());
        }
    }

    for (unsigned p = 0; p < 16; ++p) {
        const Selection &sel = selection_[p];
        PropertySet sel_set;
        sel_set.setValue("part", static_cast<int>(p));
        sel_set.setValue("bank", static_cast<int>(sel.bank.to_integer()));
        sel_set.setValue("program", static_cast<int>(sel.program));
        root.addChildElement(sel_set.createXml("selection").release());
    }

    // The chip settings as the parameters have them, which the player may run
    // otherwise (playable_chip_settings()).
    root.addChildElement(pb.chip_settings().to_properties().createXml("chip").release());
    root.addChildElement(get_player_global_parameters(pl).to_properties().createXml("global").release());

    PropertySet common_set;
    common_set.setValue("bank_title", name_from_field(std::span(bank_title_, bank_title_size_max)));
    common_set.setValue("part", static_cast<int>(active_part_));
    common_set.setValue("master_volume", static_cast<double>(pb.p_mastervol->get()));
    root.addChildElement(common_set.createXml("common").release());

    copyXmlToBinary(root, data);
}

// Reads a state into the player, and its chip settings and master volume into
// the parameters. The player lock is held.
void AdlplugAudioProcessor::read_state(const XmlElement &root)
{
    Player &pl = *player_;
    Parameter_Block &pb = *parameter_block_;
    Bank_Manager &bm = *bank_manager_;

    bm.clear_banks(false);

    for (const XmlElement *elt : root.getChildWithTagNameIterator("instrument")) {
        PropertySet ins_set;
        ins_set.restoreFromXml(*elt);
        const Bank_Id bank = Bank_Id::from_integer(static_cast<std::uint32_t>(ins_set.getIntValue("bank")));
        const int program = ins_set.getIntValue("program");
        if (program < 0 || program > 127)
            continue;
        Instrument ins = Instrument::from_properties(ins_set);
        copy_name_to_field(ins.name, ins_set.getValue("name"));
        bm.load_program(bank, static_cast<unsigned>(program), ins, 0);
    }

    for (const XmlElement *elt : root.getChildWithTagNameIterator("bank")) {
        PropertySet bank_set;
        bank_set.restoreFromXml(*elt);
        const Bank_Id bank = Bank_Id::from_integer(static_cast<std::uint32_t>(bank_set.getIntValue("bank")));
        bm.rename_bank(bank, bank_set.getValue("name").toRawUTF8(), false);
    }

    for (const XmlElement *elt : root.getChildWithTagNameIterator("selection")) {
        PropertySet sel_set;
        sel_set.restoreFromXml(*elt);
        const int part = sel_set.getIntValue("part");
        const int program = sel_set.getIntValue("program");
        if (part < 0 || part > 15 || program < 0 || program > 127)
            continue;
        Selection &sel = selection_[static_cast<std::size_t>(part)];
        sel.bank = Bank_Id::from_integer(static_cast<std::uint32_t>(sel_set.getIntValue("bank")));
        sel.program = static_cast<std::uint8_t>(program);
    }

    // chip settings
    if (const XmlElement *elt = root.getChildByName("chip")) {
        PropertySet set;
        set.restoreFromXml(*elt);
        pb.set_chip_settings(Chip_Settings::from_properties(set));
    }
    set_player_chip_settings(pl, pb.chip_settings());

    // global parameters
    if (const XmlElement *elt = root.getChildByName("global")) {
        PropertySet set;
        set.restoreFromXml(*elt);
        set_player_global_parameters(pl, Instrument_Global_Parameters::from_properties(set));
    }

    // common parameters
    PropertySet common_set;
    if (const XmlElement *elt = root.getChildByName("common"))
        common_set.restoreFromXml(*elt);
    common_set.getValue("bank_title").copyToUTF8(bank_title_, bank_title_size_max + 1);
    active_part_ = static_cast<unsigned>(jlimit(0, 15, common_set.getIntValue("part")));
    *pb.p_mastervol = static_cast<float>(common_set.getDoubleValue("master_volume", 1.0));

    mark_state_for_notification();

    // send program changes
    for (unsigned p = 0; p < 16; ++p)
        send_program_change_from_selection(p);
}

// Marks everything the editor shows, for sending to it.
void AdlplugAudioProcessor::mark_state_for_notification()
{
    mark_for_notification(Cb_ChipSettings);
    mark_for_notification(Cb_GlobalParameters);
    bank_manager_->mark_everything_for_notification();
    for (unsigned p = 0; p < 16; ++p)
        mark_for_notification(Cb_Selection1 + p);
    mark_for_notification(Cb_ActivePart);
    mark_for_notification(Cb_BankTitle);
}

//==============================================================================
void AdlplugAudioProcessor::parameterValueChangedEx(std::uint32_t tag)
{
    parameter_changed_.store(true, std::memory_order_relaxed);

    if (tag == Parameter_Tag::chip)
        mark_parameter_as_changed(Cb_ChipSettings);
    else if (tag == Parameter_Tag::global)
        mark_parameter_as_changed(Cb_GlobalParameters);
    else if (Parameter_Tag::is_instrument(tag))
        mark_parameter_as_changed(Cb_Instrument1 + (Parameter_Tag::part_of(tag) & 15));
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new AdlplugAudioProcessor();
}
