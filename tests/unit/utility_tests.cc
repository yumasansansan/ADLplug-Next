// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#include "test.h"
#include "utility/atomic_bit_set.h"
#include "resampling_settings.h"
#include "utility/chip_resampler.h"
#include "utility/counting_bitset.h"
#include "utility/field_bitops.h"
#include "utility/fourcc.h"
#include "utility/semaphore.h"
#include "ui/utility/image.h"
#include "ui/utility/knobman_skin.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

// Writes every value into a field of a byte, over a few backgrounds, and reads
// it back plainly and inverted.
template <unsigned shift, unsigned size>
void check_byte_field()
{
    const unsigned max = (1u << size) - 1;
    const unsigned others = ~(max << shift) & 0xffu;
    for (const unsigned background : {0x00u, 0xffu, 0xa5u}) {
        for (unsigned value = 0; value <= max; ++value) {
            auto byte = static_cast<std::uint8_t>(background);
            Field_Bitops::set<shift, size>(byte, value);
            CHECK(Field_Bitops::get<shift, size, unsigned>(byte) == value);
            CHECK((static_cast<unsigned>(byte) & others) == (background & others));
            Field_Bitops::set_inverted<shift, size>(byte, value);
            CHECK(Field_Bitops::get_inverted<shift, size, unsigned>(byte) == value);
            CHECK(Field_Bitops::get<shift, size, unsigned>(byte) == max - value);
        }
    }
}

}  // namespace

ADLPLUG_TEST(counting_bitset)
{
    counting_bitset<128> bits;
    CHECK(bits.none() && bits.count() == 0);
    bits.set(0).set(64).set(127);
    CHECK(bits.count() == 3 && bits.test(64) && !bits.test(63));
    bits.set(64);
    CHECK(bits.count() == 3);
    bits.reset(0);
    CHECK(bits.count() == 2 && !bits.test(0));
    bits.flip();
    CHECK(bits.count() == 126 && bits.test(0) && !bits.test(127));
    bits.flip(127);
    CHECK(bits.count() == 127);
    bits.set();
    CHECK(bits.all() && bits.count() == 128);
    bits.reset();
    CHECK(bits.none());
}

ADLPLUG_TEST(atomic_bit_set)
{
    Atomic_Bit_Set<100> bits;
    CHECK(bits.size() == 100);
    // The blocks are 32 bits wide: bits on both sides of each boundary.
    for (const std::size_t i : {0uz, 31uz, 32uz, 63uz, 64uz, 99uz}) {
        CHECK(!bits.test(i));
        CHECK(!bits.set(i));
        CHECK(bits.set(i));
        CHECK(bits.test(i));
    }
    CHECK(!bits.test(1) && !bits.test(33) && !bits.test(65));
    CHECK(bits.reset(31));
    CHECK(!bits.reset(31));
    CHECK(!bits.set(31, false));
    CHECK(!bits.set(30, true));
    CHECK(bits[30]);
    bits.reset_all();
    bool none = true;
    for (std::size_t i = 0; i < bits.size(); ++i)
        none = none && !bits.test(i);
    CHECK(none);
}

ADLPLUG_TEST(atomic_bit_set_threads)
{
    // Four threads set the bits of their own residue at once; every one sticks.
    Atomic_Bit_Set<1024> bits;
    std::vector<std::thread> threads;
    threads.reserve(4);
    for (std::size_t t = 0; t < 4; ++t) {
        threads.emplace_back([&bits, t] {
            for (std::size_t i = t; i < bits.size(); i += 4)
                bits.set(i);
        });
    }
    for (std::thread &thread : threads)
        thread.join();
    bool all = true;
    for (std::size_t i = 0; i < bits.size(); ++i)
        all = all && bits.test(i);
    CHECK(all);
}

ADLPLUG_TEST(field_bitops)
{
    check_byte_field<0, 1>();
    check_byte_field<7, 1>();
    check_byte_field<0, 4>();
    check_byte_field<4, 4>();
    check_byte_field<2, 3>();
    check_byte_field<5, 3>();
    check_byte_field<0, 8>();
}

