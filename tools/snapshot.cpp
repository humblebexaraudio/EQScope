// Renders the faceplate screenshots in docs/images from the real editor code.
//   cmake -B build -DEQSCOPE_SNAPSHOT=ON && cmake --build build --target eqscope_snapshot
//   ./build/eqscope_snapshot_artefacts/Release/eqscope_snapshot docs/images      (Linux: run under xvfb-run)
// It plays the probe through a simulated 3-band EQ into a SIGNAL and an ANALYZE instance, keeps a
// snapshot of a +6 dB bell, then renders each finish, the SIGNAL view and an iPhone-sized view.

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <atomic>
#include <thread>

using namespace juce;

namespace
{
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] {}, z2[2] {};
    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
    {
        b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
    }
    void peak (double fs, double f, double g, double q)
    {
        const double A = std::pow (10, g / 40), w = 2 * MathConstants<double>::pi * f / fs, al = std::sin (w) / (2 * q), c = std::cos (w);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void shelf (double fs, double f, double g, bool high)
    {
        const double A = std::pow (10, g / 40), w = 2 * MathConstants<double>::pi * f / fs, c = std::cos (w);
        const double s = 2 * std::sqrt (A) * std::sin (w) / 2 * std::sqrt (2.0);
        if (high) set (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
                       (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
        else      set (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
                       (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
    }
    float process (int ch, float x)
    {
        const double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return (float) y;
    }
};

void setParam (EQScopeProcessor& p, const String& id, float v)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (v));
}

void save (const Image& img, const File& f)
{
    f.deleteFile();
    FileOutputStream os (f);
    PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
}

void pump (int ms) { MessageManager::getInstance()->runDispatchLoopUntil (ms); }
} // namespace

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File out = File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "docs/images");
    out.createDirectory();

    const double fs = 48000;
    const int bs = 512;
    EQScopeProcessor gen, ana;
    for (auto* p : { &gen, &ana })
    {
        p->setPlayConfigDetails (2, 2, fs, bs);
        p->prepareToPlay (fs, bs);
        p->apvts.state.setProperty ("finish", 0, nullptr); // don't touch the user's saved default
    }
    setParam (gen, "mode", 0);
    setParam (ana, "mode", 1);

    std::atomic<int> preset { 0 };
    std::atomic<bool> running { true };
    std::thread audio ([&]
    {
        Biquad eq[3];
        int current = -1;
        AudioBuffer<float> buf (2, bs);
        MidiBuffer midi;
        while (running)
        {
            if (preset != current)
            {
                current = preset;
                for (auto& e : eq) e = Biquad();
                if (current == 0) eq[0].peak (fs, 1000, 6, 1);
                else { eq[0].shelf (fs, 150, 4, false); eq[1].peak (fs, 2500, -7, 2.5); eq[2].shelf (fs, 9000, -5, true); }
            }
            buf.clear();
            gen.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                for (int i = 0; i < bs; ++i)
                    d[i] = eq[2].process (ch, eq[1].process (ch, eq[0].process (ch, d[i])));
            }
            ana.processBlock (buf, midi);
            std::this_thread::sleep_for (std::chrono::microseconds (10600)); // about real time
        }
    });

    std::unique_ptr<AudioProcessorEditor> ed (ana.createEditor()), sig (gen.createEditor());
    ed->setSize (1040, 660);
    sig->setSize (1040, 660);
    pump (1500);

    // Keep the +6 dB bell as the orange snapshot, then switch the EQ to the 3-band curve.
    {
        probe::ResponseAnalyzer an (probe::orderForSampleRate (fs));
        std::vector<float> s;
        uint64_t stamp = 0;
        probe::Response r;
        if (ana.copyLatest (s, 2 * an.getPeriod(), stamp))
        {
            an.analyze (s.data(), r);
            ana.setSnapshot ({ r.magDb, r.phaseDeg });
        }
    }
    preset = 1;
    pump (1500);

    // Hover readout at 2.5 kHz
    for (auto* c : ed->getChildren())
        if (auto* v = dynamic_cast<ResponseView*> (c))
        {
            const float x = 8 + 40 + (float) (std::log (2500.0 / 20) / std::log (1000.0)) * ((float) v->getWidth() - 16 - 40);
            const MouseEvent e (Desktop::getInstance().getMainMouseSource(), { x, 150.0f }, {}, 0, 0, 0, 0, 0, v, v,
                                Time::getCurrentTime(), { x, 150.0f }, Time::getCurrentTime(), 0, false);
            v->mouseMove (e);
        }

    const char* names[] = { "silver", "gunmetal-blue", "burnt-orange", "vintage" };
    for (int f = 0; f < 4; ++f)
    {
        ana.apvts.state.setProperty ("finish", f, nullptr);
        pump (250);
        save (ed->createComponentSnapshot (ed->getLocalBounds()), out.getChildFile (String (names[f]) + ".png"));
    }

    // Silver details for the guide: control strip, status lights, SIGNAL view, iPhone portrait
    ana.apvts.state.setProperty ("finish", 0, nullptr);
    pump (250);
    const auto full = ed->createComponentSnapshot (ed->getLocalBounds());
    save (full.getClippedImage ({ 0, 530, 1040, 130 }), out.getChildFile ("controls.png"));
    save (full.getClippedImage ({ 560, 8, 456, 40 }), out.getChildFile ("header-right.png"));
    save (sig->createComponentSnapshot (sig->getLocalBounds()), out.getChildFile ("signal.png"));

    // The built-in instructions, opened from the faceplate button
    for (auto* c : ed->getChildren())
        if (auto* b = dynamic_cast<Button*> (c); b != nullptr && b->getButtonText().contains ("INSTRUCTIONS"))
            b->triggerClick();
    pump (300);
    save (ed->createComponentSnapshot (ed->getLocalBounds()), out.getChildFile ("instructions.png"));
    for (auto* c : ed->getChildren())
        if (dynamic_cast<InstructionsOverlay*> (c) != nullptr)
            c->setVisible (false);
    ed->setSize (390, 760);
    pump (400);
    save (ed->createComponentSnapshot (ed->getLocalBounds()), out.getChildFile ("iphone.png"));

    running = false;
    audio.join();
    ed.reset();
    sig.reset();
    return 0;
}
