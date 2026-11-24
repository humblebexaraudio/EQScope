#include "PluginEditor.h"

namespace colours
{
    static const juce::Colour background { 0xff121519 };
    static const juce::Colour bar        { 0xff181c22 };
    static const juce::Colour plot       { 0xff1a1e25 };
    static const juce::Colour gridMinor  { 0x10ffffff };
    static const juce::Colour gridMajor  { 0x24ffffff };
    static const juce::Colour text       { 0xffaab3bf };
    static const juce::Colour textDim    { 0xff6b7480 };
    static const juce::Colour flat       { 0xb0e8edf2 };
    static const juce::Colour live       { 0xff3fd3e0 };
    static const juce::Colour livePhase  { 0xff9b8cff };
    static const juce::Colour snap       { 0xffffa347 };
    static const juce::Colour ok         { 0xff3ecf7a };
    static const juce::Colour warn       { 0xffffc23d };
    static const juce::Colour bad        { 0xffff5a5a };
}

static juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

static juce::String formatFreq (double f)
{
    if (f >= 1000.0)
        return juce::String (f / 1000.0, f >= 10000.0 ? 1 : 2) + " kHz";
    return juce::String (f, f < 100.0 ? 1 : 0) + " Hz";
}

static juce::String formatDb (float db)
{
    return (db >= 0.0f ? "+" : "") + juce::String (db, 2) + " dB";
}

//==============================================================================
// ResponseView
//==============================================================================
void ResponseView::setLive (const probe::Response& r, const std::vector<float>& mag, const std::vector<float>& phase)
{
    lock = r.lock;
    minimumPhase = r.minimumPhase;
    windowLimited = r.windowLimited;
    levelDb = r.levelDb;
    liveMag = mag;
    livePhase = phase;
}

void ResponseView::clearLive (probe::Lock lockState)
{
    lock = lockState;
    liveMag.clear();
    livePhase.clear();
}

double ResponseView::maxFreq() const
{
    return juce::jmin (20000.0, 0.49 * settings.sampleRate);
}

bool ResponseView::phaseVisible() const
{
    // On very short screens the phase pane steps aside so the response stays readable.
    return settings.showPhase && getHeight() >= 210;
}

juce::Rectangle<float> ResponseView::magArea() const
{
    auto r = getLocalBounds().toFloat().reduced (8.0f);
    r.removeFromLeft (40.0f);
    r.removeFromBottom (20.0f);
    if (phaseVisible())
        r = r.removeFromTop (r.getHeight() * 0.68f);
    return r;
}

juce::Rectangle<float> ResponseView::phaseArea() const
{
    auto r = getLocalBounds().toFloat().reduced (8.0f);
    r.removeFromLeft (40.0f);
    r.removeFromBottom (20.0f);
    r.removeFromTop (r.getHeight() * 0.68f + 10.0f);
    return r;
}

float ResponseView::xForFreq (double f, juce::Rectangle<float> a) const
{
    const double t = std::log (f / 20.0) / std::log (1000.0);
    return a.getX() + (float) t * a.getWidth();
}

double ResponseView::freqForX (float x, juce::Rectangle<float> a) const
{
    const double t = (x - a.getX()) / juce::jmax (1.0f, a.getWidth());
    return 20.0 * std::pow (1000.0, t);
}

void ResponseView::drawFreqGrid (juce::Graphics& g, juce::Rectangle<float> a, bool labels) const
{
    g.setFont (uiFont (11.0f));
    for (double decade = 10.0; decade <= 10000.0; decade *= 10.0)
    {
        for (int m = 1; m <= 9; ++m)
        {
            const double f = decade * m;
            if (f < 20.0 || f > 20000.0) continue;
            const float x = xForFreq (f, a);
            const bool major = (m == 1 || m == 2 || m == 5);
            g.setColour (major ? colours::gridMajor : colours::gridMinor);
            g.drawVerticalLine (juce::roundToInt (x), a.getY(), a.getBottom());

            if (labels && major)
            {
                g.setColour (colours::textDim);
                const juce::String s = f >= 1000.0 ? juce::String ((int) (f / 1000.0)) + "k" : juce::String ((int) f);
                const bool last = f >= 20000.0;
                g.drawText (s, juce::Rectangle<float> (last ? x - 40.0f : x - 20.0f, a.getBottom() + 3.0f, 40.0f, 14.0f),
                            last ? juce::Justification::centredRight : juce::Justification::centred);
            }
        }
    }
}