ADLPLUG_TEST(fourcc)
{
    CHECK(fourcc("chip") == 0x63686970u);
    CHECK(fourcc("\xff\x01\x00\x80") == 0xff010080u);
}

// The rates of the two chips and of a host, which is where the ratios come from:
// 49716 against 44100 reduces (36), and 53267 is prime, so it does not.
ADLPLUG_TEST(chip_resampler)
{
    Chip_Resampler resampler;
    const mp::resample::Design design;  // the default is the middle of the three settings
    std::string why;

    // The OPL3's rate against a common one: a filter, and one that meets the
    // specification it was asked for rather than one that says it did.
    CHECK(resampler.prepare(49716, 44100, 256, design, why));
    CHECK(resampler.active());
    CHECK(why.empty());
    CHECK(resampler.chip_rate() == 49716);
    CHECK(resampler.host_rate() == 44100);
    CHECK(resampler.response().stopband_db <= -design.attenuation_db);
    // A linear-phase filter leaves the host nothing to make up for.
    CHECK(resampler.latency_frames() == 0.0);

    // Asked for frames, it writes that many and no fewer, block after block, and
    // asks the chip for about as many as the ratio says: the rest of a block the
    // filter produced waits for the next one rather than being thrown away.
    unsigned asked_of_chip = 0;
    const auto steady = [&asked_of_chip](float *left, float *right, unsigned frames) {
        asked_of_chip += frames;
        for (unsigned i = 0; i < frames; ++i) {
            left[i] = 0.5f;
            right[i] = -0.25f;
        }
    };

    std::vector<float> left(1024, 0.0f);
    std::vector<float> right(1024, 0.0f);
    unsigned written = 0;
    for (const unsigned block : {256u, 64u, 1u, 200u, 256u, 13u}) {
        for (unsigned i = 0; i < block; ++i) {
            left[written + i] = 123.0f;  // so that a frame not written is seen
            right[written + i] = 123.0f;
        }
        resampler.pull(left.data() + written, right.data() + written, block, steady);
        written += block;
    }
    CHECK(written == 790);
    for (unsigned i = 0; i < written; ++i) {
        CHECK(left[i] != 123.0f);
        CHECK(right[i] != 123.0f);
    }
    // 790 frames of the host's are 890 of the chip's, give or take the frames the
    // filter holds either side.
    const unsigned expected = 790u * 49716u / 44100u;
    CHECK(asked_of_chip >= expected);
    CHECK(asked_of_chip <= expected + 512);

    // What went in was a constant, and a resampler passes one through: the
    // filter's gain at direct current is one. The start of the stream is the
    // filter filling up, so the end of it is where to look.
    for (unsigned i = written - 200; i < written; ++i) {
        CHECK(std::abs(static_cast<double>(left[i]) - 0.5) < 1e-6);
        CHECK(std::abs(static_cast<double>(right[i]) + 0.25) < 1e-6);
    }

    // Equal rates are not resampled: there is nothing to say about it either.
    why = "something";
    CHECK(!resampler.prepare(44100, 44100, 256, design, why));
    CHECK(!resampler.active());
    CHECK(why == "something");

    // The OPN2's rate against the same host rate does not reduce -- 53267 is
    // prime -- so at these settings the filter is refused, in words, and the
    // caller that hears no plays what it is given instead.
    CHECK(!resampler.prepare(53267, 44100, 256, design, why));
    CHECK(!resampler.active());
    CHECK(!why.empty());

    unsigned straight_through = 0;
    const auto count = [&straight_through](float *l, float *r, unsigned frames) {
        straight_through += frames;
        for (unsigned i = 0; i < frames; ++i) {
            l[i] = 1.0f;
            r[i] = 1.0f;
        }
    };
    resampler.pull(left.data(), right.data(), 128, count);
    CHECK(straight_through == 128);
    CHECK(left[0] == 1.0f);
    CHECK(left[127] == 1.0f);
}

