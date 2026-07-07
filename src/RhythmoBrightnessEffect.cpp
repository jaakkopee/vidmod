// RhythmoBrightnessEffect.cpp
//
// Ports the Todd & Brown (1994) rhythmogram pipeline from the Swift
// implementation at github.com/jaakkopee/SpectroGram into C++.
//
// DSP chain (all in anonymous namespace):
//   ERB helpers            – Glasberg & Moore (1990) ERB-rate scale
//   GammatoneFilter        – Slaney (1993) 4-stage biquad per channel
//   GammatoneBank          – Multi-channel bank
//   PreEmphasis            – 1st-order high-pass (k = 0.95)
//   MeddisHairCell         – Meddis (1988) 3-state neurotransmitter model
//   PeripheralChain        – PreEmphasis + gammatone + hair cell + pool
//   MultiscaleMemoryBank   – Cascade of 3 leaky integrators per τ unit
//   RhythmogramAnalyzer    – Ties the chain together; emits hop columns
//
// The effect normalises the mean column energy by the spontaneous pooled
// baseline, then multiplies frame brightness by (1 + brightness_gain × energy).

#include "RhythmoBrightnessEffect.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

namespace {

// ===========================================================================
// Minimal complex arithmetic (used only at gammatone coefficient design time)
// ===========================================================================
struct CD {
    double r, i;
    CD(double r = 0.0, double i = 0.0) : r(r), i(i) {}
    CD operator+(CD b) const { return {r + b.r, i + b.i}; }
    CD operator-(CD b) const { return {r - b.r, i - b.i}; }
    CD operator*(CD b) const {
        return {r * b.r - i * b.i, r * b.i + i * b.r};
    }
    CD operator/(CD b) const {
        double d = b.r * b.r + b.i * b.i;
        return {(r * b.r + i * b.i) / d, (i * b.r - r * b.i) / d};
    }
    double magnitude() const { return std::sqrt(r * r + i * i); }
};

static CD cexpC(CD z) {
    double e = std::exp(z.r);
    return {e * std::cos(z.i), e * std::sin(z.i)};
}

// ===========================================================================
// ERB-scale helpers (Glasberg & Moore 1990)
// ===========================================================================
static double erbRate(double f) { return 21.4 * std::log10(0.00437 * f + 1.0); }
static double erbHz(double r)   { return (std::pow(10.0, r / 21.4) - 1.0) / 0.00437; }

static std::vector<double> erbCenterFreqs(int count, double fMin, double fMax) {
    if (count <= 0) return {};
    if (count == 1) return {fMin};
    double rMin = erbRate(fMin), rMax = erbRate(fMax);
    std::vector<double> cfs(count);
    for (int i = 0; i < count; i++) {
        double frac = static_cast<double>(i) / static_cast<double>(count - 1);
        cfs[i] = erbHz(rMin + frac * (rMax - rMin));
    }
    return cfs;
}

// ===========================================================================
// GammatoneFilter
//
// Slaney (1993) all-pole 4th-order Patterson gammatone realised as a cascade
// of four 2nd-order IIR sections (biquads).  Numerator b2 = 0 for every
// stage.  State is stored as Direct-Form-II transposed pairs (d0, d1).
// ===========================================================================
class GammatoneFilter {
public:
    static constexpr int STAGES = 4;