void ResponseView::drawMagCurve (juce::Graphics& g, juce::Rectangle<float> a, const std::vector<float>& v,
                                 juce::Colour c, float thick) const
{
    if (v.size() < 2) return;
    const float range = settings.rangeDb;
    const float dx = a.getWidth() / (float) (v.size() - 1);
    juce::Path p;
    for (size_t i = 0; i < v.size(); ++i)
    {
        const float db = juce::jlimit (-range * 1.5f, range * 1.5f, v[i]);
        const float y = juce::jmap (db, range, -range, a.getY(), a.getBottom());
        const float x = a.getX() + dx * (float) i;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ResponseView::drawPhaseCurve (juce::Graphics& g, juce::Rectangle<float> a, const std::vector<float>& v,
                                   juce::Colour c, float thick) const
{
    if (v.size() < 2) return;
    const float dx = a.getWidth() / (float) (v.size() - 1);
    juce::Path p;
    for (size_t i = 0; i < v.size(); ++i)
    {
        const float y = juce::jmap (v[i], 180.0f, -180.0f, a.getY(), a.getBottom());
        const float x = a.getX() + dx * (float) i;
        // Break the line where the phase wraps between +180 and -180.
        if (i == 0 || std::abs (v[i] - v[i - 1]) > 180.0f) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ResponseView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    const auto ma = magArea();
    const float range = settings.rangeDb;

    // Display frequencies: one point per pixel across the plot.
    const int n = juce::jmax (2, (int) ma.getWidth());
    if ((int) freqs.size() != n || std::abs (freqs.back() - (float) freqForX (ma.getRight(), ma)) > 1.0f)
    {
        freqs.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
            freqs[(size_t) i] = (float) freqForX (ma.getX() + ma.getWidth() * (float) i / (float) (n - 1), ma);
    }
    std::vector<float> clampedFreqs (freqs);
    for (auto& f : clampedFreqs) f = juce::jmin (f, (float) maxFreq());

    dLiveMag.clear(); dLivePhase.clear(); dSnapMag.clear(); dSnapPhase.clear();
    if (! liveMag.empty())
        probe::sampleCurve (liveMag, &livePhase, settings.sampleRate, clampedFreqs, settings.smoothOct, dLiveMag, &dLivePhase, prefix);
    if (! snap.empty() && ! settings.signalMode)
        probe::sampleCurve (snap.mag, &snap.phase, settings.sampleRate, clampedFreqs, settings.smoothOct, dSnapMag, &dSnapPhase, prefix);

    // --- Magnitude pane
    g.setColour (colours::plot);
    g.fillRoundedRectangle (ma.expanded (1.0f), 3.0f);
    drawFreqGrid (g, ma, ! phaseVisible());

    // Grid step: the range's natural step, coarsened until lines are at least 18 px apart.
    float step = range <= 6.0f ? 2.0f : range <= 12.0f ? 3.0f : range <= 24.0f ? 6.0f : 12.0f;
    while (step < range && ma.getHeight() * step / (2.0f * range) < 18.0f)
        step *= 2.0f;
    g.setFont (uiFont (11.0f));
    for (float db = -range; db <= range + 0.01f; db += step)
    {
        const float y = juce::jmap (db, range, -range, ma.getY(), ma.getBottom());
        g.setColour (colours::gridMajor);
        g.drawHorizontalLine (juce::roundToInt (y), ma.getX(), ma.getRight());
        g.setColour (colours::textDim);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (ma.getX() - 40.0f, y - 7.0f, 34.0f, 14.0f),
                    juce::Justification::centredRight);
    }

    // The flat reference: what the probe signal looks like with nothing in between.
    {
        const float y0 = juce::jmap (0.0f, range, -range, ma.getY(), ma.getBottom());
        const float dashes[] = { 6.0f, 4.0f };
        g.setColour (colours::flat);
        g.drawDashedLine (juce::Line<float> (ma.getX(), y0, ma.getRight(), y0), dashes, 2, 1.2f);
        g.setFont (uiFont (10.5f, true));
        g.drawText (settings.zeroed ? "FLAT (ZEROED)" : "FLAT 0 dB", juce::Rectangle<float> (ma.getRight() - 110.0f, y0 - 16.0f, 104.0f, 14.0f),
                    juce::Justification::centredRight);
    }

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (ma.toNearestInt());
        drawMagCurve (g, ma, dSnapMag, colours::snap.withAlpha (0.85f), 1.6f);
        const float alpha = lock == probe::Lock::Locked ? 1.0f : 0.7f;
        drawMagCurve (g, ma, dLiveMag, colours::live.withAlpha (alpha), 2.2f);
    }

    // --- Phase pane
    if (phaseVisible())
    {
        const auto pa = phaseArea();
        g.setColour (colours::plot);
        g.fillRoundedRectangle (pa.expanded (1.0f), 3.0f);
        drawFreqGrid (g, pa, true);
        g.setFont (uiFont (11.0f));
        const int degStep = pa.getHeight() >= 90.0f ? 90 : 180;
        for (int deg = -180; deg <= 180; deg += degStep)
        {
            const float y = juce::jmap ((float) deg, 180.0f, -180.0f, pa.getY(), pa.getBottom());
            g.setColour (deg == 0 ? colours::gridMajor.withMultipliedAlpha (2.0f) : colours::gridMajor);
            g.drawHorizontalLine (juce::roundToInt (y), pa.getX(), pa.getRight());
            g.setColour (colours::textDim);
            g.drawText (juce::String (deg) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")),
                        juce::Rectangle<float> (pa.getX() - 40.0f, y - 7.0f, 34.0f, 14.0f), juce::Justification::centredRight);
        }
        g.setColour (colours::textDim);
        g.setFont (uiFont (10.5f, true));
        g.drawText (minimumPhase ? "PHASE" : "PHASE (rel. to impulse peak)", pa.reduced (6.0f, 4.0f), juce::Justification::topLeft);

        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (pa.toNearestInt());
        drawPhaseCurve (g, pa, dSnapPhase, colours::snap.withAlpha (0.7f), 1.4f);
        drawPhaseCurve (g, pa, dLivePhase, colours::livePhase.withAlpha (lock == probe::Lock::Locked ? 1.0f : 0.7f), 1.8f);
    }

    drawStatus (g);
    drawReadout (g);
}

void ResponseView::drawStatus (juce::Graphics& g) const
{
    const auto ma = magArea();
    juce::String text, hint;
    juce::Colour c;

    if (settings.hostStalled)
    {
        text = "HOST NOT PROCESSING"; c = colours::textDim;
        hint = "No audio is reaching the plug-in. Start playback, or use a channel that is always live (an Aux in Logic).";
    }
    else if (settings.signalMode)
    {
        if (lock == probe::Lock::NoSignal) { text = "OUTPUT OFF"; c = colours::textDim; }
        else { text = "GENERATING PROBE"; c = colours::ok; }
        hint = settings.fineProbe ? "Fine probe (~680 ms), -18 dBFS RMS. Next: your EQ, then EQScope in ANALYZE mode."
                                  : "Fast probe (~170 ms), -18 dBFS RMS. Next: your EQ, then EQScope in ANALYZE mode.";
    }
    else
    {
        switch (lock)
        {
            case probe::Lock::Locked:   text = "LOCKED";   c = colours::ok;
                if (windowLimited && ! liveMag.empty())
                    hint = "This EQ rings longer than the Fast probe, so the low end is approximate. Press FINE on the SIGNAL copy.";
                break;
            case probe::Lock::Settling: text = "SETTLING"; c = colours::warn; break;
            case probe::Lock::NotProbe: text = "NO PROBE SIGNAL"; c = colours::bad;
                hint = "Insert EQScope in SIGNAL mode before your EQ."; break;
            case probe::Lock::NoSignal: text = "NO SIGNAL"; c = colours::textDim;
                hint = "Insert EQScope in SIGNAL mode before your EQ."; break;
        }
    }

    g.setFont (uiFont (11.5f, true));
    const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 22.0f;
    const juce::Rectangle<float> pill (ma.getX() + 8.0f, ma.getY() + 8.0f, w, 20.0f);
    g.setColour (c.withAlpha (0.16f));
    g.fillRoundedRectangle (pill, 10.0f);
    g.setColour (c);
    g.fillEllipse (pill.getX() + 7.0f, pill.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.drawText (text, pill.withTrimmedLeft (12.0f), juce::Justification::centred);

    float x = pill.getRight() + 10.0f;
    g.setFont (uiFont (11.5f));
    if (! settings.signalMode && ! liveMag.empty())
    {
        g.setColour (colours::text);
        juce::String info = minimumPhase ? "Minimum-phase EQ" : "Linear/mixed-phase EQ";
        info << (settings.fineProbe ? "  |  Fine probe" : "  |  Fast probe");
        if (settings.zeroed) info << "  |  Zeroed";
        g.drawText (info, juce::Rectangle<float> (x, pill.getY(), 300.0f, 20.0f), juce::Justification::centredLeft);
    }
    if (hint.isNotEmpty() && ma.getHeight() >= 110.0f)
    {
        const float hw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), hint) + 16.0f;
        const juce::Rectangle<float> hr (ma.getX() + 8.0f, pill.getBottom() + 5.0f, juce::jmin (hw, ma.getWidth() - 16.0f), 20.0f);
        g.setColour (colours::plot.withAlpha (0.92f));
        g.fillRoundedRectangle (hr, 4.0f);
        g.setColour (colours::text.withAlpha (0.8f));
        g.drawText (hint, hr.withTrimmedLeft (8.0f), juce::Justification::centredLeft);
    }
}

