// SPDX-FileCopyrightText: 2026 Yuma Kakei <yumasansansan@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file is part of ADLplug-Next. The SPDX lines name its copyright holder
// and its license, in the machine-readable form of the REUSE specification: the
// GNU General Public License, version 3 or any later version
// (LICENSES/GPL-3.0-or-later.txt).

#include "resampling_editor.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <system_error>

namespace {

// The fields of numbers, in the order they are laid out.
enum Field_Index : std::size_t {
    f_attenuation,
    f_ripple,
    f_bandwidth,
    f_taps,
    f_max_taps,
    f_stages,
    f_cepstrum,
    f_phase_floor,
    f_remez_max_taps,
    f_refine_rounds,
    f_refine_patience,
    f_measure_points,
    field_count,
};

constexpr int where_own = 1;
constexpr int where_library = 2;
// The named settings take the ids from 1; this one is for settings of no name.
constexpr int preset_by_hand = 100;

// A number as the state writes it: the shortest text that reads back as the same
// number, whatever the locale.
String number_text(double value)
{
    std::array<char, 32> text {};
    const auto [end, error] = std::to_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc() ? String(text.data(), static_cast<std::size_t>(end - text.data())) : String();
}

bool parse(const String &text, double &value)
{
    const std::string s = text.trim().toStdString();
    const auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), value);
    return !s.empty() && error == std::errc() && end == s.data() + s.size();
}

bool parse(const String &text, std::uint32_t &value)
{
    const std::string s = text.trim().toStdString();
    const auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), value);
    return !s.empty() && error == std::errc() && end == s.data() + s.size();
}

// A count of coefficients as memory, eight bytes apiece.
String megabytes(std::uint64_t coefficients)
{
    return String(static_cast<double>(coefficients) * 8.0 / (1024.0 * 1024.0), 1) + " MB";
}

}  // namespace