    GammatoneFilter(double cf, double fs) {
        double T       = 1.0 / fs;
        double erb     = 24.7 * (0.00437 * cf + 1.0);
        double bw      = 1.019 * 2.0 * M_PI * erb;
        double expBT   = std::exp(bw * T);
        double cosArg  = std::cos(2.0 * cf * M_PI * T);
        double sinArg  = std::sin(2.0 * cf * M_PI * T);
        double sqrtP   = std::sqrt(3.0 + std::pow(2.0, 1.5)); // sqrt(3 + 2√2)
        double sqrtM   = std::sqrt(3.0 - std::pow(2.0, 1.5)); // sqrt(3 − 2√2)

        // Shared denominator coefficients (all 4 stages)
        double a1d = -2.0 * cosArg / expBT;
        double a2d = std::exp(-2.0 * bw * T);

        // Per-stage numerator b1 (b0 = T, b2 = 0)
        auto nb1 = [&](double sign, double sqrtVal) {
            return -(2.0 * T * cosArg / expBT
                     + sign * 2.0 * sqrtVal * T * sinArg / expBT) / 2.0;
        };
        double n1[STAGES] = { nb1(+1.0, sqrtP), nb1(-1.0, sqrtP),
                              nb1(+1.0, sqrtM), nb1(-1.0, sqrtM) };

        // Gain normalisation evaluated at the centre frequency
        double omega = 2.0 * cf * M_PI * T;
        CD e4i  = cexpC({0.0, 4.0 * omega});
        CD e2i  = cexpC({-bw * T, 2.0 * omega});
        CD twoT = {2.0 * T, 0.0};

        auto zeroTerm = [&](double sign, double sqrtVal) -> CD {
            CD inner = {cosArg + sign * sqrtVal * sinArg, 0.0};
            return CD{-1.0, 0.0} * (twoT * e4i) + twoT * e2i * inner;
        };
        CD zeros = zeroTerm(-1.0, sqrtM) * zeroTerm(+1.0, sqrtM)
                 * zeroTerm(-1.0, sqrtP) * zeroTerm(+1.0, sqrtP);

        CD poleInner =
            CD{-2.0 / std::exp(2.0 * bw * T), 0.0}
            - CD{2.0, 0.0} * e4i
            + CD{2.0, 0.0} * (CD{1.0, 0.0} + e4i) / CD{expBT, 0.0};
        CD poles = poleInner * poleInner * poleInner * poleInner;
        double gain = (zeros / poles).magnitude();

        for (int s = 0; s < STAGES; s++) {
            double scale = (s == 0) ? gain : 1.0;
            b0_[s] = static_cast<float>(T   / scale);
            b1_[s] = static_cast<float>(n1[s] / scale);
        }
        a1_ = static_cast<float>(a1d);
        a2_ = static_cast<float>(a2d);

        reset();
    }

    void reset() {
        for (int s = 0; s < STAGES; s++) d_[s][0] = d_[s][1] = 0.0f;
    }

    // Direct-Form-II transposed biquad cascade (b2 = 0 for all stages)
    void process(const float* input, float* output, int count) {
        for (int n = 0; n < count; n++) {
            float x = input[n];
            for (int s = 0; s < STAGES; s++) {
                float y    = b0_[s] * x + d_[s][0];
                d_[s][0]  = b1_[s] * x - a1_ * y + d_[s][1];
                d_[s][1]  =             - a2_ * y;
                x = y;
            }
            output[n] = x;
        }
    }

private:
    float b0_[STAGES], b1_[STAGES]; // b2 = 0 for all stages
    float a1_, a2_;                  // shared denominator coefficients
    float d_[STAGES][2];             // Direct-Form-II transposed state
};

// ===========================================================================
// GammatoneBank – multi-channel bank
// ===========================================================================
class GammatoneBank {
public:
    GammatoneBank(int channelCount, double sampleRate, double fMin, double fMax) {
        auto cfs = erbCenterFreqs(channelCount, fMin, fMax);
        filters_.reserve(channelCount);
        for (double cf : cfs) filters_.emplace_back(cf, sampleRate);
    }

    void reset() { for (auto& f : filters_) f.reset(); }
    int channelCount() const { return static_cast<int>(filters_.size()); }

    // Fills outputs[ch][0..count-1] for each channel.
    void process(const float* input,
                 std::vector<std::vector<float>>& outputs,
                 int count) {
        int ch = channelCount();
        outputs.resize(ch);
        for (int c = 0; c < ch; c++) {
            outputs[c].resize(count);
            filters_[c].process(input, outputs[c].data(), count);
        }
    }

private:
    std::vector<GammatoneFilter> filters_;
};

// ===========================================================================
// PreEmphasis – 1st-order high-pass outer/middle-ear model (k = 0.95)
// ===========================================================================
class PreEmphasis {
public:
    void reset() { prev_ = 0.0f; }