ADLPLUG_TEST(resampling_settings)
{
    // Every named setting is still one the resampler takes by name, and a design
    // made from one is recognised as it.
    for (const char *name : resampling_preset_names) {
        mp::resample::Design design;
        CHECK(resampling_preset(name, design));
        CHECK(resampling_preset_of(design) == name);
    }
    // A name that is none of them leaves the design as it was.
    mp::resample::Design untouched;
    CHECK(!resampling_preset("no such setting", untouched));
    CHECK(untouched == mp::resample::Design{});

    // A design changed by hand is none of them, even one field away from one.
    mp::resample::Design custom;
    custom.attenuation_db = 130.0;
    CHECK(resampling_preset_of(custom).empty());

    // A project keeps the settings as they were, bit for bit: numbers no preset
    // has, a bandwidth that has no exact binary form, and every field that no
    // preset changes.
    Resampling_Settings rs;
    rs.own_filter = false;
    rs.design.method = mp::resample::Method::refine;
    rs.design.window = mp::resample::Window::dpss;
    rs.design.phase = mp::resample::Phase::minimum;
    rs.design.attenuation_db = 131.25;
    rs.design.passband_ripple_db = 0.001;
    rs.design.bandwidth = 0.9731;
    rs.design.taps = 96;
    rs.design.max_taps = 1u << 24;
    rs.design.cepstrum = 64;
    rs.design.phase_floor_db = -200.5;
    rs.design.remez_max_taps = 2049;
    rs.design.refine_rounds = 80;
    rs.design.refine_patience = 9;
    rs.design.measure_points = 1u << 20;
    rs.design.stages = 3;
    rs.design.verify = true;
    CHECK(Resampling_Settings::from_properties(rs.to_properties()) == rs);

    // The largest counts there are come back as themselves too.
    Resampling_Settings largest;
    largest.design.max_taps = 0xffffffffu;
    largest.design.measure_points = 0xffffffffu;
    CHECK(Resampling_Settings::from_properties(largest.to_properties()) == largest);

    // A project from before there was a choice has none of it, and gets the
    // defaults. A field that is no value of its kind keeps its default as well.
    CHECK(Resampling_Settings::from_properties(PropertySet()) == Resampling_Settings{});
    PropertySet odd = Resampling_Settings{}.to_properties();
    odd.setValue("method", "shiny");
    odd.setValue("bandwidth", "0.9x");
    odd.setValue("taps", "-3");
    odd.setValue("max_taps", "4294967296");
    CHECK(Resampling_Settings::from_properties(odd) == Resampling_Settings{});
}

ADLPLUG_TEST(semaphore)
{
    Semaphore empty;
    CHECK(!empty.try_wait());

    Semaphore two(2);
    CHECK(two.try_wait());
    CHECK(two.try_wait());
    CHECK(!two.try_wait());
    two.post();
    CHECK(two.try_wait());

    // wait() returns once another thread has posted.
    Semaphore woken;
    std::atomic<bool> posted {false};
    std::thread poster([&woken, &posted] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        posted.store(true);
        woken.post();
    });
    woken.wait();
    CHECK(posted.load());
    poster.join();
}

