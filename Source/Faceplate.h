#pragma once
// The EQScope faceplate: brushed metal in four finishes, screws, LEDs, push buttons and selector knobs.
// Everything is drawn in code (no image files), so it stays sharp at any size and on Retina/iPad screens.

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

namespace faceplate
{
namespace col
{
    static const juce::Colour ledAmber   { 0xffffa21f };
    static const juce::Colour ledGreen   { 0xff38e070 };
    static const juce::Colour ledRed     { 0xffff3b30 };
    static const juce::Colour capText    { 0xffd9dde2 };
}

//==============================================================================
// Finishes (same set and order as SoftSpoken and Keystone).
enum Finish { silver = 0, gunmetalBlue, burntOrange, vintage, numFinishes };

struct Palette
{
    const char* name;
    juce::Colour metal;                  // brushed plate base colour
    juce::Colour ink, inkSoft;           // lettering (engraved and paint-filled)
    juce::Colour engraveEdge;            // the 1 px edge under the lettering: light on light plates, dark on dark ones
    juce::Colour capLight, capDark;      // knob caps
    juce::Colour screwLight, screwDark;  // screw heads
    juce::Colour bezelLight, bezelDark;  // screen surround
    float sheen;                         // strength of the key-light sheen
    float wear;                          // darker worn edges (Vintage)
    float patina;                        // light mottled patina (Vintage)
};

inline const Palette& palette (int finish)
{
    static const std::array<Palette, numFinishes> p {{
        { "Silver",
          juce::Colour (0xffc8cbcf), juce::Colour (0xff1d2024), juce::Colour (0xff474c53), juce::Colour (0x8cffffff),
          juce::Colour (0xfff1f3f5), juce::Colour (0xff8b9198), juce::Colour (0xfff6f7f8), juce::Colour (0xff80868d),
          juce::Colour (0xfff0f2f4), juce::Colour (0xff6d737a), 0.13f, 0.0f, 0.0f },
        { "Gunmetal Blue",
          juce::Colour (0xff3f4b5d), juce::Colour (0xfff1f4f8), juce::Colour (0xffbcc6d3), juce::Colour (0x8c000000),
          juce::Colour (0xffdfe5ec), juce::Colour (0xff6b7686), juce::Colour (0xffb7c1ce), juce::Colour (0xff3a4250),
          juce::Colour (0xff8a96a6), juce::Colour (0xff1e242d), 0.09f, 0.0f, 0.0f },
        { "Burnt Orange",
          juce::Colour (0xffb0561f), juce::Colour (0xfffbf3ea), juce::Colour (0xfff2d6bd), juce::Colour (0x80000000),
          juce::Colour (0xfff1f3f5), juce::Colour (0xff8b9198), juce::Colour (0xfff0e6dc), juce::Colour (0xff7a5a44),
          juce::Colour (0xffe0a57a), juce::Colour (0xff5a2a10), 0.11f, 0.0f, 0.0f },
        { "Vintage",
          juce::Colour (0xffcdb784), juce::Colour (0xff33220f), juce::Colour (0xff47331a), juce::Colour (0x66fff6dc),
          juce::Colour (0xfff6e6b8), juce::Colour (0xff9a7b40), juce::Colour (0xffeedcab), juce::Colour (0xff7a6034),
          juce::Colour (0xfff3e4bd), juce::Colour (0xff6e5530), 0.10f, 1.0f, 1.0f },
    }};
    return p[(size_t) juce::jlimit (0, (int) numFinishes - 1, finish)];
}

inline juce::Font font (float size, bool bold = true, float kerning = 0.0f)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (kerning);
}