    void process(const float* in, float* out, int count) {
        float p = prev_;
        for (int i = 0; i < count; i++) {
            float s = in[i];
            out[i] = s - 0.95f * p;
            p = s;
        }
        prev_ = p;
    }

private:
    float prev_ = 0.0f;
};

// ===========================================================================
// MeddisHairCell – Meddis (1988) inner-hair-cell transduction model
//
// Parameters from Meddis (1988) / Slaney's Auditory Toolbox defaults.
// State (q, c, w) is kept in double precision to accumulate small dt steps
// without drift.
// ===========================================================================
class MeddisHairCell {
    static constexpr double A = 5.0;
    static constexpr double B = 300.0;
    static constexpr double G = 2000.0;
    static constexpr double Y = 5.05;
    static constexpr double L = 2500.0;
    static constexpr double R = 6580.0;
    static constexpr double X = 66.31;
    static constexpr double H = 50000.0;
    static constexpr double M = 1.0;

public:
    explicit MeddisHairCell(double sampleRate) : sampleRate_(sampleRate) { reset(); }

    // Spontaneous (no-input) steady-state output h·cSS.
    static double spontaneousFiringLevel() {
        double kt  = G * A / (A + B);
        double cSS = Y * M * kt / (Y * (L + R) + L * kt);
        return H * cSS;
    }

    void reset() {
        double kt = G * A / (A + B);
        double cSS = Y * M * kt / (Y * (L + R) + L * kt);
        c_ = cSS;
        q_ = cSS * (L + R) / kt;
        w_ = cSS * R / X;
    }

    void process(const float* input, float* output, int count) {
        double dt = 1.0 / sampleRate_;
        double qL = q_, cL = c_, wL = w_;
        for (int n = 0; n < count; n++) {
            double s   = static_cast<double>(input[n]);
            double lim = s + A;
            double kt  = (lim > 0.0) ? G * lim / (lim + B) : 0.0;
            double dq  = (Y * (M - qL) + X * wL - kt * qL) * dt;
            double dc  = (kt * qL - (L + R) * cL) * dt;
            double dw  = (R * cL - X * wL) * dt;
            qL += dq; cL += dc; wL += dw;
            output[n] = static_cast<float>(H * cL);
        }
        q_ = qL; c_ = cL; w_ = wL;
    }

private:
    double sampleRate_;
    double q_ = 0.0, c_ = 0.0, w_ = 0.0;
};

// ===========================================================================
// PeripheralChain – pre-emphasis + gammatone filterbank + hair cells + sum
// ===========================================================================
class PeripheralChain {
public:
    static constexpr float PRESSURE_GAIN = 100.0f;

    PeripheralChain(double sampleRate, int channelCount, double fMin, double fMax)
        : bank_(channelCount, sampleRate, fMin, fMax)
    {
        hairCells_.reserve(channelCount);
        for (int c = 0; c < channelCount; c++)
            hairCells_.emplace_back(sampleRate);
    }

    void reset() {
        preEmph_.reset();
        bank_.reset();
        for (auto& hc : hairCells_) hc.reset();
    }

    int channelCount() const { return bank_.channelCount(); }