ADLPLUG_TEST(knob_skin_scaled)
{
    // A strip of frames the way a knob's skin is one: a picture in each frame,
    // on a transparent border that load() crops away. The small skins are drawn
    // into one image rather than rescaled a frame at a time, and every frame has
    // to come out as Image::rescaled makes it.
    //
    // **In software images**, which draw a bitmap the same wherever it goes in
    // the target. Direct2D's high-quality scaling does not: the 64 frames of the
    // knob's small skin, each drawn alone 3, 8 and 64 pixels from a target's
    // corner, came out 147, 290 and 1558 pixels different of 55680 from the same
    // frames drawn at the corner. So on Windows the small skins are not
    // rescaled()'s to the pixel, and could not be with more than one frame in a
    // target; what this checks there is where the frames go and how they are
    // scaled, which is the same code whatever the image type.
    constexpr int frame_count = 64;
    constexpr int width = 50;
    constexpr int height = 40;
    const Image strip(Image::ARGB, width, height * frame_count, true, SoftwareImageType{});
    {
        Graphics g(strip);
        for (int i = 0; i < frame_count; ++i) {
            // juce::, since <windows.h> has a Rectangle of its own.
            const juce::Rectangle<float> face(4.0f, static_cast<float>(i * height) + 3.0f,
                                              static_cast<float>(width) - 9.0f,
                                              static_cast<float>(height) - 7.0f);
            g.setColour(Colour::fromHSV(static_cast<float>(i) / frame_count, 0.7f, 0.9f, 1.0f));
            g.fillEllipse(face);
            const float angle = static_cast<float>(i) * 0.1f;
            const juce::Point<float> centre = face.getCentre();
            g.setColour(Colours::white.withAlpha(0.8f));
            g.drawLine(centre.x, centre.y, centre.x + 15.0f * std::cos(angle),
                       centre.y + 15.0f * std::sin(angle), 2.0f);
        }
    }
    Km_Skin skin;
    skin.load(strip, frame_count);
    CHECK(skin.frames.size() == static_cast<std::size_t>(frame_count));

    for (const double ratio : {0.7, 0.5}) {
        const Km_Skin_Ptr small = skin.scaled(ratio);
        CHECK(small->frames.size() == skin.frames.size());
        for (std::size_t i = 0; i < skin.frames.size() && i < small->frames.size(); ++i) {
            const Image &got = small->frames[i];
            const int want_width = static_cast<int>(std::lround(skin.frames[i].getWidth() * ratio));
            const int want_height = static_cast<int>(std::lround(skin.frames[i].getHeight() * ratio));
            CHECK(got.getWidth() == want_width && got.getHeight() == want_height);
            const Image want = skin.frames[i].rescaled(want_width, want_height,
                                                       Graphics::highResamplingQuality);
            const Image::BitmapData got_pixels(got, Image::BitmapData::readOnly);
            const Image::BitmapData want_pixels(want, Image::BitmapData::readOnly);
            int differing = 0;
            for (int y = 0; y < want_height; ++y)
                for (int x = 0; x < want_width; ++x)
                    if (got_pixels.getPixelColour(x, y) != want_pixels.getPixelColour(x, y))
                        ++differing;
            CHECK(differing == 0);
        }
    }
}

ADLPLUG_TEST(text_icons)
{
    // The emulators' labels are drawn into one image, and each has to come out
    // as make_text_icon draws it alone: the same size, and pixel for pixel in
    // software images (see knob_skin_scaled for why software).
    StringArray texts;
    texts.add("DOSBox");
    texts.add("Opal");
    texts.add("Java");
    texts.add("MAME");
    texts.add("ymfm");
    texts.add("W");
    const std::vector<Image> together = Image_Utils::make_text_icons(texts, SoftwareImageType{});
    CHECK(together.size() == static_cast<std::size_t>(texts.size()));
    for (int i = 0; i < texts.size() && static_cast<std::size_t>(i) < together.size(); ++i) {
        const Image &got = together[static_cast<std::size_t>(i)];
        const Image want = Image_Utils::make_text_icon(texts[i], SoftwareImageType{});
        CHECK(got.getWidth() == want.getWidth() && got.getHeight() == want.getHeight());
        const Image::BitmapData got_pixels(got, Image::BitmapData::readOnly);
        const Image::BitmapData want_pixels(want, Image::BitmapData::readOnly);
        int differing = 0;
        for (int y = 0; y < want.getHeight(); ++y)
            for (int x = 0; x < want.getWidth(); ++x)
                if (got_pixels.getPixelColour(x, y) != want_pixels.getPixelColour(x, y))
                    ++differing;
        CHECK(differing == 0);
    }
    CHECK(Image_Utils::make_text_icons(StringArray()).empty());
}
