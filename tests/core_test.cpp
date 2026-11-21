// DSP tests for EQScope. Build: see README ("Tests").
#include "EQScopeDSP.h"
#include <cstdio>
#include <functional>
#include <random>

using namespace probe;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("  FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

struct Biquad {
    double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
    float process(float xf) {
        const double x = xf, y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
    cplx response(double w) const {
        const cplx z1c = std::polar(1.0, -w), z2c = std::polar(1.0, -2 * w);
        return (b0 + b1 * z1c + b2 * z2c) / (1.0 + a1 * z1c + a2 * z2c);
    }
};

static Biquad norm(double b0, double b1, double b2, double a0, double a1, double a2) {
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}
static Biquad peaking(double fs, double f0, double gainDb, double Q) {
    const double A = std::pow(10, gainDb / 40), w = 2 * kPi * f0 / fs, al = std::sin(w) / (2 * Q), c = std::cos(w);
    return norm(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
}
static Biquad highShelf(double fs, double f0, double gainDb) {
    const double A = std::pow(10, gainDb / 40), w = 2 * kPi * f0 / fs, c = std::cos(w);
    const double al = std::sin(w) / 2 * std::sqrt(2.0), sA = 2 * std::sqrt(A) * al;
    return norm(A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
                (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
}
static Biquad lowShelf(double fs, double f0, double gainDb) {
    const double A = std::pow(10, gainDb / 40), w = 2 * kPi * f0 / fs, c = std::cos(w);
    const double al = std::sin(w) / 2 * std::sqrt(2.0), sA = 2 * std::sqrt(A) * al;
    return norm(A * ((A + 1) - (A - 1) * c + sA), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sA),
                (A + 1) + (A - 1) * c + sA, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sA);
}
static Biquad highPass(double fs, double f0, double Q) {
    const double w = 2 * kPi * f0 / fs, al = std::sin(w) / (2 * Q), c = std::cos(w);
    return norm((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
}

// Plays the probe through `sys` for several periods, then analyses 2 periods starting at `offset`.
// Probe period used at a sample rate (the plug-in picks it the same way).
static int periodFor(double fs, bool fine = false) { return 1 << orderForSampleRate(fs, fine); }

static Response measure(const std::function<float(float)>& sys, int offset, int warmPeriods = 6, double fs = 48000, bool fine = false) {
    const int order = orderForSampleRate(fs, fine), N = 1 << order;
    ProbeGenerator gen;
    gen.setOrder(order);
    const int total = (warmPeriods + 2) * N + offset;
    std::vector<float> x((size_t) total), y((size_t) total);
    gen.render(x.data(), total);
    for (int i = 0; i < total; ++i) y[(size_t) i] = sys(x[(size_t) i]);
    ResponseAnalyzer an(order);
    Response r;
    an.analyze(y.data() + total - 2 * N, r);
    return r;
}

static void compare(const char* name, const Response& r, double fs, const std::function<cplx(double)>& truth,
                    double magTol, double phaseTol, bool expectMinPhase = true, double minMagDbForPhase = -40) {
    double maxMag = 0, maxPh = 0;
    const int G = (int) (r.magDb.size() - 1) * 2;
    for (int k = 1; k < G / 2; ++k) {
        const double f = k * fs / G;
        if (f < 20 || f > 20000) continue;
        const cplx t = truth(2 * kPi * f / fs);
        const double tm = 20 * std::log10(std::abs(t));
        maxMag = std::max(maxMag, std::abs(r.magDb[(size_t) k] - tm));
        if (tm > minMagDbForPhase)
            maxPh = std::max(maxPh, (double) std::abs(wrapDeg((float) (r.phaseDeg[(size_t) k] - std::arg(t) * 180 / kPi))));
    }
    std::printf("%-36s lock=%d minPh=%d delay=%6d  max|dMag|=%.5f dB  max|dPhase|=%.4f deg\n",
                name, (int) r.lock, (int) r.minimumPhase, r.delay, maxMag, maxPh);
    CHECK(r.minimumPhase == expectMinPhase, "%s minimum-phase classification", name);
    CHECK(r.lock == Lock::Locked, "%s not locked", name);
    CHECK(maxMag < magTol, "%s magnitude error %.4f dB", name, maxMag);
    CHECK(maxPh < phaseTol, "%s phase error %.3f deg", name, maxPh);
}

int main() {
    // --- Probe signal properties, for each probe length
    for (int order = kMinOrder; order <= kMaxOrder; ++order) {
        const auto& t = probeSignal(order);
        const int N = 1 << order;
        double s2 = 0, pk = 0;
        for (float v : t) { s2 += (double) v * v; pk = std::max(pk, (double) std::abs(v)); }
        const double rmsDb = 10 * std::log10(s2 / N), crestDb = 20 * std::log10(pk / std::sqrt(s2 / N));
        FFT fft(order);
        std::vector<cplx> X((size_t) N);
        for (int i = 0; i < N; ++i) X[(size_t) i] = t[(size_t) i];
        fft.perform(X, false);
        double mn = 1e9, mx = 0;
        for (int k = 1; k < N / 2; ++k) { mn = std::min(mn, std::abs(X[(size_t) k])); mx = std::max(mx, std::abs(X[(size_t) k])); }
        const double ripple = 20 * std::log10(mx / mn);
        std::printf("Probe %5d smp: RMS %.3f dBFS, peak %.2f dBFS, crest %.2f dB, spectral ripple %.6f dB\n",
                    N, rmsDb, rmsDb + crestDb, crestDb, ripple);
        CHECK(std::abs(rmsDb - kSignalRmsDb) < 0.01, "probe RMS");
        CHECK(crestDb < 6.0, "crest factor %.2f dB", crestDb);
        CHECK(ripple < 0.001, "spectrum not flat: %.5f dB", ripple);
    }
    CHECK(periodFor(44100) == 8192 && periodFor(48000) == 8192 && periodFor(96000) == 16384 && periodFor(192000) == 32768,
          "Fast probe period per sample rate");
    CHECK(periodFor(48000, true) == 32768 && periodFor(96000, true) == 65536 && periodFor(192000, true) == 131072,
          "Fine probe period per sample rate");

    // --- Generator across odd block sizes equals the table and wraps
    {
        ProbeGenerator g;
        g.setOrder(kMinOrder);
        const int N = 1 << kMinOrder;
        std::vector<float> out((size_t) N * 2 + 1000);
        size_t pos = 0; int bs = 1;
        while (pos < out.size()) { const int n = (int) std::min<size_t>((size_t) bs, out.size() - pos); g.render(out.data() + pos, n); pos += (size_t) n; bs = bs * 7 % 1013 + 1; }
        bool ok = true;
        for (size_t i = 0; i < out.size(); ++i) ok &= out[i] == probeSignal(kMinOrder)[i % (size_t) N];
        CHECK(ok, "generator output mismatch");
    }

    // --- Flat signal through nothing: the flat curve
    for (int off : { 0, 1, 5555, periodFor(48000) - 1 })
        compare(("Bypass, window offset " + std::to_string(off)).c_str(), measure([](float x) { return x; }, off), 48000,
                [](double) { return cplx(1, 0); }, 0.001, 0.01);

    // --- Gain and latency
    {
        std::vector<float> dl(137, 0.0f); size_t p = 0;
        compare("Pure delay 137 smp, -6 dB", measure([&](float x) { float o = dl[p]; dl[p] = x * 0.5f; p = (p + 1) % dl.size(); return o; }, 999),
                48000, [](double) { return cplx(0.5, 0); }, 0.001, 0.01);
    }

    // --- Minimum-phase EQs: magnitude AND phase must match the analytic response
    struct Case { const char* name; double fs; Biquad bq; };
    const Case cases[] = {
        { "Peak +6 dB 1 kHz Q1 @48k",   48000, peaking(48000, 1000, 6, 1) },
        { "Peak -12 dB 3 kHz Q4 @48k",  48000, peaking(48000, 3000, -12, 4) },
        { "Peak +12 dB 40 Hz Q2 @44.1k", 44100, peaking(44100, 40, 12, 2) },
        { "High shelf -12 dB 8 kHz @48k", 48000, highShelf(48000, 8000, -12) },
        { "High shelf +9 dB 4 kHz @96k", 96000, highShelf(96000, 4000, 9) },
        { "Low shelf +9 dB 100 Hz @44.1k", 44100, lowShelf(44100, 100, 9) },
        { "Low shelf -15 dB 300 Hz @48k", 48000, lowShelf(48000, 300, -15) },
        { "High-pass 80 Hz Q0.707 @48k",  48000, highPass(48000, 80, 0.707) },
    };
    for (bool fine : { false, true })
        for (const auto& c : cases) {
            Biquad b = c.bq;
            const std::string name = std::string (fine ? "[Fine] " : "[Fast] ") + c.name;
            // Fast: the 40 Hz Q2 boost rings slightly past the 186 ms window (0.04 dB); all else is exact.
            compare(name.c_str(), measure([&](float x) { return b.process(x); }, 4321, 10, c.fs, fine), c.fs,
                    [&](double w) { return c.bq.response(w); }, fine ? 0.01 : 0.06, fine ? 0.2 : 0.4);
        }

    // --- Bands that ring longer than the Fast window: Fast must warn, Fine must be exact
    {
        struct LF { const char* name; Biquad bq; bool fastWarns; };
        const LF lfs[] = {
            { "+12 dB 30 Hz Q4 @48k", peaking(48000, 30, 12, 4), true },
            { "+15 dB 25 Hz Q6 @48k", peaking(48000, 25, 15, 6), true },
            { "+6 dB 100 Hz Q1 @48k", peaking(48000, 100, 6, 1), false },
        };
        for (const auto& c : lfs) {
            Biquad b1 = c.bq, b2 = c.bq;
            const Response fast = measure([&](float x) { return b1.process(x); }, 99, 14, 48000, false);
            std::printf("%-36s Fast: windowLimited=%d (tail %.1f dB)\n", c.name, (int) fast.windowLimited, fast.tailDb);
            CHECK(fast.windowLimited == c.fastWarns, "%s: Fast window-limited flag", c.name);
            const std::string name = std::string ("[Fine] ") + c.name;
            const Response fine = measure([&](float x) { return b2.process(x); }, 99, 8, 48000, true);
            CHECK(! fine.windowLimited, "%s: Fine should not be window-limited", c.name);
            compare(name.c_str(), fine, 48000, [&](double w) { return c.bq.response(w); }, 0.01, 0.2);
        }
    }

    // --- Several bands in series
    {
        Biquad a = lowShelf(48000, 120, 4), b = peaking(48000, 450, -5, 1.5), c = peaking(48000, 2500, 3, 0.7), d = highShelf(48000, 10000, 5);
        const Biquad a0 = a, b0 = b, c0 = c, d0 = d;
        compare("4-band EQ @48k", measure([&](float x) { return d.process(c.process(b.process(a.process(x)))); }, 777, 10), 48000,
                [&](double w) { return a0.response(w) * b0.response(w) * c0.response(w) * d0.response(w); }, 0.01, 0.2);
    }

    // --- Linear-phase FIR (latency 255): delay must be removed, leaving ~0 phase
    {
        const int L = 511, D = 255;
        std::vector<double> hfir(L);
        // Linear-phase bell: windowed sinc band-pass added to the identity (+boost around 2 kHz).
        for (int n = 0; n < L; ++n) {
            const double t = n - D, w = 0.5 - 0.5 * std::cos(2 * kPi * n / (L - 1));
            auto sinc = [](double x) { return x == 0 ? 1.0 : std::sin(kPi * x) / (kPi * x); };
            const double f1 = 1500.0 / 48000, f2 = 2500.0 / 48000;
            hfir[(size_t) n] = (n == D ? 1.0 : 0.0) + 1.0 * w * (2 * f2 * sinc(2 * f2 * t) - 2 * f1 * sinc(2 * f1 * t));
        }
        std::vector<float> line(L, 0.0f); size_t p = 0;
        auto sys = [&](float x) { line[p] = x; double acc = 0; for (int n = 0; n < L; ++n) acc += hfir[(size_t) n] * line[(p + L - (size_t) n) % L]; p = (p + 1) % L; return (float) acc; };
        auto truth = [&](double w) { cplx s(0, 0); for (int n = 0; n < L; ++n) s += hfir[(size_t) n] * std::polar(1.0, -w * (n - D)); return s; };
        compare("Linear-phase FIR bell (latency 255)", measure(sys, 2024, 3), 48000, truth, 0.01, 0.5, false);
    }

    // --- Analog-modelled EQ that adds hiss at -90 dBFS: still locked, still accurate
    {
        Biquad b = peaking(48000, 2000, 8, 1.2); const Biquad b0 = b;
        std::mt19937 rng(7); std::normal_distribution<float> nd(0, 3.16e-5f);
        compare("Bell +8 dB 2 kHz with -90 dBFS hiss", measure([&](float x) { return b.process(x) + nd(rng); }, 31, 6), 48000,
                [&](double w) { return b0.response(w); }, 0.05, 1.0);
    }

    // --- Knob moved during the analysed window: must report Settling/NotProbe, not Locked
    {
        Biquad a = peaking(48000, 1000, 0.5, 1), b = peaking(48000, 1000, 9, 1);
        int n = 0; const int change = 7 * periodFor(48000) + 3000;
        Response r = measure([&](float x) { float ya = a.process(x), yb = b.process(x); return n++ < change ? ya : yb; }, 5000, 6);
        std::printf("Gain changed mid-window: lock=%d periodicity=%.1f dB\n", (int) r.lock, r.periodicityDb);
        CHECK(r.lock != Lock::Locked, "should not be locked while the EQ is changing");
    }

    // --- Big EQ jumps must read as SETTLING (never NO PROBE); the other probe length must be rejected
    {
        const int N = periodFor(48000);
        ResponseAnalyzer an(orderForSampleRate(48000)); Response r;
        Biquad a = peaking(48000, 100, 15, 0.7), b = peaking(48000, 5000, -15, 0.7);
        ProbeGenerator g; g.setOrder(orderForSampleRate(48000));
        std::vector<float> x((size_t) N * 8), y(x.size());
        g.render(x.data(), (int) x.size());
        const int change = N * 4 + 777;
        for (size_t i = 0; i < x.size(); ++i) { const float ya = a.process(x[i]), yb = b.process(x[i]); y[i] = (int) i < change ? ya : yb; }
        int notProbe = 0;
        for (int end = change; end <= change + 2 * N + N / 4; end += 128) {
            an.analyze(y.data() + end - 2 * N, r);
            if (r.lock == Lock::NotProbe) ++notProbe;
        }
        std::printf("+15 dB 100 Hz -> -15 dB 5 kHz jump: %d analyses reported NO PROBE\n", notProbe);
        CHECK(notProbe == 0, "a big EQ jump must not read as NO PROBE");

        ProbeGenerator fineGen; fineGen.setOrder(orderForSampleRate(48000, true));
        std::vector<float> f((size_t) periodFor(48000, true) * 2);
        fineGen.render(f.data(), (int) f.size());
        int accepted = 0;
        for (int end = 2 * N; end <= (int) f.size(); end += 997) {
            an.analyze(f.data() + end - 2 * N, r);
            if (r.lock != Lock::NotProbe) ++accepted;
        }
        std::printf("Fine probe seen by the Fast analyzer: accepted %d times\n", accepted);
        CHECK(accepted == 0, "the Fast analyzer must reject the Fine probe");
    }

    // --- Lock detection
    {
        std::mt19937 rng(1);
        std::normal_distribution<float> nd(0, 0.1f);
        std::vector<float> noise((size_t) periodFor(48000) * 2);
        for (auto& v : noise) v = nd(rng);
        ResponseAnalyzer an (orderForSampleRate(48000)); Response r;
        an.analyze(noise.data(), r);
        std::printf("White noise (not a probe): lock=%d periodicity=%.1f dB\n", (int) r.lock, r.periodicityDb);
        CHECK(r.lock == Lock::NotProbe, "noise should not lock");
        std::vector<float> silence((size_t) periodFor(48000) * 2, 0.0f);
        an.analyze(silence.data(), r);
        CHECK(r.lock == Lock::NoSignal, "silence should be NoSignal");
    }

    // --- Display sampling and zero calibration
    {
        Response r = measure([](float x) { return 0.25f * x; }, 5);
        Response z = r;
        std::vector<float> freqs;
        for (int i = 0; i < 800; ++i) freqs.push_back((float) (20.0 * std::pow(1000.0, i / 799.0)));
        std::vector<float> m, ph; std::vector<double> pre;
        for (double sm : { 0.0, 1.0 / 24, 1.0 / 3 }) {
            sampleCurve(r.magDb, &r.phaseDeg, 48000, freqs, sm, m, &ph, pre);
            double e = 0; for (float v : m) e = std::max(e, std::abs(v - 20 * std::log10(0.25)));
            CHECK(e < 0.001, "display sampling (smooth %.3f) error %.4f", sm, e);
        }
        applyZero(r.magDb, r.phaseDeg, z.magDb, z.phaseDeg);
        double e = 0; for (int k = 1; k < kFineBins; ++k) e = std::max(e, (double) std::abs(r.magDb[(size_t) k]));
        CHECK(e < 1e-4, "zero calibration error %.5f", e);
    }

    std::printf(failures ? "\n%d FAILURE(S)\n" : "\nAll tests passed.\n", failures);
    return failures ? 1 : 0;
}