    // Processes `count` mono samples and writes the pooled neural firing rate
    // to `output[0..count-1]`.
    void process(const float* input, float* output, int count) {
        // 1. Pressure scaling
        std::vector<float> scaled(count);
        for (int i = 0; i < count; i++) scaled[i] = input[i] * PRESSURE_GAIN;

        // 2. Pre-emphasis
        std::vector<float> emph(count);
        preEmph_.process(scaled.data(), emph.data(), count);

        // 3. Gammatone filterbank
        std::vector<std::vector<float>> chanBuf;
        bank_.process(emph.data(), chanBuf, count);

        // 4. Meddis hair-cell transduction (in-place per channel)
        int ch = channelCount();
        for (int c = 0; c < ch; c++)
            hairCells_[c].process(chanBuf[c].data(), chanBuf[c].data(), count);

        // 5. Cross-channel summation → pooled output
        std::fill(output, output + count, 0.0f);
        for (int c = 0; c < ch; c++)
            for (int n = 0; n < count; n++)
                output[n] += chanBuf[c][n];
    }

private:
    PreEmphasis preEmph_;
    GammatoneBank bank_;
    std::vector<MeddisHairCell> hairCells_;
};

// ===========================================================================
// MultiscaleMemoryBank
//
// Bank of unitCount leaky-integrator units.  Each unit approximates a
// Gaussian low-pass of time constant τ using a 3-stage cascade of 1st-order
// sections with per-stage time constant τ/3.
// State is kept in double precision to accumulate small per-sample increments
// at long τ without drift.
// ===========================================================================
class MultiscaleMemoryBank {
    static constexpr int STAGES = 3;
public:
    MultiscaleMemoryBank(double sampleRate, int unitCount,
                         double tauMin, double tauMax)
        : unitCount_(unitCount)
        , state_(unitCount * STAGES, 0.0)
        , alphas_(unitCount)
    {
        double logMin = std::log(tauMin), logMax = std::log(tauMax);
        double dt = 1.0 / sampleRate;
        for (int u = 0; u < unitCount; u++) {
            double frac = (unitCount > 1)
                ? static_cast<double>(u) / static_cast<double>(unitCount - 1)
                : 0.0;
            double tau = std::exp(logMin + frac * (logMax - logMin));
            double stageTau = tau / static_cast<double>(STAGES);
            alphas_[u] = 1.0 - std::exp(-dt / stageTau);
        }
    }

    void reset() { std::fill(state_.begin(), state_.end(), 0.0); }
    int unitCount() const { return unitCount_; }

    // Push one input sample through every unit; write final-stage values to
    // outputs[0..unitCount-1].
    void step(double input, double* outputs) {
        double* s = state_.data();
        for (int u = 0; u < unitCount_; u++) {
            double a = alphas_[u], b = 1.0 - a;
            int base = u * STAGES;
            double s0 = a * input + b * s[base + 0];
            double s1 = a * s0   + b * s[base + 1];
            double s2 = a * s1   + b * s[base + 2];
            s[base + 0] = s0;
            s[base + 1] = s1;
            s[base + 2] = s2;
            outputs[u] = s2;
        }
    }

private:
    int unitCount_;
    std::vector<double> state_, alphas_;
};

// ===========================================================================
// RhythmogramAnalyzer
//
// Pulls audio samples through PeripheralChain, subtracts the spontaneous
// pooled baseline, half-wave rectifies, feeds the result into
// MultiscaleMemoryBank, and accumulates per-unit maxima over each hop window.
// When a hop is complete, the maxima vector is emitted as a column.
// ===========================================================================
class RhythmogramAnalyzer {
    static constexpr int    CHAN_COUNT = 64;
    static constexpr int    UNIT_COUNT = 48;
    static constexpr double F_MIN      = 100.0;
    static constexpr double F_MAX      = 5000.0;
    static constexpr double TAU_MIN    = 0.010;
    static constexpr double TAU_MAX    = 2.400;
    static constexpr int    HOP_SIZE   = 1024;

public:
    // The spontaneous pooled level used as the zero reference.
    const double pooledBaseline;
    // Number of memory units (columns width).
    static int unitCount() { return UNIT_COUNT; }

    explicit RhythmogramAnalyzer(double sampleRate)
        : pooledBaseline(static_cast<double>(CHAN_COUNT)
                         * MeddisHairCell::spontaneousFiringLevel())
        , chain_(sampleRate, CHAN_COUNT, F_MIN, F_MAX)
        , memBank_(sampleRate, UNIT_COUNT, TAU_MIN, TAU_MAX)
        , pooledBuf_(HOP_SIZE * 2)
        , unitScratch_(UNIT_COUNT, 0.0)
        , colMaxima_(UNIT_COUNT, 0.0)
        , samplesInHop_(0)
    {}

    void reset() {
        chain_.reset();
        memBank_.reset();
        std::fill(colMaxima_.begin(), colMaxima_.end(), 0.0);
        samplesInHop_ = 0;
    }

