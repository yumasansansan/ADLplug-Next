// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// The audio device path in double precision, which patches/JUCE adds: the
// sample conversions a device makes between its own format and doubles, and the
// way from a device through the device manager and the processor player to a
// processor and back, driven here by a device made for the test, since the real
// ones need hardware. And one conversion of floats that patches/JUCE mends on
// the way: a negative float written into 24 bits of 32.

#include "test.h"
#include "JuceHeader.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Conversions

// Values in and past the range a device takes, most of them ones a float
// cannot hold, none of them half-way between two integers at the widths below.
constexpr std::array<double, 12> k_values {
    0.0, 0.1, -0.1, 1.0 / 3.0, -2.0 / 3.0, 0.999999999, -0.999999999, 1.0, -1.0, 1.5, -1.5, 1e-10,
};

// A double at the width of an integer format of `bits`, as the patch writes it:
// scaled by the power of two the format is read back with, rounded once to the
// nearest integer and limited to the largest magnitude the format keeps.
std::int64_t at_width(double value, int bits)
{
    const auto scale = std::ldexp(1.0, bits - 1);
    const auto limit = static_cast<std::int64_t>(scale) - 1;
    return std::clamp<std::int64_t>(std::llround(std::clamp(value, -1.0, 1.0) * scale), -limit, limit);
}

std::int64_t little_endian(const std::uint8_t *bytes, int size)
{
    std::uint64_t value = 0;
    for (int i = size; --i >= 0;)
        value = (value << 8) | bytes[i];
    // Sign-extended from the format's width.
    const auto shift = 64 - 8 * size;
    return static_cast<std::int64_t>(value << shift) >> shift;
}

using Float64Source = juce::AudioData::Pointer<juce::AudioData::Float64, juce::AudioData::NativeEndian,
                                               juce::AudioData::NonInterleaved, juce::AudioData::Const>;
using Float64Dest = juce::AudioData::Pointer<juce::AudioData::Float64, juce::AudioData::NativeEndian,
                                             juce::AudioData::NonInterleaved, juce::AudioData::NonConst>;
using Float32Source = juce::AudioData::Pointer<juce::AudioData::Float32, juce::AudioData::NativeEndian,
                                               juce::AudioData::NonInterleaved, juce::AudioData::Const>;

// Every value into a little-endian integer format of `size` bytes holding
// `bits`, read back, and written again from what was read.
template <class Format>
void check_integer_format(int size, int bits)
{
    using Dest = juce::AudioData::Pointer<Format, juce::AudioData::LittleEndian,
                                          juce::AudioData::NonInterleaved, juce::AudioData::NonConst>;
    using Source = juce::AudioData::Pointer<Format, juce::AudioData::LittleEndian,
                                            juce::AudioData::NonInterleaved, juce::AudioData::Const>;
    const auto count = static_cast<int>(k_values.size());
    const auto stride = static_cast<std::size_t>(size);
    std::vector<std::uint8_t> bytes(k_values.size() * stride);
    Dest {bytes.data()}.convertSamples(Float64Source {k_values.data()}, count);

    std::vector<double> back(k_values.size());
    Float64Dest {back.data()}.convertSamples(Source {bytes.data()}, count);

    std::vector<std::uint8_t> again(bytes.size());
    Dest {again.data()}.convertSamples(Float64Source {back.data()}, count);

    for (std::size_t i = 0; i < k_values.size(); ++i) {
        const auto written = little_endian(&bytes[i * stride], size);
        CHECK(written == at_width(k_values[i], bits));
        // Read back exactly, an integer over a power of two.
        CHECK(back[i] == std::ldexp(static_cast<double>(written), 1 - bits));
    }
    // And written again as the same integers.
    CHECK(again == bytes);
}

}  // namespace