//==============================================================================
// Brushed texture tile in a finish's colour. Each row's grain is blurred circularly, so the tile
// repeats with no visible seam.
inline juce::Image makeBrushedTile (juce::Colour base)
{
    constexpr int w = 1024, h = 256;
    juce::Image img (juce::Image::RGB, w, h, false);
    juce::Random rng (0x45b3);
    std::vector<float> row ((size_t) w), tmp ((size_t) w);
    // Darker metals show the grain less in absolute terms; lift it so it still reads.
    const float depth = base.getPerceivedBrightness() < 0.45f ? 1.6f : 1.0f;

    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
    {
        const float rowBase = (rng.nextFloat() - 0.5f) * 0.05f + (rng.nextFloat() < 0.04f ? (rng.nextFloat() - 0.5f) * 0.08f : 0.0f);
        for (auto& v : row) v = rng.nextFloat() - 0.5f;

        for (int pass = 0; pass < 2; ++pass) // two circular box blurs = long horizontal streaks
        {
            constexpr int rad = 28;
            float acc = 0.0f;
            for (int i = -rad; i <= rad; ++i) acc += row[(size_t) ((i + w) % w)];
            for (int x = 0; x < w; ++x)
            {
                tmp[(size_t) x] = acc / (2 * rad + 1);
                acc += row[(size_t) ((x + rad + 1) % w)] - row[(size_t) ((x - rad + w) % w)];
            }
            std::swap (row, tmp);
        }

        for (int x = 0; x < w; ++x)
        {
            const float v = (rowBase + row[(size_t) x] * 0.9f + (rng.nextFloat() - 0.5f) * 0.035f) * depth;
            const float b = juce::jmax (0.0f, 1.0f + v);
            bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (juce::jmin (1.0f, base.getFloatRed() * b),
                                                                  juce::jmin (1.0f, base.getFloatGreen() * b),
                                                                  juce::jmin (1.0f, base.getFloatBlue() * b), 1.0f));
        }
    }
    return img;
}

// The plate: brushed tile, lighting, Vintage's wear and patina, and a machined edge.
inline void drawPlate (juce::Graphics& g, juce::Rectangle<float> r, const juce::Image& tile, const Palette& pal)
{
    g.setTiledImageFill (tile, 0, 0, 1.0f);
    g.fillRect (r);

    juce::ColourGradient light (juce::Colours::white.withAlpha (0.20f), r.getX(), r.getY(),
                                juce::Colours::black.withAlpha (0.14f), r.getX(), r.getBottom(), false);
    g.setGradientFill (light);
    g.fillRect (r);

    // A broad diagonal sheen, as on real brushed metal under a key light.
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.0f), r.getX(), r.getY(),
                                juce::Colours::white.withAlpha (0.0f), r.getRight(), r.getBottom(), false);
    sheen.addColour (0.32, juce::Colours::white.withAlpha (pal.sheen));
    sheen.addColour (0.45, juce::Colours::white.withAlpha (0.0f));
    g.setGradientFill (sheen);
    g.fillRect (r);

    if (pal.patina > 0.0f)
    {
        // Light mottled patina: soft, fixed blotches (same pattern every time, scaled to the plate).
        juce::Random rng (0x7a71a);
        for (int i = 0; i < 34; ++i)
        {
            const float cx = r.getX() + rng.nextFloat() * r.getWidth();
            const float cy = r.getY() + rng.nextFloat() * r.getHeight();
            const float rad = (0.04f + 0.10f * rng.nextFloat()) * juce::jmax (r.getWidth(), r.getHeight());
            const juce::Colour c = (i % 3 == 0 ? juce::Colour (0xff6f7d5c) : juce::Colour (0xff7a5a2e))
                                       .withAlpha ((0.08f + 0.08f * rng.nextFloat()) * pal.patina);
            juce::ColourGradient blot (c, cx, cy, c.withAlpha (0.0f), cx + rad, cy, true);
            g.setGradientFill (blot);
            g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
        }
    }

    if (pal.wear > 0.0f)
    {
        // Darker worn edges, heaviest in the corners where hands and racks touch.
        const float band = juce::jmin (r.getWidth(), r.getHeight()) * 0.14f;
        const juce::Colour dark = juce::Colour (0xff3d2a12).withAlpha (0.55f * pal.wear);
        const juce::Colour none = dark.withAlpha (0.0f);
        auto edge = [&] (float x1, float y1, float x2, float y2, juce::Rectangle<float> area)
        {
            juce::ColourGradient eg (dark, x1, y1, none, x2, y2, false);
            g.setGradientFill (eg);
            g.fillRect (area);
        };
        edge (r.getX(), 0, r.getX() + band, 0, r.withWidth (band));
        edge (r.getRight(), 0, r.getRight() - band, 0, r.withLeft (r.getRight() - band));
        edge (0, r.getY(), 0, r.getY() + band, r.withHeight (band));
        edge (0, r.getBottom(), 0, r.getBottom() - band, r.withTop (r.getBottom() - band));
    }

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawRect (r.reduced (1.0f), 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRect (r, 1.0f);
}