Resampling_Editor::Resampling_Editor()
{
    const auto label = [this](const String &text) {
        auto l = std::make_unique<Label>(String(), text);
        l->setJustificationType(Justification::centredRight);
        addAndMakeVisible(l.get());
        return l;
    };
    const auto combo = [this]() {
        auto c = std::make_unique<ComboBox>();
        addAndMakeVisible(c.get());
        return c;
    };

    lbl_where_ = label("Where");
    cb_where_ = combo();
    cb_where_->addItem("The plugin's own filter", where_own);
    cb_where_->addItem("The library's straight line, as before", where_library);
    cb_where_->setTooltip("Where the chip's samples become the host's: through a filter designed for the "
                          "pair of rates, or by the interpolation inside the library");
    cb_where_->onChange = [this] { edited(); };

    lbl_preset_ = label("Setting");
    cb_preset_ = combo();
    for (std::size_t i = 0; i < resampling_preset_names.size(); ++i)
        cb_preset_->addItem(resampling_preset_names[i], static_cast<int>(i) + 1);
    cb_preset_->addItem("By hand", preset_by_hand);
    cb_preset_->setTooltip("A named setting fills in the fields below; changing a field makes the "
                           "settings your own");
    cb_preset_->onChange = [this] { preset_chosen(); };

    lbl_method_ = label("Method");
    cb_method_ = combo();
    for (const auto method : {mp::resample::Method::window, mp::resample::Method::remez, mp::resample::Method::refine})
        cb_method_->addItem(mp::resample::method_name(method), static_cast<int>(method) + 1);
    cb_method_->setTooltip("window: a windowed sinc. remez: Parks-McClellan, optimal, refused where it "
                           "cannot be trusted. refine: alternating projection from the window design");
    cb_method_->onChange = [this] { edited(); };

    lbl_window_ = label("Window");
    cb_window_ = combo();
    for (const auto window : {mp::resample::Window::kaiser, mp::resample::Window::dolph, mp::resample::Window::dpss})
        cb_window_->addItem(mp::resample::window_name(window), static_cast<int>(window) + 1);
    cb_window_->setTooltip("kaiser: the default. dolph: every sidelobe at the attenuation asked for. "
                           "dpss: the Slepian window, the optimum Kaiser approximates");
    cb_window_->onChange = [this] { edited(); };

    lbl_phase_ = label("Phase");
    cb_phase_ = combo();
    for (const auto phase : {mp::resample::Phase::linear, mp::resample::Phase::minimum})
        cb_phase_->addItem(mp::resample::phase_name(phase), static_cast<int>(phase) + 1);
    cb_phase_->setTooltip("linear: every frequency delayed alike, and exactly compensated. minimum: no "
                          "pre-ringing, and a delay the host is told of");
    cb_phase_->onChange = [this] { edited(); };

    fields_.reserve(field_count);
    add_field("Attenuation (dB)", "How far down the stopband has to be");
    add_field("Passband ripple (dB)", "How far the passband may stray from flat; 0 lets the design decide");
    add_field("Bandwidth", "The part of the output band that is kept, as a fraction of it");
    add_field("Taps per phase", "0 lets the design choose as many as the attenuation needs");
    add_field("Max taps", "The most coefficients a design may have; a design that needs more is refused");
    add_field("Stages", "How many steps the ratio may be taken in; 0 lets the design choose");
    add_field("Cepstrum", "Minimum phase only: the room the cepstrum is given, as a multiple of the length");
    add_field("Phase floor (dB)", "Minimum phase only: the floor under the magnitude before its logarithm");
    add_field("Remez max taps", "Parks-McClellan only: the longest filter it is trusted with");
    add_field("Refine rounds", "Refine only: how many rounds of projection");
    add_field("Refine patience", "Refine only: how many fruitless rounds before it stops");
    add_field("Measure points", "How many points the response is measured at");

    tb_verify_ = std::make_unique<ToggleButton>("Verify");
    tb_verify_->setTooltip("Build the design, measure it, and buy any shortfall in taps");
    tb_verify_->onClick = [this] { edited(); };
    addAndMakeVisible(tb_verify_.get());

    lbl_status_ = std::make_unique<Label>();
    lbl_status_->setJustificationType(Justification::topLeft);
    lbl_status_->setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(lbl_status_.get());

    btn_apply_ = std::make_unique<TextButton>("Apply");
    btn_apply_->onClick = [this] { apply(); };
    addAndMakeVisible(btn_apply_.get());

    btn_close_ = std::make_unique<TextButton>("Close");
    btn_close_->onClick = [this] {
        if (on_close)
            on_close();
    };
    addAndMakeVisible(btn_close_.get());

    fill(Resampling_Settings{});
    setSize(600, 470);
}

Resampling_Editor::~Resampling_Editor() = default;

Resampling_Editor::Field &Resampling_Editor::add_field(const String &name, const String &tooltip)
{
    Field field;
    field.label = std::make_unique<Label>(String(), name);
    field.label->setJustificationType(Justification::centredRight);
    addAndMakeVisible(field.label.get());
    field.editor = std::make_unique<TextEditor>();
    field.editor->setTooltip(tooltip);
    field.editor->onTextChange = [this] { edited(); };
    addAndMakeVisible(field.editor.get());
    fields_.push_back(std::move(field));
    return fields_.back();
}

void Resampling_Editor::set_state(const Resampling_Settings &settings, const Resampling_Status &status)
{
    last_status_ = status;
    last_own_filter_ = settings.own_filter;
    if (!edited_)
        fill(settings);
    show_status(status, settings.own_filter);
}