ADLPLUG_TEST(audio_data_doubles)
{
    // Each integer format rounded once at its width, read back as the exact
    // fraction it is and written again as the same integer; 24 bits in 32
    // stored as the signed number they are.
    check_integer_format<juce::AudioData::Int16>(2, 16);
    check_integer_format<juce::AudioData::Int24>(3, 24);
    check_integer_format<juce::AudioData::Int32>(4, 32);
    check_integer_format<juce::AudioData::Int24in32>(4, 24);

    // 24 bits in 32 read from the low three bytes, whatever the fourth holds: a
    // device may leave it zero, and -0.5 is then 0xc00000 in the low three and
    // nothing above.
    {
        using Source = juce::AudioData::Pointer<juce::AudioData::Int24in32, juce::AudioData::LittleEndian,
                                                juce::AudioData::NonInterleaved, juce::AudioData::Const>;
        const std::uint8_t zero_above[4] {0x00, 0x00, 0xc0, 0x00};
        double half = 0.0;
        Float64Dest {&half}.convertSamples(Source {zero_above}, 1);
        CHECK(half == -0.5);
    }

    // To single precision, rounded once, and back exactly.
    {
        using Dest = juce::AudioData::Pointer<juce::AudioData::Float32, juce::AudioData::LittleEndian,
                                              juce::AudioData::NonInterleaved, juce::AudioData::NonConst>;
        std::vector<float> floats(k_values.size());
        Dest {floats.data()}.convertSamples(Float64Source {k_values.data()}, static_cast<int>(k_values.size()));
        for (std::size_t i = 0; i < k_values.size(); ++i)
            CHECK(floats[i] == static_cast<float>(k_values[i]));
    }

    // Where double precision is the point: 0.1 at 32 bits, rounded once, and
    // the float nearest 0.1 converted as floats are, more than one step away.
    {
        using Dest = juce::AudioData::Pointer<juce::AudioData::Int32, juce::AudioData::LittleEndian,
                                              juce::AudioData::NonInterleaved, juce::AudioData::NonConst>;
        const double tenth[1] {0.1};
        const float tenth_as_float[1] {0.1f};
        std::uint8_t through_double[4] {};
        std::uint8_t through_float[4] {};
        Dest {through_double}.convertSamples(Float64Source {tenth}, 1);
        Dest {through_float}.convertSamples(Float32Source {tenth_as_float}, 1);
        CHECK(little_endian(through_double, 4) == std::llround(0.1 * 2147483648.0));
        CHECK(std::llabs(little_endian(through_double, 4) - little_endian(through_float, 4)) > 1);
    }

    // Into one channel of an interleaved 24-bit stereo buffer, as a device's
    // converter writes it, leaving the other channel alone.
    {
        using Interleaved = juce::AudioData::Pointer<juce::AudioData::Int24, juce::AudioData::LittleEndian,
                                                     juce::AudioData::Interleaved, juce::AudioData::NonConst>;
        const juce::AudioData::ConverterInstance<Float64Source, Interleaved> converter(1, 2);
        std::vector<std::uint8_t> bytes(k_values.size() * 6, 0xa5);
        converter.convertSamples(bytes.data(), 1, k_values.data(), 0, static_cast<int>(k_values.size()));
        for (std::size_t i = 0; i < k_values.size(); ++i) {
            CHECK(little_endian(&bytes[i * 6 + 3], 3) == at_width(k_values[i], 24));
            CHECK(bytes[i * 6] == 0xa5 && bytes[i * 6 + 1] == 0xa5 && bytes[i * 6 + 2] == 0xa5);
        }
    }

    // And single precision into an integer format as it always went: through
    // the 32-bit integer, the low bits cut off.
    {
        const float values[3] {0.1f, -0.3f, 0.7f};
        using Dest = juce::AudioData::Pointer<juce::AudioData::Int16, juce::AudioData::LittleEndian,
                                              juce::AudioData::NonInterleaved, juce::AudioData::NonConst>;
        std::uint8_t bytes[6] {};
        Dest {bytes}.convertSamples(Float32Source {values}, 3);
        for (std::size_t i = 0; i < 3; ++i) {
            const auto as_int32 = juce::roundToInt(static_cast<double>(values[i]) * 2147483647.0);
            CHECK(little_endian(&bytes[i * 2], 2) == (as_int32 >> 16));
        }
    }
}

// Floats into 24 bits of 32 through setAsFloat(), which scales them by the
// format's largest value and truncates: a negative sample is written as the
// signed number it is, as a positive one is. patches/JUCE converts the scaled
// value through int32; JUCE converted it straight to uint32, which for a
// negative value is undefined, and which AVX-512's and AArch64's instructions
// saturate, to 0xffffffff and to 0.
ADLPLUG_TEST(audio_data_24_in_32_from_floats)
{
    using Dest = juce::AudioData::Pointer<juce::AudioData::Int24in32, juce::AudioData::LittleEndian,
                                          juce::AudioData::NonInterleaved, juce::AudioData::NonConst>;
    constexpr std::array<float, 6> values {0.5f, -0.5f, 0.25f, -0.25f, 1.0f, -1.0f};
    for (const float value : values) {
        std::uint8_t bytes[4] {};
        Dest {bytes}.setAsFloat(value);
        const auto meant = static_cast<std::int64_t>(std::trunc(static_cast<double>(value) * 8388607.0));
        CHECK(little_endian(bytes, 4) == meant);
    }
}