void ResponseView::drawReadout (juce::Graphics& g) const
{
    if (mouse.x < 0.0f || freqs.size() < 2) return;

    const auto ma = magArea();
    const auto pa = phaseVisible() ? phaseArea() : juce::Rectangle<float>();
    if (! ma.contains (mouse) && ! pa.contains (mouse)) return;

    const auto& a = ma;
    const float dx = a.getWidth() / (float) (freqs.size() - 1);
    const int idx = juce::jlimit (0, (int) freqs.size() - 1, juce::roundToInt ((mouse.x - a.getX()) / dx));
    const float x = a.getX() + dx * (float) idx;

    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawVerticalLine (juce::roundToInt (x), ma.getY(), phaseVisible() ? pa.getBottom() : ma.getBottom());

    juce::StringArray lines;
    lines.add (formatFreq (freqs[(size_t) idx]));
    if (! dLiveMag.empty())
    {
        juce::String s = "Live  " + formatDb (dLiveMag[(size_t) idx]);
        if (phaseVisible()) s << "   " << juce::String (dLivePhase[(size_t) idx], 1) << juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
        lines.add (s);
    }
    if (! dSnapMag.empty())
    {
        juce::String s = "Snap  " + formatDb (dSnapMag[(size_t) idx]);
        if (phaseVisible()) s << "   " << juce::String (dSnapPhase[(size_t) idx], 1) << juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
        lines.add (s);
    }

    g.setFont (uiFont (12.0f));
    float w = 0.0f;
    for (auto& l : lines) w = juce::jmax (w, juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), l));
    w += 20.0f;
    const float h = 8.0f + 16.0f * (float) lines.size();
    float bx = x + 12.0f;
    if (bx + w > ma.getRight()) bx = x - 12.0f - w;
    const juce::Rectangle<float> box (bx, juce::jlimit (ma.getY(), getHeight() - h - 4.0f, mouse.y - h - 8.0f), w, h);

    g.setColour (juce::Colour (0xe0101317));
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (colours::gridMajor);
    g.drawRoundedRectangle (box, 4.0f, 1.0f);

    for (int i = 0; i < lines.size(); ++i)
    {
        g.setColour (i == 0 ? juce::Colours::white : (lines[i].startsWith ("Snap") ? colours::snap : colours::live));
        g.drawText (lines[i], juce::Rectangle<float> (box.getX() + 10.0f, box.getY() + 4.0f + 16.0f * (float) i, w - 20.0f, 16.0f),
                    juce::Justification::centredLeft);
    }
}

