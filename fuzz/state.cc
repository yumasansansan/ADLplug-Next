// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// F1: the state that a host kept in a project (plan D48). A project is the one
// input that the plugin is handed whole: banks, instruments, the selection of
// each part, the chip settings and the master volume, all of them written by a
// version of the plugin that may not be this one, and edited or damaged by
// anything that has touched the project since.
//
// The input is the text of the state. A host keeps it as
// AudioProcessor::copyXmlToBinary() leaves it, which is a header and a
// compressed block, and a change inside a compressed block only breaks its
// checksum; so the input here is the text, and the wrapper is put on it. Bytes
// that are not XML at all go in as they are, which is what the wrapper itself
// has to survive. The text is read the way JUCE reads a file, so bytes that are
// not UTF-8 are read as Windows-1252 rather than refused.
//
// What is checked: the sanitizers, and that a project does not change by being
// opened and saved. The state that the plugin writes after reading the input is
// its own, whatever the input was; reading that and writing it again has to
// give the same bytes. A project that changed every time it was opened would
// leave a host's undo history and its "unsaved changes" mark lying.
//
// Reading a state makes a player, which loads the plugin's own bank of
// instruments, so one input costs a few milliseconds; the audio is left to F3
// and F5.

#include "fuzz.h"
#include "plugin_processor.h"
#include "JuceHeader.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

namespace {

// A project can be any size, and there is nothing to learn from a state far
// larger than one the plugin writes.
constexpr std::size_t size_max = 256 * 1024;

// JUCE's XML writer walks off a null pointer when it writes, in the one-line
// form that copyXmlToBinary() uses, an element that comes after text and has
// children of its own: that element is written as though it were indented, and
// the characters that would end its lines -- which the one-line form does not
// have -- are handed to strlen() all the same. The state the plugin writes
// holds no text (a PropertySet writes attributes), so the text goes out of the
// input before it is wrapped, and nothing the plugin reads is lost with it.
void remove_text_elements(juce::XmlElement &element)
{
    element.deleteAllTextElements();
    for (juce::XmlElement *child : element.getChildIterator())
        remove_text_elements(*child);
}

// A state of no bytes is the ordinary answer here: it is what the plugin writes
// for a project whose plugin was never touched. JUCE's own comparison of two
// blocks reaches memcmp() even then, and a block of no bytes has no address to
// give it; the sanitizer refuses a null argument although nothing is read.
bool same_bytes(const juce::MemoryBlock &a, const juce::MemoryBlock &b)
{
    return a.getSize() == b.getSize() &&
        (a.isEmpty() || std::memcmp(a.getData(), b.getData(), a.getSize()) == 0);
}

}  // namespace

// The processor's interface is not built here, so this target says what the
// processor has instead of an editor: nothing. The state knows nothing of the
// editor either (plugin_editor.cc holds the other two lines of this pair).
bool AdlplugAudioProcessor::hasEditor() const
{
    return false;
}

juce::AudioProcessorEditor *AdlplugAudioProcessor::createEditor()
{
    return nullptr;
}

int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    // The parameters of a processor want JUCE alive. libFuzzer calls this in one
    // thread, so one initialiser for the whole run will do.
    static const juce::ScopedJuceInitialiser_GUI juce_alive;

    size = std::min(size, size_max);

    AdlplugAudioProcessor processor;

    const juce::String text = juce::String::createStringFromData(data, static_cast<int>(size));
    if (const std::unique_ptr<juce::XmlElement> xml = juce::parseXML(text)) {
        remove_text_elements(*xml);
        juce::MemoryBlock wrapped;
        juce::AudioProcessor::copyXmlToBinary(*xml, wrapped);
        processor.setStateInformation(wrapped.getData(), static_cast<int>(wrapped.getSize()));
    }
    else {
        processor.setStateInformation(data, static_cast<int>(size));
    }

    juce::MemoryBlock written;
    processor.getStateInformation(written);
    processor.setStateInformation(written.getData(), static_cast<int>(written.getSize()));

    juce::MemoryBlock again;
    processor.getStateInformation(again);
    FUZZ_CHECK(same_bytes(written, again));

    return 0;
}