namespace {

// ---------------------------------------------------------------------------
// The way through the device manager and the player

constexpr int k_block = 64;

// What the processor below writes: numbers a float cannot hold.
double exact_sample(int channel, int index)
{
    return (channel == 0 ? 1.0 : -1.0) * (1.0 / 3.0) * static_cast<double>(index + 1) / k_block;
}

// A processor that writes exact_sample in the precision it is processing in.
class ExactProcessor final : public juce::AudioProcessor {
public:
    ExactProcessor()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    const juce::String getName() const override { return "Exact"; }
    void prepareToPlay(double /*sample_rate*/, int /*block_size*/) override {}
    void releaseResources() override {}
    bool supportsDoublePrecisionProcessing() const override { return true; }

    void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer & /*midi*/) override
    {
        ++single_blocks;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample(c, i, static_cast<float>(exact_sample(c, i)));
    }

    void processBlock(juce::AudioBuffer<double> &buffer, juce::MidiBuffer & /*midi*/) override
    {
        ++double_blocks;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample(c, i, exact_sample(c, i));
    }

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor *createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int /*index*/) override {}
    const juce::String getProgramName(int /*index*/) override { return {}; }
    void changeProgramName(int /*index*/, const juce::String & /*name*/) override {}
    void getStateInformation(juce::MemoryBlock & /*data*/) override {}
    void setStateInformation(const void * /*data*/, int /*size*/) override {}

    int single_blocks = 0;
    int double_blocks = 0;
};

// A stereo output device that runs one block when asked, in the precision its
// callback asks for, as a device's own thread would.
class FakeDevice final : public juce::AudioIODevice {
public:
    FakeDevice() : AudioIODevice("Fake device", "Fake") {}
    ~FakeDevice() override { close(); }

    juce::StringArray getOutputChannelNames() override { return {"Left", "Right"}; }
    juce::StringArray getInputChannelNames() override { return {}; }
    juce::Array<double> getAvailableSampleRates() override { return {48000.0}; }
    juce::Array<int> getAvailableBufferSizes() override { return {k_block}; }
    int getDefaultBufferSize() override { return k_block; }

    juce::String open(const juce::BigInteger &inputs, const juce::BigInteger &outputs, double /*sample_rate*/,
                      int /*buffer_size*/) override
    {
        active_inputs = inputs;
        active_outputs = outputs;
        opened = true;
        return {};
    }

    void close() override
    {
        stop();
        opened = false;
    }

    bool isOpen() override { return opened; }

    void start(juce::AudioIODeviceCallback *new_callback) override
    {
        if (new_callback != nullptr)
            new_callback->audioDeviceAboutToStart(this);
        const juce::ScopedLock lock(callback_lock);
        callback = new_callback;
    }

    void stop() override
    {
        juce::AudioIODeviceCallback *old = nullptr;
        {
            const juce::ScopedLock lock(callback_lock);
            old = std::exchange(callback, nullptr);
        }
        if (old != nullptr)
            old->audioDeviceStopped();
    }

    bool isPlaying() override { return callback != nullptr; }
    juce::String getLastError() override { return {}; }
    int getCurrentBufferSizeSamples() override { return k_block; }
    double getCurrentSampleRate() override { return 48000.0; }
    int getCurrentBitDepth() override { return 64; }
    juce::BigInteger getActiveOutputChannels() const override { return active_outputs; }
    juce::BigInteger getActiveInputChannels() const override { return active_inputs; }
    int getOutputLatencyInSamples() override { return 0; }
    int getInputLatencyInSamples() override { return 0; }

    // One block; true when it went in double precision. What the callback
    // wrote is kept in `written`, as doubles either way.
    bool run_block()
    {
        const juce::ScopedLock lock(callback_lock);
        if (callback == nullptr)
            return false;
        const bool as_doubles = callback->wantsDoublePrecision();
        if (as_doubles) {
            juce::AudioBuffer<double> outs(2, k_block);
            outs.clear();
            callback->audioDeviceIOCallbackWithContextDouble(nullptr, 0, outs.getArrayOfWritePointers(), 2,
                                                             k_block, {});
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < k_block; ++i)
                    written[static_cast<std::size_t>(c)][static_cast<std::size_t>(i)] = outs.getSample(c, i);
        }
        else {
            juce::AudioBuffer<float> outs(2, k_block);
            outs.clear();
            callback->audioDeviceIOCallbackWithContext(nullptr, 0, outs.getArrayOfWritePointers(), 2, k_block, {});
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < k_block; ++i)
                    written[static_cast<std::size_t>(c)][static_cast<std::size_t>(i)] =
                        static_cast<double>(outs.getSample(c, i));
        }
        return as_doubles;
    }

    std::array<std::array<double, k_block>, 2> written {};