//==============================================================================
// Editor
//==============================================================================
EQScopeEditor::EQScopeEditor (EQScopeProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setOpaque (true);
    setLookAndFeel (&lnf);
    lnf.setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0202327));
    lnf.setColour (juce::TooltipWindow::textColourId, juce::Colour (0xffe6e8eb));
    lnf.setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff6b7480));

    // Mode buttons (radio)
    signalBtn.onClick  = [this] { setMode (EQScopeProcessor::modeSignal); };
    analyzeBtn.onClick = [this] { setMode (EQScopeProcessor::modeAnalyze); };
    signalBtn.setTooltip ("Generate the flat probe signal. Put this instance BEFORE the EQ.");
    analyzeBtn.setTooltip ("Measure the response. Put this instance AFTER the EQ.");

    // Latching buttons attached to parameters
    for (auto* b : { &outputBtn, &monitorBtn, &phaseBtn })
        b->setClickingTogglesState (true);
    outputAtt  = std::make_unique<BA> (proc.apvts, "signalOn", outputBtn);
    fineBtn.setClickingTogglesState (true);
    fineAtt    = std::make_unique<BA> (proc.apvts, "fine", fineBtn);
    fineBtn.setTooltip ("Off: fast probe (~170 ms), the curve follows the EQ almost instantly. On: fine probe (~680 ms), "
                        "exact even for very narrow low-frequency bands. The ANALYZE copy follows automatically.");
    monitorAtt = std::make_unique<BA> (proc.apvts, "monitor", monitorBtn);
    phaseAtt   = std::make_unique<BA> (proc.apvts, "showPhase", phaseBtn);
    outputBtn.setTooltip ("Turns the probe signal on or off.");
    monitorBtn.setTooltip ("Off: this instance outputs silence so you don't hear the probe. On: passes the audio through.");
    phaseBtn.setTooltip ("Show the phase response under the frequency response.");

    // Selector knobs attached to the choice parameters
    channelAtt = std::make_unique<SA> (proc.apvts, "channel", channelKnob.slider);
    rangeAtt   = std::make_unique<SA> (proc.apvts, "range", rangeKnob.slider);
    smoothAtt  = std::make_unique<SA> (proc.apvts, "smoothing", smoothKnob.slider);
    for (auto* k : { &channelKnob, &rangeKnob, &smoothKnob })
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (
                k == &channelKnob ? "channel" : k == &rangeKnob ? "range" : "smoothing")))
            k->slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    channelKnob.slider.setTooltip ("Which channel to measure: Mid (L+R), Left or Right.");
    rangeKnob.slider.setTooltip ("Vertical scale of the display.");
    smoothKnob.slider.setTooltip ("Fractional-octave smoothing of the curve.");

    // Snapshot / zero
    clearSnapBtn.hasLed = false;
    snapBtn.setTooltip ("Freeze the current curve as an orange overlay to compare against. LED: a snapshot is stored.");
    clearSnapBtn.setTooltip ("Remove the snapshot.");
    zeroBtn.setTooltip ("Treat the current response as flat (cancels anything else in the chain). Press again to remove.");
    snapBtn.onClick = [this]
    {
        if (view.hasLive())
        {
            proc.setSnapshot ({ view.getLiveMag(), view.getLivePhase() });
            view.setSnapshot (proc.getSnapshot());
        }
        syncControls();
    };
    clearSnapBtn.onClick = [this]
    {
        proc.setSnapshot ({});
        view.setSnapshot ({});
        syncControls();
    };
    zeroBtn.onClick = [this]
    {
        if (proc.hasZero())
            proc.setZero ({});
        else if (haveRawLocked)
            proc.setZero ({ rawLocked.magDb, rawLocked.phaseDeg });
        syncControls();
    };

    for (juce::Component* c : { (juce::Component*) &signalBtn, (juce::Component*) &analyzeBtn, (juce::Component*) &outputBtn, (juce::Component*) &fineBtn,
                                (juce::Component*) &monitorBtn, (juce::Component*) &phaseBtn, (juce::Component*) &snapBtn,
                                (juce::Component*) &clearSnapBtn, (juce::Component*) &zeroBtn, (juce::Component*) &channelKnob,
                                (juce::Component*) &rangeKnob, (juce::Component*) &smoothKnob })
        addAndMakeVisible (c);

    addAndMakeVisible (view);
    view.setSnapshot (proc.getSnapshot());

    // Finish selector (bottom left) and the built-in instructions (bottom right)
    finishSwatches.onSelect = [this] (int f) { proc.setFinish (f); applyFinish (f); };
    addAndMakeVisible (finishSwatches);
    instructionsBtn.hasLed = false;
    instructionsBtn.reserveLedSpace = false;
    instructionsBtn.setTooltip ("Short instructions. The full user guide is installed with the plug-in.");
    instructionsBtn.onClick = [this] { instructions.setVisible (true); instructions.toFront (true); };
    addAndMakeVisible (instructionsBtn);
    addChildComponent (instructions);
    applyFinish (proc.getFinish());

   #if JUCE_IOS
    // AUv3 hosts (and the standalone app) decide the size; the faceplate adapts down to phone width.
    setResizable (true, false);
    setResizeLimits (320, 260, 4096, 4096);
    setSize (760, 500);
   #else
    setResizable (true, true);
    setResizeLimits (480, 360, 2400, 1400);
    setSize (1040, 660);
   #endif

    syncControls();
    startTimerHz (60);
}

