//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#include "wave_label.h"
#include "waves.h"

Wave_Label::Wave_Label(const Waves &waves)
    : waves_(waves)
{
}

Wave_Label::Wave_Label(const Waves &waves, const String &name)
    : Component(name), waves_(waves)
{
}

void Wave_Label::set_wave(unsigned wave, NotificationType notification)
{
    if (wave_ == wave)
        return;
    wave_ = wave;
    repaint();

    if (notification == dontSendNotification)
        return;

    if (notification == sendNotificationSync)
        handleAsyncUpdate();
    else
        triggerAsyncUpdate();
}

void Wave_Label::add_listener(Listener *l)
{
    listeners_.add(l);
}

void Wave_Label::remove_listener(Listener *l)
{
    listeners_.remove(l);
}

void Wave_Label::handleAsyncUpdate()
{
    cancelPendingUpdate();

    Component::BailOutChecker checker(this);
    listeners_.callChecked(checker, [this](Wave_Label::Listener &l) { l.wave_changed(this); });
}

void Wave_Label::paint(Graphics &g)
{
    const Rectangle<double> area = getLocalBounds().reduced(2, 2).toDouble();
    const auto centre_y = static_cast<float>(area.getCentreY());

    g.setColour(Colour::fromRGBA(0xa0, 0xa0, 0xa0, 0xff));
    g.drawLine(static_cast<float>(area.getX()), centre_y, static_cast<float>(area.getRight() - 1), centre_y);

    const Rectangle<double> wave_area = area.reduced(12, 0);
    const int w = static_cast<int>(wave_area.getWidth());
    if (w < 2)
        return;

    g.setColour(Colour::fromRGBA(0xff, 0x00, 0x00, 0xff));
    Point<float> xy_last;
    for (int i = -1; i < w; ++i) {
        const double phase = i * (1.0 / (w - 1));
        const double value = waves_.compute_wave(wave_, phase);
        const Point<float> xy_new = Point<double>(
            wave_area.getX() + i, area.getY() + area.getHeight() * 0.5 * (1.0 - value)).toFloat();
        if (i >= 0)
            g.drawLine(Line<float>(xy_last, xy_new), 1.5f);
        xy_last = xy_new;
    }
}
