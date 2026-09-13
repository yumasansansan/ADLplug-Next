//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#include "ui/components/vu_meter.h"
#include <algorithm>
#include <cmath>

namespace {

constexpr double default_hue_start = 210.0;
constexpr double default_hue_range = -240.0;

}  // namespace

Vu_Meter::Vu_Meter()
    : Vu_Meter(String())
{
}

Vu_Meter::Vu_Meter(const String &name)
    : Component(name)
    , hue_start_(default_hue_start / 360.0)
    , hue_range_(default_hue_range / 360.0)
{
    update_gradient();
}

void Vu_Meter::set_value(double value)
{
    if (value_ == value)
        return;
    value_ = value;
    repaint();
}

void Vu_Meter::set_logarithmic(bool logarithmic)
{
    if (logarithmic_ == logarithmic)
        return;
    logarithmic_ = logarithmic;
    repaint();
}

void Vu_Meter::set_hue(double start, double range)
{
    start *= 1.0 / 360.0;
    range *= 1.0 / 360.0;
    if (hue_start_ == start && hue_range_ == range)
        return;
    hue_start_ = start;
    hue_range_ = range;
    update_gradient();
    repaint();
}

void Vu_Meter::set_num_stops(unsigned num_stops)
{
    jassert(num_stops >= 2);
    if (num_stops_ == num_stops)
        return;
    num_stops_ = num_stops;
    update_gradient();
    repaint();
}

void Vu_Meter::paint(Graphics &g)
{
    const Rectangle<int> bounds = getLocalBounds().reduced(1, 1);
    const int w = bounds.getWidth();
    if (w <= 0)
        return;

    const double value = value_;
    double logvalue = 0;
    if (!logarithmic_)
        logvalue = value;
    else if (value > 0) {
        const double db = 20 * std::log10(value);
        constexpr double dbmin = -60.0;
        logvalue = (db - dbmin) / (0 - dbmin);
    }

    const int w2 = std::min(w, static_cast<int>(std::lround(w * logvalue)));

    const Rectangle<float> area = bounds.toFloat();
    ColourGradient gradient = gradient_;
    gradient.point1 = area.getTopLeft();
    gradient.point2 = area.getTopRight();
    g.setGradientFill(gradient);
    g.fillRect(bounds.withWidth(w2));
}

void Vu_Meter::update_gradient()
{
    gradient_.clearColours();
    for (unsigned s = 0; s < num_stops_; ++s) {
        const double r = s / static_cast<double>(num_stops_ - 1);
        const double hue = hue_start_ + r * hue_range_;
        gradient_.addColour(r, Colour::fromHSV(static_cast<float>(hue), 0.75f, 0.75f, 1.0f));
    }
}