EQScopeEditor::~EQScopeEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EQScopeEditor::setMode (int mode)
{
    if (auto* param = proc.apvts.getParameter ("mode"))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) mode));
        param->endChangeGesture();
    }
    syncControls();
}

void EQScopeEditor::syncControls()
{
    const bool signalMode = proc.getMode() == EQScopeProcessor::modeSignal;
    signalBtn.setToggleState (signalMode, juce::dontSendNotification);
    analyzeBtn.setToggleState (! signalMode, juce::dontSendNotification);

    const bool layoutChanged = outputBtn.isVisible() != signalMode;
    outputBtn.setVisible (signalMode);
    fineBtn.setVisible (signalMode);
    for (juce::Component* c : { (juce::Component*) &channelKnob, (juce::Component*) &monitorBtn,
                                (juce::Component*) &snapBtn, (juce::Component*) &clearSnapBtn, (juce::Component*) &zeroBtn })
        c->setVisible (! signalMode);

    const bool zeroed = proc.hasZero(), snapped = proc.hasSnapshot();
    zeroBtn.setToggleState (zeroed, juce::dontSendNotification);
    zeroBtn.setEnabled (zeroed || haveRawLocked);
    snapBtn.setToggleState (snapped, juce::dontSendNotification);
    clearSnapBtn.setEnabled (snapped);

    if (layoutChanged)
    {
        resized();
        repaint();
    }
}

void EQScopeEditor::runAnalysis()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const uint64_t w = proc.getWrittenCount();
    if (w != lastSeenWritten)
    {
        lastSeenWritten = w;
        lastStampChangeMs = now;
    }

    const double rate = proc.getAnalysisSampleRate();
    if (fastAnalyzer == nullptr || std::abs (rate - analyzerRate) > 0.5)
    {
        fastAnalyzer = std::make_unique<probe::ResponseAnalyzer> (probe::orderForSampleRate (rate, false));
        fineAnalyzer = std::make_unique<probe::ResponseAnalyzer> (probe::orderForSampleRate (rate, true));
        analyzerRate = rate;
    }

    auto* current = usingFine ? fineAnalyzer.get() : fastAnalyzer.get();
    const int N = current->getPeriod();

    if (w < 2 * (uint64_t) fastAnalyzer->getPeriod())
    {
        view.clearLive (probe::Lock::NoSignal);
        panelLock = probe::Lock::NoSignal;
        haveRawLocked = false;
        lastStamp = w;
        return;
    }

    // Re-analyse every 1/16 of a period (~11 ms with the Fast probe), at most once per frame.
    const uint64_t hop = (uint64_t) juce::jmax (256, N / 16);
    if (w >= lastStamp && w - lastStamp < hop)
        return;

    // Keep the analysis to roughly a fifth of one core at most (only matters for the Fine probe at
    // 96/192 kHz; at 44.1/48 kHz the Fast probe is analysed on every frame).
    if (now - lastFullMs < 4.0 * lastAnalysisCostMs)
        return;

    auto analyse = [this] (probe::ResponseAnalyzer& an) -> bool
    {
        uint64_t stamp = 0;
        if (! proc.copyLatest (samples, 2 * an.getPeriod(), stamp))
            return false;
        lastStamp = stamp;
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        an.analyze (samples.data(), response);
        lastAnalysisCostMs = juce::Time::getMillisecondCounterHiRes() - t0;
        return true;
    };

    // Nothing in the chain has changed since the last analysis (the signal still repeats exactly):
    // the result would be identical, so skip the FFTs. Refresh once a second regardless.
    {
        uint64_t stamp = 0;
        if (! proc.copyLatest (samples, 2 * N, stamp))
            return;
        const bool steady = probe::periodicityDb (samples.data(), N) < -30.0;
        if (steady && lastWasSteady && now - lastFullMs < 1000.0)
        {
            lastStamp = stamp;
            return;
        }
        lastWasSteady = steady;
    }

    lastFullMs = now;
    if (! analyse (*current))
        return;

    const auto good = [] (probe::Lock l) { return l == probe::Lock::Locked || l == probe::Lock::Settling; };
    if (good (response.lock))
        lastGoodMs = now;
    else if (response.lock == probe::Lock::NotProbe)
    {
        // Maybe the SIGNAL copy switched between Fast and Fine: try the other probe length.
        auto* other = usingFine ? fastAnalyzer.get() : fineAnalyzer.get();
        probe::Response saved = response;
        if (w >= 2 * (uint64_t) other->getPeriod() && analyse (*other) && good (response.lock))
        {
            usingFine = ! usingFine;
            lastGoodMs = now;
        }
        else
        {
            response = std::move (saved);
            if (now - lastGoodMs < 1500.0)
            {
                // The probe itself just changed (FINE switched, or a new SIGNAL copy): hold the
                // last curve while the new probe fills the window, instead of flashing NO PROBE.
                panelLock = probe::Lock::Settling;
                return;
            }
        }
    }

    panelLock = response.lock;

    if (good (response.lock))
    {
        if (response.lock == probe::Lock::Locked)
        {
            rawLocked = response;
            haveRawLocked = true;
        }
        mag = response.magDb;
        phase = response.phaseDeg;
        if (proc.getMode() == EQScopeProcessor::modeAnalyze)
        {
            const auto z = proc.getZero();
            if (! z.empty())
                probe::applyZero (mag, phase, z.mag, z.phase);
        }
        view.setLive (response, mag, phase);
    }
    else
    {
        haveRawLocked = false;
        view.clearLive (response.lock);
    }
}

