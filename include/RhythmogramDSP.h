#pragma once

// RhythmogramDSP
//
// Shared DSP front-end for all Rhythmo* effects.  Implements the Todd &
// Brown (1994) rhythmogram pipeline ported from Swift (github.com/jaakkopee/
// SpectroGram):
//
//   PeripheralChain  (PreEmphasis → GammatoneBank → MeddisHairCells → pool)
//   MultiscaleMemoryBank  (3-stage leaky integrators, τ log-spaced 10ms–2.4s)
//
// Usage:
//   RhythmogramDSP dsp(sampleRate);
//   auto cols = dsp.processAndCollect(samples.data(), sampleCount);
//   float e   = dsp.normalizedEnergy(cols, audioGain);

#include <vector>
#include <memory>

struct RhythmogramDSPState; // defined in RhythmogramDSP.cpp

class RhythmogramDSP {
public:
    // Spontaneous pooled baseline used as the normalisation reference.
    const double pooledBaseline;

    // Number of memory units per hop column.
    static constexpr int UNIT_COUNT = 48;

    explicit RhythmogramDSP(double sampleRate);
    ~RhythmogramDSP();

    // Reset all filter and integrator states.
    void reset();

    // Process `count` mono float samples.  Returns any complete hop columns
    // emitted during processing; each column contains UNIT_COUNT floats.
    std::vector<std::vector<float>> processAndCollect(const float* samples, int count);

    // Compute the mean per-unit energy across `columns`, normalise by
    // pooledBaseline, and scale by audioGain.  Returns 0 when columns is empty.
    float normalizedEnergy(const std::vector<std::vector<float>>& columns,
                           float audioGain) const;

private:
    std::unique_ptr<RhythmogramDSPState> state_;
};
