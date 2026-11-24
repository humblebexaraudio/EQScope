#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    static const juce::String mode     = "mode";
    static const juce::String signalOn = "signalOn";
    static const juce::String fine     = "fine";
    static const juce::String channel  = "channel";
    static const juce::String monitor  = "monitor";
    static const juce::String range    = "range";
    static const juce::String smooth   = "smoothing";
    static const juce::String phase    = "showPhase";
}

EQScopeProcessor::EQScopeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "EQScope", createLayout()),
      ring ((size_t) ringSize, 0.0f)
{
    modeParam     = apvts.getRawParameterValue (ids::mode);
    signalOnParam = apvts.getRawParameterValue (ids::signalOn);
    fineParam     = apvts.getRawParameterValue (ids::fine);
    channelParam  = apvts.getRawParameterValue (ids::channel);
    monitorParam  = apvts.getRawParameterValue (ids::monitor);
    apvts.state.setProperty ("finish", loadDefaultFinish(), nullptr);
}

//==============================================================================
static juce::PropertiesFile::Options settingsOptions()
{
    // ~/Library/Application Support/Humble Bexar Audio/EQScope.settings on macOS,
    // %APPDATA%\Humble Bexar Audio\EQScope.settings on Windows.
    juce::PropertiesFile::Options o;
    o.applicationName = "EQScope";
    o.filenameSuffix = ".settings";
    o.folderName = "Humble Bexar Audio";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    return o;
}

int EQScopeProcessor::loadDefaultFinish()
{
    juce::PropertiesFile props (settingsOptions());
    return juce::jlimit (0, kNumFinishes - 1, props.getIntValue ("defaultFinish", 0));
}

void EQScopeProcessor::saveDefaultFinish (int finish)
{
    juce::PropertiesFile props (settingsOptions());
    props.setValue ("defaultFinish", juce::jlimit (0, kNumFinishes - 1, finish));
    props.saveIfNeeded();
}

int EQScopeProcessor::getFinish() const
{
    return juce::jlimit (0, kNumFinishes - 1, (int) apvts.state.getProperty ("finish", 0));
}

void EQScopeProcessor::setFinish (int finish)
{
    finish = juce::jlimit (0, kNumFinishes - 1, finish);
    apvts.state.setProperty ("finish", finish, nullptr);
    saveDefaultFinish (finish);
}

juce::AudioProcessorValueTreeState::ParameterLayout EQScopeProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::mode, 1 }, "Mode",
                                                   StringArray { "Signal", "Analyze" }, 1));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ids::signalOn, 1 }, "Signal Output", true));
    // Off: ~170 ms probe, the curve follows the EQ almost instantly. On: ~680 ms probe, exact even for
    // very narrow low-frequency bands. The analyzer follows automatically.
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ids::fine, 1 }, "Fine Resolution", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::channel, 1 }, "Channel",
                                                   StringArray { "Mid (L+R)", "Left", "Right" }, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ids::monitor, 1 }, "Monitor", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::range, 1 }, "Range",
                                                   StringArray { "+/-6 dB", "+/-12 dB", "+/-24 dB", "+/-48 dB" }, 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::smooth, 1 }, "Smoothing",
                                                   StringArray { "No smoothing", "1/48 oct", "1/24 oct", "1/12 oct", "1/6 oct", "1/3 oct" }, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ids::phase, 1 }, "Show Phase", true));
    return l;
}

bool EQScopeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return in == out;
}

void EQScopeProcessor::prepareToPlay (double sampleRate, int)
{
    currentRate.store (sampleRate);
    generator.setOrder (probe::orderForSampleRate (sampleRate, fineParam->load() > 0.5f));
    std::fill (ring.begin(), ring.end(), 0.0f);
    written.store (0, std::memory_order_release);
}

void EQScopeProcessor::record (const float* data, int n)
{
    const uint64_t w = written.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
        ring[(size_t) ((w + (uint64_t) i) & (uint64_t) ringMask)] = data[i];
    written.store (w + (uint64_t) n, std::memory_order_release);
}

void EQScopeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    if (n == 0 || buffer.getNumChannels() == 0)
        return;

    const int mode = getMode();
    if (mode != lastMode)
    {
        // Don't let the other mode's audio count as part of a period.
        generator.reset();
        lastMode = mode;
    }

    if (mode == modeSignal)
    {
        const int order = probe::orderForSampleRate (currentRate.load(), fineParam->load() > 0.5f);
        if (order != generator.getOrder())
            generator.setOrder (order); // the tables are prebuilt, so this doesn't allocate
        float* out0 = buffer.getWritePointer (0);
        if (signalOnParam->load() > 0.5f)
            generator.render (out0, n);
        else
            buffer.clear (0, 0, n);

        for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, n);

        record (out0, n);
        return;
    }

    // Analyze: record the chosen channel, then pass audio through or mute it.
    const int chSel = (int) channelParam->load();
    const bool stereo = numIn >= 2;
    const float* l = buffer.getReadPointer (0);
    const float* r = stereo ? buffer.getReadPointer (1) : l;

    if (! stereo || chSel == chLeft)
        record (l, n);
    else if (chSel == chRight)
        record (r, n);
    else
    {
        float scratch[512];
        for (int start = 0; start < n; start += 512)
        {
            const int len = juce::jmin (512, n - start);
            for (int i = 0; i < len; ++i)
                scratch[i] = 0.5f * (l[start + i] + r[start + i]);
            record (scratch, len);
        }
    }

    if (monitorParam->load() < 0.5f)
        buffer.clear();
}

