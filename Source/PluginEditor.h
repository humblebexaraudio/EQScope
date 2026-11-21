#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "Faceplate.h"
#include "Instructions.h"

//==============================================================================
class ResponseView : public juce::Component
{
public:
    struct Settings
    {
        double sampleRate = 48000.0;
        float rangeDb = 24.0f;
        double smoothOct = 0.0;
        bool showPhase = true;
        bool signalMode = false;
        bool zeroed = false;
        bool hostStalled = false;
        bool fineProbe = false;
    };

    void setSettings (const Settings& s) { settings = s; }
    void setLive (const probe::Response& r, const std::vector<float>& mag, const std::vector<float>& phase);
    void clearLive (probe::Lock lockState);
    void setSnapshot (const EQScopeProcessor::Curve& c) { snap = c; }

    const std::vector<float>& getLiveMag() const   { return liveMag; }
    const std::vector<float>& getLivePhase() const { return livePhase; }
    bool hasLive() const { return ! liveMag.empty(); }

    void paint (juce::Graphics&) override;
    // Desktop: hover shows the readout. Touch screens: tap or drag on the graph, and it stays put.
    void mouseMove (const juce::MouseEvent& e) override { mouse = e.position; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { mouse = e.position; repaint(); }
    void mouseDrag (const juce::MouseEvent& e) override { mouse = e.position; repaint(); }
    void mouseExit (const juce::MouseEvent& e) override { if (! e.source.isTouch()) { mouse = { -1.0f, -1.0f }; repaint(); } }

private:
    bool phaseVisible() const;
    juce::Rectangle<float> magArea() const;
    juce::Rectangle<float> phaseArea() const;
    float xForFreq (double f, juce::Rectangle<float> a) const;
    double freqForX (float x, juce::Rectangle<float> a) const;
    double maxFreq() const;

    void drawFreqGrid (juce::Graphics&, juce::Rectangle<float> a, bool labels) const;
    void drawMagCurve (juce::Graphics&, juce::Rectangle<float> a, const std::vector<float>& v, juce::Colour c, float thick) const;
    void drawPhaseCurve (juce::Graphics&, juce::Rectangle<float> a, const std::vector<float>& v, juce::Colour c, float thick) const;
    void drawStatus (juce::Graphics&) const;
    void drawReadout (juce::Graphics&) const;

    Settings settings;
    probe::Lock lock = probe::Lock::NoSignal;
    bool minimumPhase = true;
    bool windowLimited = false;
    double levelDb = -200.0;

    std::vector<float> liveMag, livePhase; // per bin
    EQScopeProcessor::Curve snap;

    // Per-pixel display buffers (rebuilt in paint)
    mutable std::vector<float> freqs, dLiveMag, dLivePhase, dSnapMag, dSnapPhase;
    mutable std::vector<double> prefix;

    juce::Point<float> mouse { -1.0f, -1.0f };
};

//==============================================================================
class EQScopeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EQScopeEditor (EQScopeProcessor&);
    ~EQScopeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void applyFinish (int finish);
    void showFinishMenu();
    void timerCallback() override;
    void setMode (int mode);
    void syncControls();
    void runAnalysis();
    juce::Rectangle<int> contentArea() const;
    void drawHeader (juce::Graphics&) const;

    EQScopeProcessor& proc;
    faceplate::PanelLookAndFeel lnf;
    std::array<juce::Image, faceplate::numFinishes> tiles; // brushed texture per finish, made on first use
    int currentFinish = -1;
    const faceplate::Palette* pal = &faceplate::palette (faceplate::silver);
    faceplate::FinishSwatches finishSwatches;
    faceplate::LedButton instructionsBtn { "? INSTRUCTIONS" };
    InstructionsOverlay instructions;

    faceplate::LedButton signalBtn { "SIGNAL" }, analyzeBtn { "ANALYZE" },
                         outputBtn { "OUTPUT", faceplate::col::ledGreen }, fineBtn { "FINE" },
                         monitorBtn { "MONITOR" }, phaseBtn { "PHASE" },
                         snapBtn { "SNAPSHOT" }, clearSnapBtn { "CLEAR" }, zeroBtn { "ZERO" };
    faceplate::SelectorKnob channelKnob { "CHANNEL", { "MID", "L", "R" } },
                            rangeKnob { juce::String (juce::CharPointer_UTF8 ("RANGE \xc2\xb1" "dB")), { "6", "12", "24", "48" } },
                            smoothKnob { "SMOOTH", { "OFF", "1/48", "1/24", "1/12", "1/6", "1/3" } };

    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<BA> outputAtt, fineAtt, monitorAtt, phaseAtt;
    std::unique_ptr<SA> channelAtt, rangeAtt, smoothAtt;

    ResponseView view;
    juce::TooltipWindow tooltipWindow { this, 600 };

    // One analyzer per probe length; the editor detects which probe is arriving.
    std::unique_ptr<probe::ResponseAnalyzer> fastAnalyzer, fineAnalyzer;
    double analyzerRate = 0.0;
    bool usingFine = false;
    double lastGoodMs = -1.0e9;
    double lastFullMs = -1.0e9, lastAnalysisCostMs = 0.0;
    bool lastWasSteady = false;
    probe::Response response, rawLocked;
    std::vector<float> samples, mag, phase;
    uint64_t lastStamp = 0, lastSeenWritten = 0;
    int snapRefreshTicks = 0;
    double lastStampChangeMs = 0.0;
    bool haveRawLocked = false;

    // Faceplate state
    probe::Lock panelLock = probe::Lock::NoSignal;
    bool panelStalled = false, compact = false;
    juce::Rectangle<int> headerArea, controlsArea;
    mutable juce::Rectangle<float> logoArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQScopeEditor)
};
