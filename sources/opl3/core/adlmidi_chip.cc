// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).
//
// ADLplug-Next's OPL3 core as a chip of libADLMIDI, which makes it as emulator
// 14 (patches/libADLMIDI/0010-emulator-14-is-adlplug-next-s-own-opl3-core.patch).

#include "adlmidi_chip.h"
#include "core.h"

#include "opl_chip_base.h"

#include <cstdint>

namespace adlplug::opl3 {
namespace {

class Adlmidi_chip final : public OPLChipBaseT<Adlmidi_chip> {
public:
    [[nodiscard]] bool canRunAtPcmRate() const override { return false; }
    void setRate(std::uint32_t rate) override
    {
        OPLChipBaseT<Adlmidi_chip>::setRate(rate);
        core_.reset();
    }
    void reset() override
    {
        OPLChipBaseT<Adlmidi_chip>::reset();
        core_.reset();
    }
    void writeReg(std::uint16_t address, std::uint8_t value) override { core_.write(address, value); }
    void writePan(std::uint16_t address, std::uint8_t value) override { core_.write_pan(address, value); }
    void nativePreGenerate() override {}
    void nativePostGenerate() override {}
    void nativeGenerate(std::int16_t *frame) override
    {
        std::int32_t sample[2];
        core_.generate(sample);
        // Within sixteen bits already.
        frame[0] = static_cast<std::int16_t>(sample[0]);
        frame[1] = static_cast<std::int16_t>(sample[1]);
    }
    const char *emulatorName() override { return "ADLplug-Next OPL3"; }
    ChipType chipType() override { return CHIPTYPE_OPL3; }
    bool hasFullPanning() override { return true; }

private:
    Core core_;
};

}  // namespace

OPLChipBase *create_adlmidi_chip()
{
    return new Adlmidi_chip;
}

}  // namespace adlplug::opl3