void EQScopeEditor::timerCallback()
{
    const auto prevLock = panelLock;
    const bool prevStalled = panelStalled;

    syncControls();
    runAnalysis();

    if (++snapRefreshTicks >= 30) // pick up snapshots restored from a saved session
    {
        snapRefreshTicks = 0;
        view.setSnapshot (proc.getSnapshot());
    }

    static const float ranges[] = { 6.0f, 12.0f, 24.0f, 48.0f };
    static const double smooths[] = { 0.0, 1.0 / 48.0, 1.0 / 24.0, 1.0 / 12.0, 1.0 / 6.0, 1.0 / 3.0 };

    ResponseView::Settings s;
    s.sampleRate  = proc.getAnalysisSampleRate();
    s.rangeDb     = ranges[juce::jlimit (0, 3, (int) proc.apvts.getRawParameterValue ("range")->load())];
    s.smoothOct   = smooths[juce::jlimit (0, 5, (int) proc.apvts.getRawParameterValue ("smoothing")->load())];
    s.showPhase   = proc.apvts.getRawParameterValue ("showPhase")->load() > 0.5f;
    s.signalMode  = proc.getMode() == EQScopeProcessor::modeSignal;
    s.zeroed      = ! s.signalMode && proc.hasZero();
    s.hostStalled = juce::Time::getMillisecondCounterHiRes() - lastStampChangeMs > 750.0;
    s.fineProbe   = usingFine;
    panelStalled  = s.hostStalled;
    view.setSettings (s);
    view.repaint();

    if (panelLock != prevLock || panelStalled != prevStalled)
        repaint (headerArea);

    if (proc.getFinish() != currentFinish) // a session was loaded with a different finish
        applyFinish (proc.getFinish());
}

juce::Rectangle<int> EQScopeEditor::contentArea() const
{
    auto area = getLocalBounds();
   #if JUCE_IOS
    // The standalone app fills the screen, so keep clear of the notch and home indicator.
    if (juce::JUCEApplicationBase::isStandaloneApp())
        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            area = display->safeAreaInsets.subtractedFrom (area);
   #endif
    return area;
}

//==============================================================================
void EQScopeEditor::applyFinish (int finish)
{
    finish = juce::jlimit (0, faceplate::numFinishes - 1, finish);
    currentFinish = finish;
    pal = &faceplate::palette (finish);
    if (! tiles[(size_t) finish].isValid())
        tiles[(size_t) finish] = faceplate::makeSurfaceTile (*pal);
    lnf.pal = pal;
    for (auto* k : { &channelKnob, &rangeKnob, &smoothKnob })
        k->pal = pal;
    finishSwatches.pal = pal;
    finishSwatches.current = finish;
    repaint();
    for (auto* c : getChildren())
        c->repaint();
}

void EQScopeEditor::showFinishMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Faceplate finish");
    for (int i = 0; i < faceplate::numFinishes; ++i)
        menu.addItem (i + 1, faceplate::palette (i).name, true, i == currentFinish);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                        [safe = juce::Component::SafePointer<EQScopeEditor> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                            {
                                safe->proc.setFinish (result - 1);
                                safe->applyFinish (result - 1);
                            }
                        });
}

void EQScopeEditor::mouseDown (const juce::MouseEvent& e)
{
    // Right-click (or Ctrl-click) anywhere on the faceplate: choose a finish, as in SoftSpoken.
    if (e.mods.isPopupMenu())
        showFinishMenu();
}

