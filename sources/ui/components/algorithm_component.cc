//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Modified for ADLplug-Next. The modifications are distributed under the
// GNU GPL v3 or later; see the accompanying file LICENSE, and
// LICENSE.BSL-1.0.txt for the Boost Software License.

#include "algorithm_component.h"
#include <string_view>

void Algorithm_Component::scale(double s)
{
    if (s == scale_)
        return;
    scale_ = s;
    repaint();
}

void Algorithm_Component::paint(Graphics &g)
{
    if (!algorithm_)
        return;

    const double scale = scale_;
    unsigned row = 0;
    unsigned col = 0;

    Colour pen_color(0xf0, 0xf8, 0xff);
    Colour fill_color(0x42, 0xa2, 0xc8);

    g.setColour(pen_color);
    g.drawRect(getLocalBounds());

    for (const char16_t character : std::u16string_view(algorithm_)) {
        const Rectangle<float> bounds = Rectangle<double>(scale * col, scale * row, scale, scale).toFloat();

        switch (character) {
        case u'│':
            g.setColour(pen_color);
            g.drawLine(Line<float>(bounds.getRelativePoint(0.5f, 0.0f),
                                   bounds.getRelativePoint(0.5f, 1.0f)));
            break;
        case u'─':
            g.setColour(pen_color);
            g.drawLine(Line<float>(bounds.getRelativePoint(0.0f, 0.5f),
                                   bounds.getRelativePoint(1.0f, 0.5f)));
            break;
        case u'┌':
            g.setColour(pen_color);
            g.drawLine(Line<float>(bounds.getRelativePoint(0.5f, 1.0f),
                                   bounds.getRelativePoint(0.5f, 0.5f)));
            g.drawLine(Line<float>(bounds.getRelativePoint(0.5f, 0.5f),
                                   bounds.getRelativePoint(1.0f, 0.5f)));
            break;
        case u'┐':
            g.setColour(pen_color);
            g.drawLine(Line<float>(bounds.getRelativePoint(0.0f, 0.5f),
                                   bounds.getRelativePoint(0.5f, 0.5f)));
            g.drawLine(Line<float>(bounds.getRelativePoint(0.5f, 0.5f),
                                   bounds.getRelativePoint(0.5f, 1.0f)));
            break;
        case u'└':
            g.setColour(pen_color);
            g.drawLine(Line<float>(bounds.getRelativePoint(0.5f, 0.0f),
                                   bounds.getRelativePoint(0.5f, 0.5f)));
            g.drawLine(Line<float>(bounds.getRelativePoint(0.5f, 0.5f),
                                   bounds.getRelativePoint(1.0f, 0.5f)));
            break;
        case u'┘':
            g.setColour(pen_color);
            g.drawLine(Line<float>(bounds.getRelativePoint(0.0f, 0.5f),
                                   bounds.getRelativePoint(0.5f, 0.5f)));
            g.drawLine(Line<float>(bounds.getRelativePoint(0.5f, 0.5f),
                                   bounds.getRelativePoint(0.5f, 0.0f)));
            break;
        case u'┤':
            g.setColour(pen_color);
            g.drawLine(Line<float>(bounds.getRelativePoint(0.0f, 0.5f),
                                   bounds.getRelativePoint(0.5f, 0.5f)));
            g.drawLine(Line<float>(bounds.getRelativePoint(0.5f, 0.0f),
                                   bounds.getRelativePoint(0.5f, 1.0f)));
            break;
        case u'├':
            g.setColour(pen_color);
            g.drawLine(Line<float>(bounds.getRelativePoint(0.5f, 0.5f),
                                   bounds.getRelativePoint(1.0f, 0.5f)));
            g.drawLine(Line<float>(bounds.getRelativePoint(0.5f, 0.0f),
                                   bounds.getRelativePoint(0.5f, 1.0f)));
            break;
        case u'┴':
            g.setColour(pen_color);
            g.drawLine(Line<float>(bounds.getRelativePoint(0.0f, 0.5f),
                                   bounds.getRelativePoint(1.0f, 0.5f)));
            g.drawLine(Line<float>(bounds.getRelativePoint(0.5f, 0.5f),
                                   bounds.getRelativePoint(0.5f, 0.0f)));
            break;
        case u'┬':
            g.setColour(pen_color);
            g.drawLine(Line<float>(bounds.getRelativePoint(0.0f, 0.5f),
                                   bounds.getRelativePoint(1.0f, 0.5f)));
            g.drawLine(Line<float>(bounds.getRelativePoint(0.5f, 0.5f),
                                   bounds.getRelativePoint(0.5f, 1.0f)));
            break;
        case u'┼':
            g.setColour(pen_color);
            g.drawLine(Line<float>(bounds.getRelativePoint(0.0f, 0.5f),
                                   bounds.getRelativePoint(1.0f, 0.5f)));
            g.drawLine(Line<float>(bounds.getRelativePoint(0.5f, 0.0f),
                                   bounds.getRelativePoint(0.5f, 1.0f)));
            break;
        case u' ':
        case u'\n':
            break;
        default:
            g.setColour(fill_color);
            g.fillRoundedRectangle(bounds, 5.0f);
            g.setColour(Colours::black);
            g.drawText(String::charToString(static_cast<juce_wchar>(character)),
                       bounds, Justification::centred, false);
        }

        if (character == u'\n') {
            col = 0;
            ++row;
        }
        else
            ++col;
    }
}