// Lettering with a 1 px edge below it (a lit edge on light plates, a shadow on dark ones).
inline void drawEngraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
                          const juce::Font& f, juce::Justification j, const Palette& pal, bool soft = false)
{
    g.setFont (f);
    g.setColour (pal.engraveEdge);
    g.drawText (text, r.translated (0.0f, 1.0f), j, false);
    g.setColour (soft ? pal.inkSoft : pal.ink);
    g.drawText (text, r, j, false);
}

// Phillips-head screw sitting in a countersink.
inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle, const Palette& pal)
{
    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.fillEllipse (juce::Rectangle<float> (r * 2.5f, r * 2.5f).withCentre (c.translated (0.0f, 0.8f)));

    juce::ColourGradient sink (pal.metal.darker (0.5f), c.x, c.y - r * 1.2f, pal.metal.brighter (0.5f), c.x, c.y + r * 1.2f, false);
    g.setGradientFill (sink);
    g.fillEllipse (juce::Rectangle<float> (r * 2.35f, r * 2.35f).withCentre (c));

    juce::ColourGradient head (pal.screwLight, c.x - r * 0.45f, c.y - r * 0.55f,
                               pal.screwDark, c.x + r * 0.6f, c.y + r * 0.9f, true);
    g.setGradientFill (head);
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 0.7f);

    for (int i = 0; i < 2; ++i)
    {
        juce::Path slot;
        slot.addRoundedRectangle (-r * 0.68f, -r * 0.14f, r * 1.36f, r * 0.28f, r * 0.1f);
        const auto t = juce::AffineTransform::rotation (angle + (float) i * juce::MathConstants<float>::halfPi).translated (c);
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.fillPath (slot, t.translated (0.0f, 0.6f));
        g.setColour (pal.screwDark.darker (0.9f));
        g.fillPath (slot, t);
    }
}

