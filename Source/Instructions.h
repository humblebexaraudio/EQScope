#pragma once
// Built-in instructions, opened with "? INSTRUCTIONS" on the faceplate. Scrolls on small screens;
// click outside the card, or CLOSE, to dismiss.

#include <juce_gui_basics/juce_gui_basics.h>

class InstructionsOverlay : public juce::Component
{
public:
    InstructionsOverlay()
    {
        closeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2e35));
        closeButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe8ebef));
        closeButton.onClick = [this] { setVisible (false); };
        addAndMakeVisible (closeButton);
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.55f));
        g.setColour (juce::Colour (0xf2171a1f));
        g.fillRoundedRectangle (card, 10.0f);
        g.setColour (juce::Colour (0xff3a3f47));
        g.drawRoundedRectangle (card, 10.0f, 1.2f);
        g.setColour (juce::Colour (0xffffa21f));
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)).withExtraKerningFactor (0.08f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("EQSCOPE  \xc2\xb7  INSTRUCTIONS")), card.reduced (18.0f, 12.0f).removeFromTop (22.0f), juce::Justification::centredLeft);
    }

    void resized() override
    {
        const auto b = getLocalBounds().toFloat();
        card = b.withSizeKeepingCentre (juce::jmin (620.0f, b.getWidth() - 24.0f), juce::jmin (560.0f, b.getHeight() - 24.0f));
        auto inner = card.reduced (18.0f, 12.0f).toNearestInt();
        inner.removeFromTop (30);
        closeButton.setBounds (inner.removeFromBottom (30).removeFromRight (90));
        inner.removeFromBottom (8);
        viewport.setBounds (inner);
        content.layoutFor (inner.getWidth() - viewport.getScrollBarThickness() - 4);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! card.contains (e.position))
            setVisible (false);
    }

private:
    struct Content : public juce::Component
    {
        void layoutFor (int width)
        {
            juce::AttributedString s;
            s.setLineSpacing (3.0f);
            const auto head = juce::Font (juce::FontOptions (13.5f, juce::Font::bold));
            const auto body = juce::Font (juce::FontOptions (13.0f));
            const juce::Colour amber (0xffffb14a), text (0xffd6dbe1);
            auto h = [&] (const char* t) { s.append (juce::String (juce::CharPointer_UTF8 (t)) + "\n", head, amber); };
            auto p = [&] (const char* t) { s.append (juce::String (juce::CharPointer_UTF8 (t)) + "\n\n", body, text); };

            h ("Set up");
            p ("Put three plug-ins on one channel:  EQScope (SIGNAL) \xe2\x86\x92 your EQ \xe2\x86\x92 EQScope (ANALYZE).\n"
               "In Logic, use an Aux channel strip: it is always live, so you don't need to press play. "
               "On iPad or iPhone, use three effect slots on one channel in your AUv3 host.");
            h ("Read the response");
            p ("Open the ANALYZE copy. When the green LOCKED light is on, the curve is exact. Turn your EQ and the curve "
               "follows within about 0.18 s. Hover over the graph (or tap it) to read frequency, gain and phase.");
            h ("Controls");
            p ("SIGNAL / ANALYZE: what this copy does.\n"
               "OUTPUT: probe signal on/off (SIGNAL).\n"
               "FINE: longer probe for very narrow bands below about 60 Hz; the curve then takes about 0.7 s to follow (SIGNAL).\n"
               "CHANNEL: measure MID, L or R (ANALYZE).\n"
               "MONITOR: off = this copy is silent; on = audio passes through (ANALYZE).\n"
               "RANGE, SMOOTH: display scale and smoothing.  PHASE: show the phase response.\n"
               "SNAPSHOT / CLEAR: keep the current curve as an orange overlay to compare against.\n"
               "ZERO: treat the current response as flat; press again to remove.");
            h ("Status lights");
            p ("LOCKED (green): exact.  SETTLING (amber): a knob is moving.  NO PROBE (red): the audio isn't the probe "
               "signal; put an EQScope in SIGNAL mode before your EQ.  All off: no audio is arriving.\n"
               "If the screen says the EQ rings longer than the Fast probe, press FINE on the SIGNAL copy.");
            h ("Finish");
            p ("Choose Silver, Gunmetal Blue, Burnt Orange or Vintage with the FINISH swatches at the bottom left, "
               "by right-clicking the faceplate, or by clicking the EQSCOPE logo. The last one you pick becomes the default.");
            h ("More");
            p ("The full user guide is installed with the plug-in: on macOS in /Applications/Humble Bexar Audio, on Windows "
               "in the Start menu under Humble Bexar Audio.");

            layout.createLayout (s, (float) juce::jmax (100, width));
            setSize (juce::jmax (100, width), (int) std::ceil (layout.getHeight()) + 8);
        }

        void paint (juce::Graphics& g) override { layout.draw (g, getLocalBounds().toFloat()); }

        juce::TextLayout layout;
    };

    juce::Rectangle<float> card;
    juce::TextButton closeButton { "CLOSE" };
    juce::Viewport viewport;
    Content content;
};
