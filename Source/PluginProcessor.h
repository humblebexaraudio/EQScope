#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "EQScopeDSP.h"
#include <atomic>
#include <limits>

class EQScopeProcessor : public juce::AudioProcessor
{
public:
    enum Mode { modeSignal = 0, modeAnalyze = 1 };
    enum Channel { chMid = 0, chLeft = 1, chRight = 2 };

    EQScopeProcessor();
    ~EQScopeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // Endless tail: the SIGNAL copy makes sound from a silent input, so hosts must never pause it
    // (Logic and others stop calling plug-ins on a silent channel once their tail has run out).
    double getTailLengthSeconds() const override { return std::numeric_limits<double>::infinity(); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    int getMode() const { return (int) modeParam->load(); }
    double getAnalysisSampleRate() const { return currentRate.load(); }

    // Message thread: copies the 'count' most recent recorded samples (oldest first).
    // Returns false until enough audio has been recorded. 'stamp' is the total sample count.
    bool copyLatest (std::vector<float>& dest, int count, uint64_t& stamp) const;
    uint64_t getWrittenCount() const { return written.load (std::memory_order_acquire); }

    // Per-bin curves (calibration "zero" and the A/B snapshot). Message thread.
    struct Curve
    {
        std::vector<float> mag, phase;
        bool empty() const { return mag.empty(); }
    };
    Curve getZero() const;
    void setZero (Curve c);
    Curve getSnapshot() const;
    bool hasZero() const;
    bool hasSnapshot() const;

    // Faceplate finish (0 Silver, 1 Gunmetal Blue, 2 Burnt Orange, 3 Vintage, 4 Glacier Blue, 5 Pecan,
    // 6 Polished Enamel Cream, 7 Polished Enamel Black). Saved with the session;
    // the last one picked also becomes the default for new instances. Message thread.
    static constexpr int kNumFinishes = 8;
    int getFinish() const;
    void setFinish (int finish);
    static int loadDefaultFinish();
    static void saveDefaultFinish (int finish);
    void setSnapshot (Curve c);

private:
    // Holds two periods of the longest probe (Fine at 192 kHz) with room to spare.
    static constexpr int ringSize = (1 << probe::kMaxOrder) * 4;
    static constexpr int ringMask = ringSize - 1;

    void record (const float* data, int n);

    probe::ProbeGenerator generator;
    std::vector<float> ring;
    std::atomic<uint64_t> written { 0 };
    std::atomic<double> currentRate { 48000.0 };

    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* signalOnParam = nullptr;
    std::atomic<float>* fineParam = nullptr;
    std::atomic<float>* channelParam = nullptr;
    std::atomic<float>* monitorParam = nullptr;
    int lastMode = -1;

    mutable juce::CriticalSection curveLock;
    Curve zero, snapshot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQScopeProcessor)
};