bool EQScopeProcessor::copyLatest (std::vector<float>& dest, int count, uint64_t& stamp) const
{
    const uint64_t w = written.load (std::memory_order_acquire);
    const uint64_t need = (uint64_t) juce::jlimit (1, ringSize / 2, count);
    if (w < need)
        return false;

    dest.resize ((size_t) need);
    const uint64_t start = w - need;
    for (uint64_t i = 0; i < need; ++i)
        dest[(size_t) i] = ring[(size_t) ((start + i) & (uint64_t) ringMask)];
    stamp = w;
    return true;
}

EQScopeProcessor::Curve EQScopeProcessor::getZero() const      { const juce::ScopedLock sl (curveLock); return zero; }
void EQScopeProcessor::setZero (Curve c)                        { const juce::ScopedLock sl (curveLock); zero = std::move (c); }
EQScopeProcessor::Curve EQScopeProcessor::getSnapshot() const  { const juce::ScopedLock sl (curveLock); return snapshot; }
void EQScopeProcessor::setSnapshot (Curve c)                    { const juce::ScopedLock sl (curveLock); snapshot = std::move (c); }
bool EQScopeProcessor::hasZero() const                          { const juce::ScopedLock sl (curveLock); return ! zero.empty(); }
bool EQScopeProcessor::hasSnapshot() const                      { const juce::ScopedLock sl (curveLock); return ! snapshot.empty(); }

//==============================================================================
static juce::String floatsToBase64 (const std::vector<float>& v)
{
    juce::MemoryBlock mb (v.data(), v.size() * sizeof (float));
    return mb.toBase64Encoding();
}

static std::vector<float> base64ToFloats (const juce::String& s)
{
    juce::MemoryBlock mb;
    // Curves are stored on the response grid of the session that saved them (16385 or more points).
    if (s.isEmpty() || ! mb.fromBase64Encoding (s) || mb.getSize() < sizeof (float) * 3 || mb.getSize() % sizeof (float) != 0)
        return {};
    std::vector<float> v (mb.getSize() / sizeof (float));
    std::memcpy (v.data(), mb.getData(), mb.getSize());
    return v;
}

void EQScopeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree curves ("Curves");
    {
        const juce::ScopedLock sl (curveLock);
        curves.setProperty ("zeroMag",   floatsToBase64 (zero.mag), nullptr);
        curves.setProperty ("zeroPhase", floatsToBase64 (zero.phase), nullptr);
        curves.setProperty ("snapMag",   floatsToBase64 (snapshot.mag), nullptr);
        curves.setProperty ("snapPhase", floatsToBase64 (snapshot.phase), nullptr);
    }
    state.appendChild (curves, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void EQScopeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (state.hasType ("EQProbe"))
    {
        // Sessions saved with EQ Probe (EQScope's name before 1.0) load unchanged.
        juce::ValueTree renamed (apvts.state.getType());
        renamed.copyPropertiesAndChildrenFrom (state, nullptr);
        state = renamed;
    }
    if (! state.hasType (apvts.state.getType()))
        return;

    auto curves = state.getChildWithName ("Curves");
    if (curves.isValid())
    {
        Curve z { base64ToFloats (curves["zeroMag"].toString()), base64ToFloats (curves["zeroPhase"].toString()) };
        Curve s { base64ToFloats (curves["snapMag"].toString()), base64ToFloats (curves["snapPhase"].toString()) };
        if (z.mag.empty() || z.mag.size() != z.phase.size()) z = {};
        if (s.mag.empty() || s.mag.size() != s.phase.size()) s = {};
        setZero (std::move (z));
        setSnapshot (std::move (s));
        state.removeChild (curves, nullptr);
    }

    const bool hasFinish = state.hasProperty ("finish");
    apvts.replaceState (state);
    if (! hasFinish) // sessions from before finishes existed
        apvts.state.setProperty ("finish", loadDefaultFinish(), nullptr);
}

juce::AudioProcessorEditor* EQScopeProcessor::createEditor()
{
    return new EQScopeEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQScopeProcessor();
}