// Panel LED with a chrome bezel. 'level' 0 = off, 1 = fully lit.
inline void drawLed (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, float level)
{
    if (level > 0.0f)
    {
        juce::ColourGradient glow (colour.withAlpha (0.45f * level), c.x, c.y, colour.withAlpha (0.0f), c.x + r * 3.2f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (juce::Rectangle<float> (r * 6.4f, r * 6.4f).withCentre (c));
    }

    juce::ColourGradient bezel (juce::Colour (0xfff2f3f5), c.x, c.y - r * 1.4f, juce::Colour (0xff5d6268), c.x, c.y + r * 1.4f, false);
    g.setGradientFill (bezel);
    g.fillEllipse (juce::Rectangle<float> (r * 2.7f, r * 2.7f).withCentre (c));
    g.setColour (juce::Colour (0xff15171a));
    g.fillEllipse (juce::Rectangle<float> (r * 2.25f, r * 2.25f).withCentre (c));

    const juce::Colour off = colour.withMultipliedSaturation (0.55f).withMultipliedBrightness (0.28f);
    const juce::Colour body = off.interpolatedWith (colour, level);
    juce::ColourGradient lens (body.brighter (0.25f + 0.9f * level), c.x - r * 0.25f, c.y - r * 0.3f,
                               body.darker (0.6f), c.x + r, c.y + r, true);
    g.setGradientFill (lens);
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.fillEllipse (juce::Rectangle<float> (r * 0.8f, r * 0.55f).withCentre (c.translated (-r * 0.3f, -r * 0.45f)));
}

// A recessed window for the display screen.
inline void drawScreenBezel (juce::Graphics& g, juce::Rectangle<float> screen, const Palette& pal)
{
    const auto outer = screen.expanded (7.0f);
    juce::ColourGradient rim (pal.bezelDark, outer.getX(), outer.getY(), pal.bezelLight, outer.getX(), outer.getBottom(), false);
    g.setGradientFill (rim);
    g.fillRoundedRectangle (outer, 7.0f);
    g.setColour (juce::Colour (0xff0b0c0e));
    g.fillRoundedRectangle (screen.expanded (3.0f), 4.0f);
}

//==============================================================================
// Black push button with an LED above its cap.
class LedButton : public juce::Button
{
public:
    LedButton (const juce::String& name, juce::Colour led = col::ledAmber) : juce::Button (name), ledColour (led)
    {
        setButtonText (name);
    }

    std::function<bool()> isLit;  // default: the toggle state
    bool hasLed = true;
    bool reserveLedSpace = true;  // false: a short button with no LED row (e.g. ? INSTRUCTIONS)

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat();
        const float ledR = juce::jlimit (2.6f, 4.2f, r.getHeight() * 0.075f);
        auto ledArea = reserveLedSpace ? r.removeFromTop (ledR * 2.0f + 8.0f) : juce::Rectangle<float>();
        auto cap = r.reduced (1.5f, 0.0f).withTrimmedBottom (2.0f);

        const bool lit = isEnabled() && (isLit ? isLit() : getToggleState());
        if (hasLed && reserveLedSpace)
            drawLed (g, ledArea.getCentre().translated (0.0f, -1.0f), ledR, ledColour, lit ? 1.0f : 0.0f);

        // shadow, then the cap (pushed down by 1 px while held)
        juce::Path capPath;
        capPath.addRoundedRectangle (cap, 4.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (down ? 0.25f : 0.5f), down ? 2 : 5, { 0, down ? 1 : 2 }).drawForPath (g, capPath);
        if (down) cap = cap.translated (0.0f, 1.0f);

        juce::ColourGradient body (juce::Colour (over ? 0xff3a3e44 : 0xff32353a), cap.getX(), cap.getY(),
                                   juce::Colour (0xff111214), cap.getX(), cap.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (cap, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawHorizontalLine ((int) cap.getY() + 1, cap.getX() + 3.0f, cap.getRight() - 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (cap, 4.0f, 1.0f);

        const float fs = juce::jlimit (8.5f, 12.0f, cap.getHeight() * (reserveLedSpace ? 0.36f : 0.44f));
        g.setFont (font (fs, true, 0.06f));
        g.setColour ((lit ? ledColour.interpolatedWith (col::capText, 0.35f) : col::capText).withAlpha (isEnabled() ? 1.0f : 0.35f));
        g.drawFittedText (getButtonText(), cap.reduced (3.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }

private:
    juce::Colour ledColour;
};

//==============================================================================
// Knob drawing for the selector switches: black knurled skirt, spun-metal cap, white pointer.
class PanelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    const Palette* pal = &palette (silver);

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        const float r = (float) juce::jmin (w, h) * 0.5f - 2.0f;
        const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
        const float angle = startAngle + pos * (endAngle - startAngle);

        juce::Path body;
        body.addEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
        juce::DropShadow (juce::Colours::black.withAlpha (0.55f), juce::roundToInt (r * 0.35f), { 0, juce::roundToInt (r * 0.16f) }).drawForPath (g, body);

        juce::ColourGradient skirt (juce::Colour (0xff3c3f45), c.x, c.y - r, juce::Colour (0xff0e0f11), c.x, c.y + r, false);
        g.setGradientFill (skirt);
        g.fillPath (body);

        // knurled edge
        for (int i = 0; i < 60; ++i)
        {
            const float a = (float) i / 60.0f * juce::MathConstants<float>::twoPi;
            const juce::Point<float> p1 (c.x + std::sin (a) * r * 0.80f, c.y - std::cos (a) * r * 0.80f);
            const juce::Point<float> p2 (c.x + std::sin (a) * r * 0.98f, c.y - std::cos (a) * r * 0.98f);
            const float lightness = 0.5f + 0.5f * -std::cos (a); // lit from above
            g.setColour (juce::Colours::white.withAlpha ((i % 2 == 0 ? 0.13f : 0.03f) * (0.4f + lightness)));
            g.drawLine ({ p1, p2 }, 1.0f);
        }

        // spun metal cap (aluminium, blued steel or brass, depending on the finish)
        const float capR = r * 0.64f;
        juce::ColourGradient cap (pal->capLight, c.x - capR * 0.4f, c.y - capR * 0.6f,
                                  pal->capDark, c.x + capR * 0.5f, c.y + capR, true);
        g.setGradientFill (cap);
        g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c));
        for (int i = 1; i < 9; ++i)
        {
            g.setColour ((i % 2 ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f));
            g.drawEllipse (juce::Rectangle<float> (capR * 2.0f * (float) i / 9.0f, capR * 2.0f * (float) i / 9.0f).withCentre (c), 0.8f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c), 1.0f);

        // pointer: white line on the skirt, engraved line on the cap
        const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
        g.setColour (juce::Colour (0xfff5f5f2));
        g.drawLine ({ c + dir * (capR + r * 0.06f), c + dir * (r * 0.97f) }, juce::jmax (1.6f, r * 0.09f));
        g.setColour (juce::Colour (0xff3a3e44));
        g.drawLine ({ c + dir * (capR * 0.15f), c + dir * (capR * 0.92f) }, juce::jmax (1.2f, r * 0.06f));
    }
};

//==============================================================================
// Stepped rotary selector with its positions printed around it, like a hardware rotary switch.
// Drag up/down (or turn with a scroll wheel), click a printed position to jump to it, double-click to reset.
class SelectorKnob : public juce::Component
{
public:
    SelectorKnob (const juce::String& captionText, juce::StringArray positionLabels)
        : caption (captionText), labels (std::move (positionLabels))
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setMouseDragSensitivity (60 * juce::jmax (1, labels.size() - 1));
        const float sweep = juce::jmin (250.0f, 50.0f * (float) (labels.size() - 1)) * juce::MathConstants<float>::pi / 180.0f;
        startAngle = juce::MathConstants<float>::twoPi - sweep * 0.5f;
        endAngle = juce::MathConstants<float>::twoPi + sweep * 0.5f;
        slider.setRotaryParameters (startAngle, endAngle, true);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
    }

    juce::Slider slider;
    const Palette* pal = &palette (silver);

    void paint (juce::Graphics& g) override
    {
        drawEngraved (g, caption, getLocalBounds().toFloat().removeFromTop (captionH), font (captionH * 0.86f, true, 0.1f),
                      juce::Justification::centred, *pal);

        const auto kb = slider.getBounds().toFloat();
        const auto c = kb.getCentre();
        const float r = kb.getWidth() * 0.5f - 2.0f;
        const int n = labels.size();
        const int current = juce::roundToInt (slider.getValue());
        for (int i = 0; i < n; ++i)
        {
            const float a = angleFor (i);
            const juce::Point<float> dir (std::sin (a), -std::cos (a));
            g.setColour (pal->ink.withAlpha (0.8f));
            g.drawLine ({ c + dir * (r + 1.5f), c + dir * (r + 4.5f) }, 1.2f);

            const juce::Point<float> lp = c + dir * (r + 4.5f + labelGap * 0.55f);
            const float tw = juce::GlyphArrangement::getStringWidth (font (labelFont, true), labels[i]) + 2.0f;
            juce::Rectangle<float> tr (tw, labelFont + 2.0f);
            tr.setCentre (lp.translated (dir.x * tw * 0.25f, 0.0f));
            drawEngraved (g, labels[i], tr, font (labelFont, i == current), juce::Justification::centred,
                          *pal, i != current);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().toFloat();
        captionH = juce::jlimit (8.5f, 11.5f, r.getHeight() * 0.13f);
        labelFont = juce::jlimit (8.0f, 10.5f, r.getHeight() * 0.12f);
        labelGap = labelFont + 6.0f;
        r.removeFromTop (captionH + 1.0f);

        // Largest knob whose printed positions still fit: bounding box of the knob plus every label.
        auto extent = [this] (float rad)
        {
            juce::Rectangle<float> box (-rad, -rad, rad * 2.0f, rad * 2.0f);
            for (int i = 0; i < labels.size(); ++i)
            {
                const float a = angleFor (i);
                const juce::Point<float> dir (std::sin (a), -std::cos (a));
                const float tw = juce::GlyphArrangement::getStringWidth (font (labelFont, true), labels[i]) + 2.0f;
                juce::Rectangle<float> tr (tw, labelFont + 2.0f);
                tr.setCentre ((dir * (rad + 4.5f + labelGap * 0.55f)).translated (dir.x * tw * 0.25f, 0.0f));
                box = box.getUnion (tr);
            }
            return box;
        };
        float lo = 8.0f, hi = juce::jmax (8.0f, juce::jmin (r.getWidth(), r.getHeight()) * 0.5f);
        for (int it = 0; it < 18; ++it)
        {
            const float mid = 0.5f * (lo + hi);
            const auto b = extent (mid);
            if (b.getWidth() <= r.getWidth() && b.getHeight() <= r.getHeight()) lo = mid; else hi = mid;
        }
        const auto b = extent (lo);
        const juce::Point<float> c (r.getCentreX() - b.getCentreX(), r.getY() - b.getY() + (r.getHeight() - b.getHeight()) * 0.5f);
        slider.setBounds (juce::Rectangle<float> (lo * 2.0f + 4.0f, lo * 2.0f + 4.0f).withCentre (c).toNearestInt());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Clicking a printed position selects it.
        const auto kb = slider.getBounds().toFloat();
        const float r = kb.getWidth() * 0.5f - 2.0f;
        int best = -1; float bestD = labelFont * 1.6f;
        for (int i = 0; i < labels.size(); ++i)
        {
            const float a = angleFor (i);
            const auto lp = kb.getCentre() + juce::Point<float> (std::sin (a), -std::cos (a)) * (r + 4.5f + labelGap * 0.55f);
            const float d = lp.getDistanceFrom (e.position);
            if (d < bestD) { bestD = d; best = i; }
        }
        if (best >= 0)
            slider.setValue (best, juce::sendNotificationSync);
    }

private:
    float angleFor (int i) const
    {
        const int n = labels.size();
        return n > 1 ? startAngle + (endAngle - startAngle) * (float) i / (float) (n - 1) : 0.0f;
    }

    juce::String caption;
    juce::StringArray labels;
    float startAngle = 0.0f, endAngle = 0.0f, captionH = 11.0f, labelFont = 10.0f, labelGap = 16.0f;
};

//==============================================================================
// FINISH selector: engraved label plus one round swatch per finish, bottom left of the faceplate
// (same layout and behaviour as SoftSpoken). The current finish has an amber ring.
class FinishSwatches : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (int)> onSelect;
    const Palette* pal = &palette (silver);
    int current = silver;

    FinishSwatches() { setTooltip ("Faceplate finish: Silver, Gunmetal Blue, Burnt Orange, Vintage"); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float labelW = labelWidth();
        drawEngraved (g, "FINISH", b.withWidth (labelW), font (fontSize(), true, 0.14f), juce::Justification::centredLeft, *pal);

        for (int i = 0; i < numFinishes; ++i)
        {
            const auto sw = swatch (i);
            const auto& p = palette (i);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillEllipse (sw.translated (0.0f, 1.0f).expanded (1.5f));
            juce::ColourGradient body (p.metal.brighter (0.35f), sw.getX(), sw.getY(), p.metal.darker (0.35f), sw.getX(), sw.getBottom(), false);
            g.setGradientFill (body);
            g.fillEllipse (sw);
            g.setColour (p.ink.withAlpha (0.9f));
            g.fillEllipse (sw.withSizeKeepingCentre (sw.getWidth() * 0.22f, sw.getHeight() * 0.22f)); // lettering colour dot
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawEllipse (sw, 1.0f);
            if (i == current)
            {
                g.setColour (col::ledAmber);
                g.drawEllipse (sw.expanded (3.0f), 2.0f);
            }
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < numFinishes; ++i)
            if (swatch (i).expanded (4.0f).contains (e.position) && onSelect)
                onSelect (i);
    }

    // Natural width for a given height.
    int preferredWidth (int height) const
    {
        const float d = (float) height * 0.66f;
        return juce::roundToInt (labelWidthFor ((float) height) + (float) numFinishes * (d + 9.0f));
    }

private:
    float fontSize() const { return juce::jlimit (8.5f, 11.0f, (float) getHeight() * 0.42f); }
    float labelWidthFor (float h) const
    {
        const float fs = juce::jlimit (8.5f, 11.0f, h * 0.42f);
        return juce::GlyphArrangement::getStringWidth (font (fs, true, 0.14f), "FINISH") + 10.0f;
    }
    float labelWidth() const { return labelWidthFor ((float) getHeight()); }
    juce::Rectangle<float> swatch (int i) const
    {
        const float h = (float) getHeight(), d = h * 0.66f;
        return { labelWidth() + 4.0f + (float) i * (d + 9.0f), (h - d) * 0.5f, d, d };
    }
};

} // namespace faceplate