void EQScopeEditor::mouseUp (const juce::MouseEvent& e)
{
    // Clicking the EQSCOPE logo gives the same choices.
    if (! e.mods.isPopupMenu() && logoArea.contains (e.position) && e.mouseWasClicked())
        showFinishMenu();
}

void EQScopeEditor::paint (juce::Graphics& g)
{
    using namespace faceplate;
    const auto full = getLocalBounds().toFloat();
    drawPlate (g, full, tiles[(size_t) currentFinish], *pal);

    // Corner screws
    const float inset = compact ? 10.0f : 13.0f, sr = compact ? 4.3f : 5.6f;
    drawScrew (g, { inset, inset }, sr, 0.35f, *pal);
    drawScrew (g, { full.getRight() - inset, inset }, sr, 1.10f, *pal);
    drawScrew (g, { inset, full.getBottom() - inset }, sr, 0.80f, *pal);
    drawScrew (g, { full.getRight() - inset, full.getBottom() - inset }, sr, 0.15f, *pal);

    drawHeader (g);
    drawScreenBezel (g, view.getBounds().toFloat(), *pal);

    // Engraved rule above the controls
    const float y = (float) controlsArea.getY() - 4.0f;
    g.setColour (pal->ink.withAlpha (0.35f));
    g.drawHorizontalLine ((int) y, (float) controlsArea.getX(), (float) controlsArea.getRight());
    g.setColour (pal->engraveEdge);
    g.drawHorizontalLine ((int) y + 1, (float) controlsArea.getX(), (float) controlsArea.getRight());
}

