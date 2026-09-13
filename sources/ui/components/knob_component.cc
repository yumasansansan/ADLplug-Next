//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#include "ui/components/knob_component.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

Knob::Knob()
    = default;

Knob::Knob(const String &name)
    : Component(name)
{
}

Km_Skin *Knob::skin() const
{
    return skin_.get();
}

void Knob::set_skin(Km_Skin *skin)
{
    if (skin_.get() != skin) {
        skin_ = skin;
        repaint();
    }
}

void Knob::set_value(double v, NotificationType notification)
{
    v = (v < min_) ? min_ : (v > max_) ? max_ : v;
    if (v == value_)
        return;
    value_ = v;
    repaint();

    if (notification == dontSendNotification)
        return;

    if (notification == sendNotificationSync)
        handleAsyncUpdate();
    else
        triggerAsyncUpdate();
}

void Knob::set_range(double min, double max)
{
    jassert(min < max);

    min_ = min;
    max_ = max;
    set_value(value_, dontSendNotification);
}

void Knob::set_max_increment(double maxinc)
{
    max_increment_ = maxinc;
}

void Knob::add_listener(Listener *l)
{
    listeners_.add(l);
}

void Knob::remove_listener(Listener *l)
{
    listeners_.remove(l);
}

void Knob::handleAsyncUpdate()
{
    cancelPendingUpdate();

    Component::BailOutChecker checker(this);
    listeners_.callChecked(checker, [this](Knob::Listener &l) { l.knob_value_changed(this); });
}

void Knob::paint(Graphics &g)
{
    const Km_Skin *skin = skin_.get();
    if (!skin || !*skin)
        return;

    const std::vector<Image> &frames = skin->frames;
    const std::size_t last = frames.size() - 1;

    const double ratio = std::clamp((value_ - min_) / (max_ - min_), 0.0, 1.0);
    const auto index = static_cast<std::size_t>(std::lround(ratio * static_cast<double>(last)));

    g.drawImage(frames[std::min(index, last)], get_frame_bounds());
}

void Knob::mouseWheelMove(const MouseEvent &event, const MouseWheelDetails &wheel)
{
    if (in_drag_ || wheel.deltaY == 0)
        return;

    const Rectangle<int> frame_bounds = get_frame_bounds().toType<int>();
    if (!frame_bounds.contains(event.getPosition()))
        return;

    Component::BailOutChecker checker(this);
    listeners_.callChecked(checker, [this](Knob::Listener &l) { l.knob_drag_started(this); });
    if (checker.shouldBailOut())
        return;

    double inc = 0.5 * (max_ - min_) * static_cast<double>(wheel.deltaY);
    if (max_increment_ > 0)
        inc = std::copysign(std::min(std::fabs(inc), max_increment_), inc);

    set_value(value_ + inc, sendNotificationSync);
    if (checker.shouldBailOut())
        return;

    listeners_.callChecked(checker, [this](Knob::Listener &l) { l.knob_drag_ended(this); });
}

void Knob::mouseDown(const MouseEvent &event)
{
    if (in_drag_)
        return;

    const Rectangle<int> frame_bounds = get_frame_bounds().toType<int>();
    if (!frame_bounds.contains(event.getPosition()))
        return;

    in_drag_ = true;
    Component::BailOutChecker checker(this);
    listeners_.callChecked(checker, [this](Knob::Listener &l) { l.knob_drag_started(this); });
    if (checker.shouldBailOut())
        return;

    handle_drag(event);
}

void Knob::mouseUp([[maybe_unused]] const MouseEvent &event)
{
    if (!in_drag_)
        return;

    in_drag_ = false;
    Component::BailOutChecker checker(this);
    listeners_.callChecked(checker, [this](Knob::Listener &l) { l.knob_drag_ended(this); });
}

void Knob::mouseDrag(const MouseEvent &event)
{
    if (!in_drag_)
        return;

    handle_drag(event);
}

void Knob::handle_drag(const MouseEvent &event)
{
    const Km_Skin *skin = skin_.get();
    if (!skin)
        return;

    const Rectangle<double> bounds = get_frame_bounds().toDouble();
    const auto x = static_cast<double>(event.position.x);
    const auto y = static_cast<double>(event.position.y);

    if (skin->style == Km_Rotary) {
        const double dx = x - bounds.getCentreX();
        const double dy = y - bounds.getCentreY();

        if (dx * dx + dy * dy > 25.0) {
            const double angle = std::clamp(std::atan2(dx, -dy), min_angle_, max_angle_);
            const double r = (angle - min_angle_) / (max_angle_ - min_angle_);
            set_value(min_ + r * (max_ - min_), sendNotificationSync);
        }
    }
    else if (skin->style == Km_LinearHorizontal) {
        const double w = bounds.getWidth();
        if (w > 0) {
            const double r = std::clamp((x - bounds.getX()) / w, 0.0, 1.0);
            set_value(min_ + r * (max_ - min_), sendNotificationSync);
        }
    }
}

Rectangle<float> Knob::get_frame_bounds() const
{
    const Km_Skin *skin = skin_.get();
    if (!skin || !*skin)
        return {};

    const Rectangle<int> frame = skin->frames[0].getBounds();
    return getLocalBounds().toFloat().withSizeKeepingCentre(
        static_cast<float>(frame.getWidth()), static_cast<float>(frame.getHeight()));
}