private:
    juce::CriticalSection callback_lock;
    juce::AudioIODeviceCallback *callback = nullptr;
    juce::BigInteger active_inputs, active_outputs;
    bool opened = false;
};

class FakeType final : public juce::AudioIODeviceType {
public:
    FakeType() : AudioIODeviceType("Fake") {}

    void scanForDevices() override {}
    [[nodiscard]] juce::StringArray getDeviceNames(bool inputs) const override
    {
        return inputs ? juce::StringArray() : juce::StringArray("Fake device");
    }
    [[nodiscard]] int getDefaultDeviceIndex(bool /*for_input*/) const override { return 0; }
    int getIndexOfDevice(juce::AudioIODevice *device, bool /*as_input*/) const override
    {
        return device != nullptr ? 0 : -1;
    }
    [[nodiscard]] bool hasSeparateInputsAndOutputs() const override { return true; }
    juce::AudioIODevice *createDevice(const juce::String & /*output_name*/,
                                      const juce::String & /*input_name*/) override
    {
        return new FakeDevice();
    }
};

// A callback that knows only single precision.
class SingleCallback final : public juce::AudioIODeviceCallback {
public:
    void audioDeviceIOCallbackWithContext(const float *const * /*ins*/, int /*num_ins*/, float *const *outs,
                                          int num_outs, int num_samples,
                                          const juce::AudioIODeviceCallbackContext & /*context*/) override
    {
        for (int c = 0; c < num_outs; ++c)
            if (outs[c] != nullptr)
                juce::FloatVectorOperations::clear(outs[c], num_samples);
    }
    void audioDeviceAboutToStart(juce::AudioIODevice * /*device*/) override {}
    void audioDeviceStopped() override {}
};

bool wrote_exactly(const FakeDevice &device)
{
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < k_block; ++i)
            if (device.written[static_cast<std::size_t>(c)][static_cast<std::size_t>(i)] != exact_sample(c, i))
                return false;
    return true;
}

bool wrote_in_single_precision(const FakeDevice &device)
{
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < k_block; ++i)
            if (device.written[static_cast<std::size_t>(c)][static_cast<std::size_t>(i)] !=
                static_cast<double>(static_cast<float>(exact_sample(c, i))))
                return false;
    return true;
}

}  // namespace

ADLPLUG_TEST(device_doubles)
{
    const juce::ScopedJuceInitialiser_GUI juce_gui;

    juce::AudioDeviceManager manager;
    manager.addAudioDeviceType(std::make_unique<FakeType>());
    manager.setCurrentAudioDeviceType("Fake", true);
    CHECK(manager.initialise(0, 2, nullptr, false).isEmpty());
    auto *device = dynamic_cast<FakeDevice *>(manager.getCurrentAudioDevice());
    CHECK(device != nullptr);
    if (device == nullptr)
        return;

    ExactProcessor processor;
    juce::AudioProcessorPlayer player;
    player.setDoublePrecisionProcessing(true);
    player.setProcessor(&processor);
    manager.addAudioCallback(&player);

    // A processor in double precision is asked for doubles, and they come out
    // of the device as the processor wrote them.
    CHECK(player.wantsDoublePrecision());
    CHECK(device->run_block());
    CHECK(processor.double_blocks == 1 && processor.single_blocks == 0);
    CHECK(wrote_exactly(*device));

    // A callback beside it that knows only floats: the manager asks for floats,
    // and the player converts for its processor, which still processes doubles.
    SingleCallback single;
    manager.addAudioCallback(&single);
    CHECK(!device->run_block());
    CHECK(processor.double_blocks == 2 && processor.single_blocks == 0);
    CHECK(wrote_in_single_precision(*device));
    manager.removeAudioCallback(&single);
    CHECK(device->run_block());
    CHECK(wrote_exactly(*device));

    // Single precision asked for: floats all the way, as before.
    player.setDoublePrecisionProcessing(false);
    CHECK(!player.wantsDoublePrecision());
    CHECK(!device->run_block());
    CHECK(processor.single_blocks == 1);
    CHECK(wrote_in_single_precision(*device));

    manager.removeAudioCallback(&player);
    player.setProcessor(nullptr);
    manager.closeAudioDevice();
}