void Resampling_Editor::fill(const Resampling_Settings &settings)
{
    filling_ = true;
    const mp::resample::Design &d = settings.design;
    cb_where_->setSelectedId(settings.own_filter ? where_own : where_library, dontSendNotification);
    cb_method_->setSelectedId(static_cast<int>(d.method) + 1, dontSendNotification);
    cb_window_->setSelectedId(static_cast<int>(d.window) + 1, dontSendNotification);
    cb_phase_->setSelectedId(static_cast<int>(d.phase) + 1, dontSendNotification);
    fields_[f_attenuation].editor->setText(number_text(d.attenuation_db), false);
    fields_[f_ripple].editor->setText(number_text(d.passband_ripple_db), false);
    fields_[f_bandwidth].editor->setText(number_text(d.bandwidth), false);
    fields_[f_taps].editor->setText(String(d.taps), false);
    fields_[f_max_taps].editor->setText(String(d.max_taps), false);
    fields_[f_stages].editor->setText(String(d.stages), false);
    fields_[f_cepstrum].editor->setText(String(d.cepstrum), false);
    fields_[f_phase_floor].editor->setText(number_text(d.phase_floor_db), false);
    fields_[f_remez_max_taps].editor->setText(String(d.remez_max_taps), false);
    fields_[f_refine_rounds].editor->setText(String(d.refine_rounds), false);
    fields_[f_refine_patience].editor->setText(String(d.refine_patience), false);
    fields_[f_measure_points].editor->setText(String(d.measure_points), false);
    tb_verify_->setToggleState(d.verify, dontSendNotification);

    const std::string preset = resampling_preset_of(d);
    int id = preset_by_hand;
    for (std::size_t i = 0; i < resampling_preset_names.size(); ++i)
        if (preset == resampling_preset_names[i])
            id = static_cast<int>(i) + 1;
    cb_preset_->setSelectedId(id, dontSendNotification);
    filling_ = false;
}

void Resampling_Editor::show_status(const Resampling_Status &status, bool own_filter)
{
    String text;
    const String rates = String(status.chip_rate) + " Hz to " + String(status.host_rate) + " Hz";
    if (status.active) {
        text << "Running, " << rates << ": " << String(status.multiplies, 0)
             << " multiplies a frame and channel, " << String(static_cast<int64>(status.coefficients))
             << " coefficients (" << megabytes(status.coefficients) << "), the stopband "
             << String(status.stopband_db, 1) << " dB down.";
    }
    else if (!own_filter) {
        text << "The library interpolates, " << rates << ", as chosen.";
    }
    else if (status.why[0] == '\0') {
        text << "The chip runs at the host's rate, " << String(status.host_rate)
             << " Hz: there is nothing to resample.";
    }
    else {
        text << "Not running: the library interpolates instead, " << rates << ". The design said: "
             << String::fromUTF8(status.why);
    }
    if (edited_)
        text << "\n\nChanged here and not applied yet.";
    lbl_status_->setText(text, dontSendNotification);
}

bool Resampling_Editor::read(Resampling_Settings &settings, String &why) const
{
    settings.own_filter = cb_where_->getSelectedId() != where_library;
    mp::resample::Design &d = settings.design;
    d.method = static_cast<mp::resample::Method>(std::max(0, cb_method_->getSelectedId() - 1));
    d.window = static_cast<mp::resample::Window>(std::max(0, cb_window_->getSelectedId() - 1));
    d.phase = static_cast<mp::resample::Phase>(std::max(0, cb_phase_->getSelectedId() - 1));
    d.verify = tb_verify_->getToggleState();

    const auto number = [this, &why](std::size_t index, auto &value) {
        if (parse(fields_[index].editor->getText(), value))
            return true;
        why = fields_[index].label->getText() + " is not a number of its kind.";
        return false;
    };
    return number(f_attenuation, d.attenuation_db) && number(f_ripple, d.passband_ripple_db) &&
           number(f_bandwidth, d.bandwidth) && number(f_taps, d.taps) && number(f_max_taps, d.max_taps) &&
           number(f_stages, d.stages) && number(f_cepstrum, d.cepstrum) &&
           number(f_phase_floor, d.phase_floor_db) && number(f_remez_max_taps, d.remez_max_taps) &&
           number(f_refine_rounds, d.refine_rounds) && number(f_refine_patience, d.refine_patience) &&
           number(f_measure_points, d.measure_points);
}

