#pragma once
// EQScope DSP core. Plain C++17, no JUCE dependency, so it can be unit tested.
//
// How the measurement works
// -------------------------
// The Signal instance plays a periodic "multisine": one period of 2^order samples
// containing every FFT bin from 1 to N/2-1 at exactly the same amplitude, with
// Schroeder phases so the crest factor stays low. Its spectrum is perfectly flat.
//
// Because the signal repeats every period, ANY one-period window of
// it (or of a linear EQ's steady-state output) is just a circular shift of one
// period. So a rectangular-window FFT of that window has no leakage, and dividing
// it by the known probe spectrum gives the EQ's exact transfer function H(f) at
// every bin. No sidechain, no sync between instances, no latency compensation
// needed: the window offset only adds a pure delay, which is estimated and removed
// before the phase is shown.

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace probe {

constexpr double kPi = 3.14159265358979323846;
// Probe period: 2^order samples, chosen from the sample rate so it lasts a fixed time:
//   Fast (default): ~170 ms (8192 samples at 44.1/48 kHz, 16384 at 88.2/96, 32768 at 176.4/192).
//                   After an EQ change the curve has fully caught up within one period.
//   Fine:           ~680 ms (4x longer), for very narrow low-frequency bands that ring longer
//                   than the Fast window.
// The analyzer recognises which probe is arriving by its period, so only the SIGNAL copy chooses.
constexpr int kMinOrder = 13, kMaxOrder = 17;
inline int orderForSampleRate(double sampleRate, bool fine = false) {
    const int fast = sampleRate > 100000.0 ? 15 : sampleRate > 50000.0 ? 14 : 13;
    return fine ? fast + 2 : fast;
}

// The response is delivered on a grid of at least 32768 points (bin k = k * fs / gridSize Hz;
// gridSize = max(32768, period)). A short probe gives coarse bins directly; the fine grid is
// rebuilt from the EQ's impulse response (zero-padded), which is exact as long as the EQ's
// ringing dies away within one period. Response::windowLimited flags when it doesn't.
constexpr int kFineOrder = 15;
constexpr int kFineSize = 1 << kFineOrder;
constexpr int kFineBins = kFineSize / 2;
constexpr double kSignalRmsDb = -18.0; // probe level, dBFS RMS (nominal level for analog-modelled EQs)

using cplx = std::complex<double>;

//==============================================================================
class FFT {
public:
    explicit FFT(int order) : n(1 << order), re((size_t) n / 2), im((size_t) n / 2), rev((size_t) n) {
        for (int i = 0; i < n / 2; ++i) {
            re[(size_t) i] = std::cos(2.0 * kPi * i / n);
            im[(size_t) i] = -std::sin(2.0 * kPi * i / n);
        }
        for (int i = 0; i < n; ++i) {
            int r = 0;
            for (int b = 0; b < order; ++b)
                if (i & (1 << b)) r |= 1 << (order - 1 - b);
            rev[(size_t) i] = r;
        }
    }

    int size() const { return n; }

    // In-place, unscaled. inverse = true uses conjugate twiddles (caller divides by n if needed).
    // Plain arithmetic rather than std::complex multiplies, which compilers route through a slow
    // NaN-safe library call.
    void perform(std::vector<cplx>& data, bool inverse) const {
        double* d = reinterpret_cast<double*>(data.data()); // std::complex<double> is laid out as {re, im}
        for (int i = 0; i < n; ++i) {
            const int j = rev[(size_t) i];
            if (i < j) { std::swap(d[2 * i], d[2 * j]); std::swap(d[2 * i + 1], d[2 * j + 1]); }
        }
        const double sign = inverse ? -1.0 : 1.0;
        for (int len = 2; len <= n; len <<= 1) {
            const int half = len / 2, step = n / len;
            for (int j = 0; j < half; ++j) {
                const double wr = re[(size_t) (j * step)], wi = sign * im[(size_t) (j * step)];
                for (int i = j; i < n; i += len) {
                    double* u = d + 2 * i;
                    double* v = d + 2 * (i + half);
                    const double tr = v[0] * wr - v[1] * wi;
                    const double ti = v[0] * wi + v[1] * wr;
                    v[0] = u[0] - tr; v[1] = u[1] - ti;
                    u[0] += tr;       u[1] += ti;
                }
            }
        }
    }

private:
    int n;
    std::vector<double> re, im;
    std::vector<int> rev;
};

//==============================================================================
// One period of the flat-spectrum probe signal (deterministic: every instance builds the same table).
inline std::vector<float> makeProbeSignal(int order) {
    const int N = 1 << order, K = N / 2 - 1;
    FFT fft(order);
    std::vector<cplx> X((size_t) N, cplx(0.0, 0.0));

    for (int k = 1; k <= K; ++k) {
        // Schroeder phase -pi*k*(k-1)/K, reduced exactly with integer maths.
        const int64_t r = ((int64_t) k * (k - 1)) % (2 * (int64_t) K);
        const double phi = -kPi * (double) r / K;
        X[(size_t) k] = std::polar(1.0, phi);
        X[(size_t) (N - k)] = std::conj(X[(size_t) k]);
    }
    fft.perform(X, true);

    double sum2 = 0.0;
    for (const auto& v : X) sum2 += v.real() * v.real();
    const double rms = std::sqrt(sum2 / N);
    const double g = std::pow(10.0, kSignalRmsDb / 20.0) / rms;

    std::vector<float> out((size_t) N);
    for (int i = 0; i < N; ++i) out[(size_t) i] = (float) (X[(size_t) i].real() * g);
    return out;
}

inline const std::vector<float>& probeSignal(int order) {
    static const std::vector<float> tables[] = { makeProbeSignal(13), makeProbeSignal(14), makeProbeSignal(15),
                                                 makeProbeSignal(16), makeProbeSignal(17) };
    return tables[std::clamp(order, kMinOrder, kMaxOrder) - kMinOrder];
}

//==============================================================================
class ProbeGenerator {
public:
    ProbeGenerator() { (void) probeSignal(kMinOrder); } // build the tables off the audio thread
    void setOrder(int newOrder) { order = std::clamp(newOrder, kMinOrder, kMaxOrder); pos = 0; }
    int getOrder() const { return order; }
    void reset() { pos = 0; }

    void render(float* out, int numSamples) {
        const auto& t = probeSignal(order);
        const int period = 1 << order;
        int done = 0;
        while (done < numSamples) {
            const int chunk = std::min(numSamples - done, period - pos);
            std::copy(t.begin() + pos, t.begin() + pos + chunk, out + done);
            done += chunk;
            pos = (pos + chunk) & (period - 1);
        }
    }

private:
    int order = kMinOrder;
    int pos = 0;
};

//==============================================================================
// Energy of (last period - the period before it) relative to the last period, in dB. Cheap (no FFT):
// very negative means the signal is repeating exactly, i.e. nothing in the chain has changed.
inline double periodicityDb(const float* samples, int period) {
    const float* prev = samples;
    const float* cur = samples + period;
    double e = 0.0, p = 0.0;
    for (int i = 0; i < period; ++i) {
        const double d = (double) cur[i] - (double) prev[i];
        e += d * d;
        p += (double) cur[i] * (double) cur[i];
    }
    return 10.0 * std::log10((e + 1e-30) / (p + 1e-30));
}

//==============================================================================
enum class Lock { NoSignal, NotProbe, Settling, Locked };

struct Response {
    std::vector<float> magDb;    // size gridSize / 2 + 1; bin k is k * fs / gridSize Hz (gridSize >= kFineSize)
    std::vector<float> phaseDeg; // same bins, delay removed, wrapped to (-180, 180]
    Lock lock = Lock::NoSignal;
    double levelDb = -200.0;       // RMS of the analysed window, dBFS
    double periodicityDb = 0.0;    // energy of (this period - previous period) relative to this period
    int delay = 0;                 // removed delay in samples (includes the arbitrary window offset)
    bool minimumPhase = true;      // false: linear-phase or mixed-phase EQ (phase is relative to the impulse peak)
    double tailDb = -200.0;        // impulse response level at the end of the window, relative to its peak
    bool windowLimited = false;    // the EQ rings longer than the probe period: low-frequency detail is approximate
    double irCrestDb = 0.0;        // impulse response peak vs its RMS: high for any real EQ (even mid-change),
                                   // ~12 dB when the window holds something that isn't this probe
};

class ResponseAnalyzer {
public:
    explicit ResponseAnalyzer(int probeOrder = kMinOrder)
        : order(std::clamp(probeOrder, kMinOrder, kMaxOrder)), N(1 << order), bins(N / 2),
          grid(std::max(kFineSize, N)), fft(order), fineFft(std::max(kFineOrder, order)),
          buf((size_t) N), fine((size_t) grid),
          ref((size_t) bins + 1), h((size_t) bins + 1) {
        const auto& t = probeSignal(order);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = cplx(t[(size_t) i], 0.0);
        fft.perform(buf, false);
        for (int k = 0; k <= bins; ++k) ref[(size_t) k] = buf[(size_t) k];
    }

    int getOrder() const { return order; }
    int getPeriod() const { return N; }
    int getGridSize() const { return grid; }

    // samples: the 2 * getPeriod() most recent samples, oldest first.
    void analyze(const float* samples, Response& r) {
        const float* prev = samples;
        const float* cur = samples + N;

        double e = 0.0, p = 0.0;
        for (int i = 0; i < N; ++i) {
            const double d = (double) cur[i] - (double) prev[i];
            e += d * d;
            p += (double) cur[i] * (double) cur[i];
        }
        r.levelDb = 10.0 * std::log10(p / N + 1e-30);
        r.periodicityDb = 10.0 * std::log10((e + 1e-30) / (p + 1e-30));
        r.magDb.assign((size_t) grid / 2 + 1, -200.0f);
        r.phaseDeg.assign((size_t) grid / 2 + 1, 0.0f);
        r.tailDb = -200.0;
        r.windowLimited = false;

        if (r.levelDb < -90.0) { r.lock = Lock::NoSignal; return; }
        // Provisional; a window that isn't periodic is re-judged below from the impulse response.
        r.lock = r.periodicityDb < -30.0 ? Lock::Locked : Lock::Settling;

        for (int i = 0; i < N; ++i) buf[(size_t) i] = cplx(cur[i], 0.0);
        fft.perform(buf, false);

        for (int k = 1; k < bins; ++k) h[(size_t) k] = buf[(size_t) k] / ref[(size_t) k];
        h[0] = h[1];
        h[(size_t) bins] = h[(size_t) bins - 1];

        r.delay = estimateDelay(r.minimumPhase);

        // Delay-free impulse response (circular, length N). The probe carries no DC or Nyquist
        // energy, so those two bins are extrapolated (quadratically, which is exact for flat
        // responses and for the k^2 roll-off of a high-pass) and kept real, as a real impulse
        // response needs.
        for (int k = 1; k < bins; ++k) {
            const int64_t m = (((int64_t) k * r.delay) % N + N) % N;
            const cplx v = h[(size_t) k] * std::polar(1.0, 2.0 * kPi * (double) m / N);
            buf[(size_t) k] = v;
            buf[(size_t) (N - k)] = std::conj(v);
        }
        buf[0] = cplx(3.0 * buf[1].real() - 3.0 * buf[2].real() + buf[3].real(), 0.0);
        const size_t b = (size_t) bins;
        buf[b] = cplx(3.0 * buf[b - 1].real() - 3.0 * buf[b - 2].real() + buf[b - 3].real(), 0.0);
        fft.perform(buf, true);

        // Zero-pad to the fine grid. Samples before 'split' are after the impulse; the rest wrap
        // round to just before it (pre-ringing). Minimum-phase EQs never ring before the impulse,
        // so almost the whole period is theirs; linear-phase EQs ring both ways, so half each side.
        const int split = r.minimumPhase ? N - 32 : N / 2;
        std::fill(fine.begin(), fine.end(), cplx(0.0, 0.0));
        double peak = 0.0, tail = 0.0, energy = 0.0;
        for (int n = 0; n < N; ++n) {
            const double v = buf[(size_t) n].real() / N;
            energy += v * v;
            fine[(size_t) (n < split ? n : grid - (N - n))] = cplx(v, 0.0);
            peak = std::max(peak, std::abs(v));
            // The last eighth of the window on each side of the split: if the EQ is still
            // ringing there, its tail has wrapped round and the low end is approximate.
            const int fromSplit = n < split ? split - n : n - split + 1;
            if (fromSplit <= N / 8) tail = std::max(tail, std::abs(v));
        }
        r.tailDb = 20.0 * std::log10((tail + 1e-30) / (peak + 1e-30));
        r.irCrestDb = 10.0 * std::log10((peak * peak + 1e-60) / (energy / N + 1e-60));

        // While an EQ is being changed the two periods differ, sometimes by as much as the signal
        // itself, but the measured impulse response stays sharp (crest > 35 dB). Anything that
        // isn't this probe (music, noise, the other probe length) gives a noise-like one (< 13 dB).
        if (r.lock != Lock::Locked && r.periodicityDb >= -6.0 && r.irCrestDb < kMinCrestDb)
            r.lock = Lock::NotProbe;
        // Hiss in the EQ also shows up in the tail, about 30 dB below the period-to-period
        // difference; only flag ringing that stands clearly above that, and only once settled.
        r.windowLimited = r.lock == Lock::Locked && r.tailDb > std::max(kTailLimitDb, r.periodicityDb - 24.0);
        fineFft.perform(fine, false);

        for (int k = 0; k <= grid / 2; ++k) {
            const cplx v = fine[(size_t) k];
            r.magDb[(size_t) k] = (float) std::max(-200.0, 10.0 * std::log10(std::norm(v) + 1e-24));
            r.phaseDeg[(size_t) k] = (float) (std::arg(v) * 180.0 / kPi);
        }
    }

private:
    int wrapDelay(long d) const {
        d %= N;
        if (d > N / 2) d -= N;
        if (d <= -N / 2) d += N;
        return (int) d;
    }

    // Pure delay to remove (window offset + any EQ latency), in whole samples.
    //
    // 1. Build the minimum-phase response that has the measured magnitude (real cepstrum).
    // 2. Excess phase = measured phase - minimum phase. For a minimum-phase EQ (analog-style
    //    bells, shelves, filters) that is exactly a pure delay, so its slope gives the delay
    //    and the phase shown is the EQ's true phase.
    // 3. If the excess phase isn't a straight line, the EQ isn't minimum-phase (linear-phase
    //    or mixed). Then the delay is taken from the impulse response peak, so a linear-phase
    //    EQ shows ~0 phase.
    int estimateDelay(bool& isMinimumPhase) {
        // Minimum-phase spectrum from log magnitude.
        for (int k = 0; k <= bins; ++k) {
            const double lm = std::log(std::max(std::abs(h[(size_t) k]), 1e-10));
            buf[(size_t) k] = cplx(lm, 0.0);
            if (k > 0 && k < bins) buf[(size_t) (N - k)] = cplx(lm, 0.0);
        }
        fft.perform(buf, true);
        for (int n = 0; n < N; ++n) {
            const double c = buf[(size_t) n].real() / N;
            const double w = (n == 0 || n == bins) ? 1.0 : (n < bins ? 2.0 : 0.0);
            buf[(size_t) n] = cplx(c * w, 0.0);
        }
        fft.perform(buf, false);

        // Excess-phase phasors (bins 1..bins-1), stored back into buf.
        cplx acc(0.0, 0.0);
        for (int k = 1; k < bins; ++k) {
            const cplx hm = std::exp(buf[(size_t) k]);
            const cplx ex = h[(size_t) k] * std::conj(hm);
            const double m = std::abs(ex);
            buf[(size_t) k] = m > 1e-30 ? ex / m : cplx(1.0, 0.0);
            if (k > 1) acc += buf[(size_t) k] * std::conj(buf[(size_t) k - 1]);
        }
        const int dMin = wrapDelay(std::lround(-std::arg(acc) * N / (2.0 * kPi)));

        // How straight is the excess phase after removing that delay? Check bins that carry signal.
        double peakDb = -200.0;
        for (int k = 1; k < bins; ++k) peakDb = std::max(peakDb, 20.0 * std::log10(std::abs(h[(size_t) k]) + 1e-30));
        double sum = 0.0; int count = 0;
        for (int k = 1; k < bins; k += 2) {
            if (20.0 * std::log10(std::abs(h[(size_t) k]) + 1e-30) < peakDb - 40.0) continue;
            const int64_t m = (((int64_t) k * dMin) % N + N) % N;
            const double e = std::arg(buf[(size_t) k] * std::polar(1.0, 2.0 * kPi * (double) m / N));
            sum += e * e; ++count;
        }
        const double rmsDeg = count ? std::sqrt(sum / count) * 180.0 / kPi : 0.0;
        isMinimumPhase = rmsDeg < 2.0;
        if (isMinimumPhase) return dMin;

        // Impulse response peak.
        for (int k = 0; k <= bins; ++k) {
            buf[(size_t) k] = h[(size_t) k];
            if (k > 0 && k < bins) buf[(size_t) (N - k)] = std::conj(h[(size_t) k]);
        }
        fft.perform(buf, true);
        int best = 0; double bestV = -1.0;
        for (int n = 0; n < N; ++n) {
            const double v = std::abs(buf[(size_t) n].real());
            if (v > bestV) { bestV = v; best = n; }
        }
        return wrapDelay(best);
    }

    static constexpr double kTailLimitDb = -85.0; // ~0.1 dB worst-case error at the low end
    static constexpr double kMinCrestDb = 25.0;   // between real EQs (> 35 dB) and non-probe audio (< 13 dB)

    int order, N, bins, grid;
    FFT fft, fineFft;
    std::vector<cplx> buf, fine, ref, h;
};

//==============================================================================
inline float wrapDeg(float d) {
    while (d > 180.0f) d -= 360.0f;
    while (d <= -180.0f) d += 360.0f;
    return d;
}

// Subtract a stored "zero" calibration curve (per bin) from a response, in place.
// The zero may come from a different grid size (Fast vs Fine probe at high sample rates);
// it is then resampled by frequency.
inline void applyZero(std::vector<float>& magDb, std::vector<float>& phaseDeg,
                      const std::vector<float>& zeroMag, const std::vector<float>& zeroPhase) {
    if (zeroMag.size() < 2 || zeroMag.size() != zeroPhase.size() || magDb.size() < 2 || magDb.size() != phaseDeg.size()) return;
    const size_t n = magDb.size(), zn = zeroMag.size();
    for (size_t k = 0; k < n; ++k) {
        float zm, zp;
        if (zn == n) { zm = zeroMag[k]; zp = zeroPhase[k]; }
        else {
            const double pos = (double) k * (double) (zn - 1) / (double) (n - 1);
            const size_t k0 = std::min(zn - 2, (size_t) pos);
            const float t = (float) (pos - (double) k0);
            zm = zeroMag[k0] * (1.0f - t) + zeroMag[k0 + 1] * t;
            zp = zeroPhase[t < 0.5f ? k0 : k0 + 1];
        }
        magDb[k] -= zm;
        phaseDeg[k] = wrapDeg(phaseDeg[k] - zp);
    }
}

// Resample a per-bin curve at display frequencies (ascending, Hz).
// smoothOct > 0: fractional-octave power average. 0: each point averages the bins
// under its own pixel (or interpolates where bins are wider than a pixel).
inline void sampleCurve(const std::vector<float>& magDb, const std::vector<float>* phaseDeg,
                        double sampleRate, const std::vector<float>& freqs, double smoothOct,
                        std::vector<float>& outMag, std::vector<float>* outPhase,
                        std::vector<double>& prefix) {
    const int M = (int) magDb.size() - 1;
    const size_t n = freqs.size();
    outMag.assign(n, -200.0f);
    if (outPhase) outPhase->assign(n, 0.0f);
    if (M < 3 || sampleRate <= 0.0) return;

    prefix.assign((size_t) M + 2, 0.0);
    for (int b = 0; b <= M; ++b)
        prefix[(size_t) b + 1] = prefix[(size_t) b] + std::pow(10.0, magDb[(size_t) b] / 10.0);

    const double binPerHz = 2.0 * M / sampleRate;
    const double sHalf = std::pow(2.0, smoothOct / 2.0);

    for (size_t i = 0; i < n; ++i) {
        const double f = freqs[i];
        double lo, hi;
        if (smoothOct > 0.0) { lo = f / sHalf; hi = f * sHalf; }
        else {
            lo = i > 0 ? std::sqrt((double) freqs[i - 1] * f) : f;
            hi = i + 1 < n ? std::sqrt(f * (double) freqs[i + 1]) : f;
        }
        const double b = f * binPerHz;
        const int a = std::max(1, (int) std::ceil(lo * binPerHz));
        const int z = std::min(M - 1, (int) std::floor(hi * binPerHz));

        if (z - a + 1 >= 2) {
            const double avg = (prefix[(size_t) z + 1] - prefix[(size_t) a]) / (z - a + 1);
            outMag[i] = (float) (10.0 * std::log10(avg + 1e-30));
        } else {
            const double bc = std::clamp(b, 1.0, (double) (M - 1));
            const int k0 = (int) std::floor(bc);
            const int k1 = std::min(k0 + 1, M - 1);
            const double t = bc - k0;
            outMag[i] = (float) (magDb[(size_t) k0] * (1.0 - t) + magDb[(size_t) k1] * t);
        }
        if (outPhase && phaseDeg) {
            const int kn = std::clamp((int) std::lround(b), 1, M - 1);
            (*outPhase)[i] = (*phaseDeg)[(size_t) kn];
        }
    }
}

} // namespace probe
