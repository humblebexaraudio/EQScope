// BexarLogo.h — Humble Bexar Audio bear head logo for the faceplate label.
//
// Every faceplate's "Humble Bexar Audio" label starts with the bear head logo:  [bear] HUMBLE BEXAR AUDIO
//   humble-bexar-bear-head.png        Silver, Burnt Orange, Vintage, Glacier Blue, Pecan, Polished Enamel Cream
//   humble-bexar-bear-head-cream.png  Gunmetal Blue, Polished Enamel Black (dark finishes)
// Sizing (Humble-Bexar-Logo-Options.md): logo height about 1.6x the label's cap height, vertically
// centred on the text, with a gap of about half the logo width before the "H".
// The images are compiled in from Resources/logo by the BexarLogoData binary-data target (CMakeLists.txt).
#pragma once

#include <juce_graphics/juce_graphics.h>
#include "BexarLogoData.h"

namespace bexar
{
    /** The bear head for a finish: the cream-outline version on the dark finishes, the black-filled one elsewhere. */
    inline juce::Image bearHead (bool darkFinish)
    {
        static const juce::Image normal = juce::ImageCache::getFromMemory (BexarLogoData::humblebexarbearhead_png,
                                                                           BexarLogoData::humblebexarbearhead_pngSize);
        static const juce::Image cream  = juce::ImageCache::getFromMemory (BexarLogoData::humblebexarbearheadcream_png,
                                                                           BexarLogoData::humblebexarbearheadcream_pngSize);
        return darkFinish ? cream : normal;
    }

    /** Draws the bear head in front of a one-line label and returns the area to draw the label's text in.

        Pass the text, font, area and justification you would give drawText(). Right-aligned labels keep
        their position and the bear goes in front; centred labels are re-centred as [bear + gap + text];
        left-aligned labels move right to make room. */
    inline juce::Rectangle<float> drawBearBeforeLabel (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                                                      juce::Rectangle<float> area, juce::Justification just, bool darkFinish)
    {
        const auto img = bearHead (darkFinish);
        if (! img.isValid())
            return area;

        const float capHeight = font.getHeight() * 0.62f;
        const float logoH     = capHeight * 1.6f;
        const float logoW     = logoH * (float) img.getWidth() / (float) img.getHeight();
        const float gap       = logoW * 0.5f;
        const float textW     = juce::GlyphArrangement::getStringWidth (font, text);

        float logoX;
        auto textArea = area;

        if (just.testFlags (juce::Justification::right))
        {
            logoX = area.getRight() - textW - gap - logoW;
        }
        else if (just.testFlags (juce::Justification::horizontallyCentred))
        {
            logoX = area.getCentreX() - (logoW + gap + textW) * 0.5f;
            textArea = area.translated ((logoW + gap) * 0.5f, 0.0f);
        }
        else
        {
            logoX = area.getX();
            textArea = area.withTrimmedLeft (logoW + gap);
        }

        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (1.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (img, { logoX, area.getCentreY() - logoH * 0.5f, logoW, logoH }, juce::RectanglePlacement::stretchToFit);
        return textArea;
    }

    /** Width the bear and its gap add in front of a label drawn in this font (for layouts that measure labels). */
    inline float bearWidthFor (const juce::Font& font)
    {
        const float logoH = font.getHeight() * 0.62f * 1.6f;
        return logoH * (253.0f / 192.0f) * 1.5f;
    }
}