void Resampling_Editor::preset_chosen()
{
    if (filling_)
        return;
    const int id = cb_preset_->getSelectedId();
    if (id < 1 || id > static_cast<int>(resampling_preset_names.size()))
        return;  // "by hand" is where the fields already are

    Resampling_Settings settings;
    String why;
    static_cast<void>(read(settings, why));  // keep where the resampling is done
    if (!resampling_preset(resampling_preset_names[static_cast<std::size_t>(id - 1)], settings.design))
        return;
    fill(settings);
    edited();
}

void Resampling_Editor::edited()
{
    if (filling_)
        return;
    edited_ = true;

    // The name of the settings follows the fields.
    Resampling_Settings settings;
    String why;
    if (read(settings, why)) {
        const std::string preset = resampling_preset_of(settings.design);
        int id = preset_by_hand;
        for (std::size_t i = 0; i < resampling_preset_names.size(); ++i)
            if (preset == resampling_preset_names[i])
                id = static_cast<int>(i) + 1;
        filling_ = true;
        cb_preset_->setSelectedId(id, dontSendNotification);
        filling_ = false;
    }
    show_status(last_status_, last_own_filter_);
}

void Resampling_Editor::apply()
{
    Resampling_Settings settings;
    String why;
    if (!read(settings, why)) {
        lbl_status_->setText(why, dontSendNotification);
        return;
    }
    edited_ = false;
    if (on_apply)
        on_apply(settings);
}

void Resampling_Editor::paint(Graphics &g)
{
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
}

void Resampling_Editor::resized()
{
    constexpr int row = 24;
    constexpr int gap = 6;
    constexpr int label_width = 150;
    Rectangle<int> area = getLocalBounds().reduced(12);

    const auto place = [&](Label &label, Component &control, Rectangle<int> line) {
        label.setBounds(line.removeFromLeft(label_width));
        line.removeFromLeft(gap);
        control.setBounds(line);
    };

    place(*lbl_where_, *cb_where_, area.removeFromTop(row));
    area.removeFromTop(gap);
    place(*lbl_preset_, *cb_preset_, area.removeFromTop(row));
    area.removeFromTop(gap * 2);

    // Two columns: what the filter is on the left, what it is allowed on the right.
    Rectangle<int> columns = area.removeFromTop(row * 8 + gap * 7);
    Rectangle<int> left = columns.removeFromLeft(columns.getWidth() / 2 - gap);
    columns.removeFromLeft(gap * 2);
    Rectangle<int> right = columns;

    const auto next = [&](Rectangle<int> &column) {
        const Rectangle<int> line = column.removeFromTop(row);
        column.removeFromTop(gap);
        return line;
    };
    const auto place_narrow = [&](Label &label, Component &control, Rectangle<int> line) {
        label.setBounds(line.removeFromLeft(line.getWidth() - 96 - gap));
        line.removeFromLeft(gap);
        control.setBounds(line);
    };

    place_narrow(*lbl_method_, *cb_method_, next(left));
    place_narrow(*lbl_window_, *cb_window_, next(left));
    place_narrow(*lbl_phase_, *cb_phase_, next(left));
    for (const std::size_t index : {f_attenuation, f_ripple, f_bandwidth, f_taps})
        place_narrow(*fields_[index].label, *fields_[index].editor, next(left));
    tb_verify_->setBounds(next(left).removeFromRight(96));

    for (const std::size_t index : {f_max_taps, f_stages, f_cepstrum, f_phase_floor, f_remez_max_taps,
                                    f_refine_rounds, f_refine_patience, f_measure_points})
        place_narrow(*fields_[index].label, *fields_[index].editor, next(right));

    area.removeFromTop(gap * 2);
    Rectangle<int> buttons = area.removeFromBottom(row + 4);
    btn_close_->setBounds(buttons.removeFromRight(90));
    buttons.removeFromRight(gap);
    btn_apply_->setBounds(buttons.removeFromRight(90));
    area.removeFromBottom(gap);
    lbl_status_->setBounds(area);
}
