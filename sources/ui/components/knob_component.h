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

#include "ui/utility/knobman_skin.h"
#include "JuceHeader.h"
#include <numbers>

class Knob : public Component,
             public AsyncUpdater,
             public SettableTooltipClient
{
public:
    Knob();
    explicit Knob(const String &name);
    ~Knob() override = default;

    [[nodiscard]] Km_Skin *skin() const;
    void set_skin(Km_Skin *skin);

    [[nodiscard]] double value() const
        { return value_; }
    void set_value(double v, NotificationType notification);

    [[nodiscard]] double min() const
        { return min_; }
    [[nodiscard]] double max() const
        { return max_; }
    void set_range(double min, double max);

    void set_max_increment(double maxinc);

    [[nodiscard]] bool is_dragging() const
        { return in_drag_; }

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void knob_value_changed([[maybe_unused]] Knob *k) {}
        virtual void knob_drag_started([[maybe_unused]] Knob *k) {}
        virtual void knob_drag_ended([[maybe_unused]] Knob *k) {}
    };

    void add_listener(Listener *l);
    void remove_listener(Listener *l);

protected:
    void handleAsyncUpdate() override;
    void paint(Graphics &g) override;
    void mouseWheelMove(const MouseEvent &event, const MouseWheelDetails &wheel) override;
    void mouseDown(const MouseEvent &event) override;
    void mouseUp(const MouseEvent &event) override;
    void mouseDrag(const MouseEvent &event) override;

private:
    void handle_drag(const MouseEvent &event);
    [[nodiscard]] Rectangle<float> get_frame_bounds() const;
    Km_Skin_Ptr skin_;
    double value_ = 0;
    double min_ = 0;
    double max_ = 1;
    double max_increment_ = 0;
    ListenerList<Listener> listeners_;
    bool in_drag_ = false;
    static constexpr double min_angle_ = std::numbers::pi * -3.0 / 4.0;
    static constexpr double max_angle_ = std::numbers::pi * +3.0 / 4.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Knob)
};