// Header: EQSCOPE logo on the left, HUMBLE BEXAR AUDIO at the top right, status lights between them.
// On narrow screens the status lights drop to a second line under the maker's name.
void EQScopeEditor::drawHeader (juce::Graphics& g) const
{
    using namespace faceplate;
    auto h = headerArea.toFloat();

    const auto titleFont = font (compact ? 17.0f : 22.0f, true, 0.10f);
    const auto makerFont = font (compact ? 8.5f : 10.0f, true, 0.22f);
    const auto lf = font (compact ? 9.0f : 10.0f, true, 0.08f);
    const float ledR = compact ? 3.6f : 4.4f;
    const float titleW = juce::GlyphArrangement::getStringWidth (titleFont, "EQSCOPE") + 4.0f;
    const float makerW = juce::GlyphArrangement::getStringWidth (makerFont, "HUMBLE BEXAR AUDIO") + 4.0f;

    struct Light { const char* label; juce::Colour colour; bool on; };
    const bool live = ! panelStalled;
    const Light lights[] = {
        { "LOCKED",   col::ledGreen, live && panelLock == probe::Lock::Locked },
        { "SETTLING", col::ledAmber, live && panelLock == probe::Lock::Settling },
        { "NO PROBE", col::ledRed,   live && panelLock == probe::Lock::NotProbe },
    };
    float lightsW = 0.0f;
    for (auto& l : lights)
        lightsW += ledR * 2.0f + 6.0f + juce::GlyphArrangement::getStringWidth (lf, l.label) + 2.0f + (compact ? 10.0f : 16.0f);

    const bool oneLine = titleW + lightsW + makerW + 40.0f <= h.getWidth();
    auto lightsRow = h;
    if (oneLine)
    {
        logoArea = h.withWidth (titleW);
        drawEngraved (g, "EQSCOPE", logoArea, titleFont, juce::Justification::centredLeft, *pal);
        auto right = h.removeFromRight (makerW);
        drawEngraved (g, "HUMBLE BEXAR AUDIO", right, makerFont, juce::Justification::centredRight, *pal, true);
        lightsRow = h.withTrimmedRight (24.0f);
    }
    else
    {
        auto top = h.removeFromTop (h.getHeight() * 0.55f);
        logoArea = top.withWidth (titleW);
        drawEngraved (g, "EQSCOPE", logoArea, titleFont, juce::Justification::centredLeft, *pal);
        drawEngraved (g, "HUMBLE BEXAR AUDIO", top, makerFont, juce::Justification::centredRight, *pal, true);
        lightsRow = h;
    }

    // Status lights, right-aligned in their row
    float x = lightsRow.getRight();
    for (int i = 2; i >= 0; --i)
    {
        const float tw = juce::GlyphArrangement::getStringWidth (lf, lights[i].label) + 2.0f;
        x -= tw;
        drawEngraved (g, lights[i].label, { x, lightsRow.getY(), tw, lightsRow.getHeight() }, lf, juce::Justification::centredLeft, *pal);
        x -= ledR * 2.0f + 6.0f;
        drawLed (g, { x + ledR, lightsRow.getCentreY() }, ledR, lights[i].colour, lights[i].on ? 1.0f : 0.0f);
        x -= compact ? 10.0f : 16.0f;
    }
}
// The controls are groups that wrap onto more rows when the faceplate is narrow
// (an iPhone in portrait needs three). Within a row, controls shrink towards a minimum width.
void EQScopeEditor::resized()
{
    auto area = contentArea();
    compact = area.getHeight() < 470 || area.getWidth() < 600;
    const float k = compact ? 0.85f : 1.0f;
    const int side = compact ? 22 : 28;     // keeps the header and controls clear of the corner screws
    const int rowH = compact ? 70 : 92, rowGap = 6, groupGap = juce::roundToInt (16 * k);
    const int buttonH = compact ? 44 : 54;

    struct Item  { juce::Component* c; int w, minW, gapAfter; bool knob; };
    struct Group { std::vector<Item> items; bool rightAligned; };
    auto sc = [k] (int v) { return juce::roundToInt ((float) v * k); };

    const bool signalMode = outputBtn.isVisible();
    std::vector<Group> groups;
    groups.push_back ({ { { &signalBtn, sc (78), sc (66), 6, false }, { &analyzeBtn, sc (86), sc (72), 0, false } }, false });
    if (signalMode)
        groups.push_back ({ { { &outputBtn, sc (80), sc (68), 6, false }, { &fineBtn, sc (70), sc (60), 0, false } }, false });
    else
        groups.push_back ({ { { &channelKnob, sc (92), sc (78), 8, true }, { &monitorBtn, sc (84), sc (70), 0, false } }, false });
    groups.push_back ({ { { &rangeKnob, sc (100), sc (82), 8, true }, { &smoothKnob, sc (124), sc (100), 8, true },
                          { &phaseBtn, sc (74), sc (62), 0, false } }, true });
    if (! signalMode)
        groups.push_back ({ { { &snapBtn, sc (92), sc (76), 6, false }, { &clearSnapBtn, sc (70), sc (58), 6, false },
                              { &zeroBtn, sc (70), sc (58), 0, false } }, true });

    auto groupWidth = [] (const Group& g, bool minimum)
    {
        int w = 0;
        for (auto& i : g.items) w += (minimum ? i.minW : i.w) + i.gapAfter;
        return w;
    };

    const int avail = area.getWidth() - 2 * side;
    std::vector<std::vector<const Group*>> rows (1);
    int rowMin = 0;
    for (auto& g : groups)
    {
        const int gm = groupWidth (g, true);
        if (! rows.back().empty() && rowMin + groupGap + gm > avail)
        {
            rows.emplace_back();
            rowMin = 0;
        }
        rowMin += (rows.back().empty() ? 0 : groupGap) + gm;
        rows.back().push_back (&g);
    }

    // Header across the top; the FINISH swatches and ? INSTRUCTIONS along the very bottom;
    // the controls above them; the screen in between.
    const bool narrowHeader = area.getWidth() < 640;
    headerArea = area.removeFromTop (narrowHeader ? 52 : (compact ? 40 : 50)).reduced (side, 0).withTrimmedTop (compact ? 2 : 4);

    const int footerH = compact ? 20 : 24;
    auto footer = area.removeFromBottom (footerH + (compact ? 6 : 8)).withTrimmedBottom (compact ? 6 : 8).reduced (side, 0);
    instructionsBtn.setBounds (footer.removeFromRight (compact ? 104 : 124));
    footer.removeFromRight (8);
    finishSwatches.setBounds (footer.removeFromLeft (juce::jmin (footer.getWidth(), finishSwatches.preferredWidth (footerH))));
    instructions.setBounds (getLocalBounds());

    const int controlsH = (int) rows.size() * rowH + ((int) rows.size() - 1) * rowGap;
    area.removeFromBottom (compact ? 6 : 8);
    controlsArea = area.removeFromBottom (controlsH).reduced (side, 0);
    area.removeFromBottom (compact ? 12 : 16);
    view.setBounds (area.reduced (side - 6, compact ? 8 : 10));

    int y = controlsArea.getY();
    for (auto& row : rows)
    {
        int natural = 0, minimum = 0, gaps = 0;
        for (size_t gi = 0; gi < row.size(); ++gi)
        {
            natural += groupWidth (*row[gi], false);
            minimum += groupWidth (*row[gi], true);
            if (gi > 0) gaps += groupGap;
        }
        const int space = avail - gaps;
        const double t = natural <= space ? 1.0
                       : juce::jlimit (0.0, 1.0, (double) (space - minimum) / juce::jmax (1, natural - minimum));

        int used = gaps;
        for (auto* g : row)
            for (auto& i : g->items)
                used += i.minW + juce::roundToInt ((i.w - i.minW) * t) + i.gapAfter;
        const int spare = juce::jmax (0, avail - used);

        int x = controlsArea.getX();
        bool pushedRight = false;
        for (size_t gi = 0; gi < row.size(); ++gi)
        {
            if (gi > 0) x += groupGap;
            if (row[gi]->rightAligned && ! pushedRight && gi > 0 && ! row[gi - 1]->rightAligned)
            {
                x += spare; // on a single wide row, display/actions sit on the right
                pushedRight = true;
            }
            for (auto& i : row[gi]->items)
            {
                const int w = i.minW + juce::roundToInt ((i.w - i.minW) * t);
                if (i.knob)
                    i.c->setBounds (x, y, w, rowH);
                else
                    i.c->setBounds (x, y + rowH - buttonH - (compact ? 2 : 4), w, buttonH);
                x += w + i.gapAfter;
            }
        }
        y += rowH + rowGap;
    }
}
