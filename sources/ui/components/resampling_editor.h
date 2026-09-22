// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#pragma once
#include "JuceHeader.h"
#include "resampling_settings.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// The settings of the resampling (sources/resampling_settings.h), every one of
// them, and what became of the ones last applied.
//
// A named setting fills in the fields it stands for; a field changed by hand
// makes the settings one of no name. Nothing is sent until Apply, because every
// change designs a filter again, which takes a noticeable part of a second, and
// a person half way through typing a number has not asked for one.
class Resampling_Editor final : public Component {
public:
    Resampling_Editor();
    ~Resampling_Editor() override;

    // The settings the processor has and what became of them. Fields changed here
    // and not yet applied stay as they are; what became of the last ones applied
    // is shown either way.
    void set_state(const Resampling_Settings &settings, const Resampling_Status &status);

    std::function<void(const Resampling_Settings &settings)> on_apply;
    std::function<void()> on_close;

    void paint(Graphics &g) override;
    void resized() override;

private:
    struct Field {
        std::unique_ptr<Label> label;
        std::unique_ptr<TextEditor> editor;
    };

    Field &add_field(const String &name, const String &tooltip);
    void fill(const Resampling_Settings &settings);
    void show_status(const Resampling_Status &status, bool own_filter);
    // What the fields say, or why they do not say anything a design can take.
    [[nodiscard]] bool read(Resampling_Settings &settings, String &why) const;
    void preset_chosen();
    void edited();
    void apply();

    std::unique_ptr<Label> lbl_where_;
    std::unique_ptr<ComboBox> cb_where_;
    std::unique_ptr<Label> lbl_preset_;
    std::unique_ptr<ComboBox> cb_preset_;
    std::unique_ptr<Label> lbl_method_;
    std::unique_ptr<ComboBox> cb_method_;
    std::unique_ptr<Label> lbl_window_;
    std::unique_ptr<ComboBox> cb_window_;
    std::unique_ptr<Label> lbl_phase_;
    std::unique_ptr<ComboBox> cb_phase_;
    std::vector<Field> fields_;
    std::unique_ptr<ToggleButton> tb_verify_;
    std::unique_ptr<Label> lbl_status_;
    std::unique_ptr<TextButton> btn_apply_;
    std::unique_ptr<TextButton> btn_close_;

    // Changed here and not applied: the processor's word does not overwrite it.
    bool edited_ = false;
    // Filling the fields is not a person changing them.
    bool filling_ = false;
    Resampling_Status last_status_;
    bool last_own_filter_ = true;
};