    // Process `count` audio samples; return any complete hop columns that
    // were emitted (each column is a vector of UNIT_COUNT floats).
    std::vector<std::vector<float>> processAndCollect(const float* input, int count) {
        std::vector<std::vector<float>> columns;
        double baseline = pooledBaseline;

        int offset = 0;
        while (offset < count) {
            int n = std::min(static_cast<int>(pooledBuf_.size()), count - offset);
            chain_.process(input + offset, pooledBuf_.data(), n);

            double* uScratch = unitScratch_.data();
            for (int i = 0; i < n; i++) {
                double excess = std::max(0.0, static_cast<double>(pooledBuf_[i]) - baseline);
                memBank_.step(excess, uScratch);

                for (int u = 0; u < UNIT_COUNT; u++)
                    if (uScratch[u] > colMaxima_[u]) colMaxima_[u] = uScratch[u];

                if (++samplesInHop_ >= HOP_SIZE) {
                    std::vector<float> col(UNIT_COUNT);
                    for (int u = 0; u < UNIT_COUNT; u++)
                        col[u] = static_cast<float>(colMaxima_[u]);
                    columns.push_back(std::move(col));
                    std::fill(colMaxima_.begin(), colMaxima_.end(), 0.0);
                    samplesInHop_ = 0;
                }
            }
            offset += n;
        }
        return columns;
    }

private:
    PeripheralChain chain_;
    MultiscaleMemoryBank memBank_;
    std::vector<float>  pooledBuf_;
    std::vector<double> unitScratch_, colMaxima_;
    int samplesInHop_;
};

} // anonymous namespace

// ===========================================================================
// RhythmoBrightnessImpl
// ===========================================================================
struct RhythmoBrightnessImpl {
    RhythmogramAnalyzer analyzer;
    explicit RhythmoBrightnessImpl(double sampleRate) : analyzer(sampleRate) {}
};

// ===========================================================================
// RhythmoBrightnessEffect
// ===========================================================================
RhythmoBrightnessEffect::RhythmoBrightnessEffect()
    : Effect("RhythmoBrightness"), lastSampleRate(0)
{
    setParameter("brightness_gain", 1.0f);
    setParameter("audio_gain",      1.0f);
}

RhythmoBrightnessEffect::~RhythmoBrightnessEffect() = default;

void RhythmoBrightnessEffect::ensureImpl(int sampleRate) {
    if (!impl || lastSampleRate != sampleRate) {
        impl = std::make_unique<RhythmoBrightnessImpl>(static_cast<double>(sampleRate));
        lastSampleRate = sampleRate;
    }
}

cv::Mat RhythmoBrightnessEffect::apply(const cv::Mat& frame,
                                        AudioBuffer* audioBuffer,
                                        float videoFps) {
    if (!audioBuffer) return frame.clone();

    int audioFramesPerVideoFrame =
        static_cast<int>(audioBuffer->getSampleRate() / videoFps);
    std::vector<float> samples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
    if (samples.empty()) return frame.clone();

    float brightnessGain = getParameter("brightness_gain", 1.0f);
    float audioGain      = getParameter("audio_gain",      1.0f);

    ensureImpl(audioBuffer->getSampleRate());

    // Run audio through rhythmogram pipeline; collect any emitted hop columns.
    auto columns = impl->analyzer.processAndCollect(
        samples.data(), static_cast<int>(samples.size()));

    // Compute mean unit energy across all emitted columns.
    float energy = 0.0f;
    if (!columns.empty()) {
        for (const auto& col : columns)
            for (float v : col)
                energy += v;
        energy /= static_cast<float>(columns.size() * RhythmogramAnalyzer::unitCount());
    }

    // Normalise by the spontaneous pooled baseline so that energy ≈ 1 when
    // the signal equals the baseline level.
    energy /= static_cast<float>(impl->analyzer.pooledBaseline);
    energy *= audioGain;

    // Brightness factor: 1 at silence, grows with rhythmic energy.
    float factor = 1.0f + brightnessGain * energy;
    factor = std::max(0.0f, factor);

    // Scale every pixel uniformly and clamp to [0, 255].
    cv::Mat output;
    frame.convertTo(output, CV_32FC3);
    output *= factor;
    cv::min(output, 255.0f, output);
    output.convertTo(output, CV_8UC3);

    return output;
}
