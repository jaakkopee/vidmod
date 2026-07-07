#pragma once

#include "Effect.h"
#include <memory>

class RhythmogramDSP;

// RhythmoLightEffect
//
// Uses the rhythmogram pipeline to derive a per-frame rhythmic energy value
// and projects it over the image's local maxima and their neighborhoods:
// the frame is dilated to a local-maxima image (neighborhood set by
// kernel_size / morph_iterations) and blended into the source with a
// coefficient of light_gain × energy.
class RhythmoLightEffect : public Effect {
private:
    std::unique_ptr<RhythmogramDSP> dsp_;
    int lastSampleRate_;

    void ensureDSP(int sampleRate);

public:
    RhythmoLightEffect();
    ~RhythmoLightEffect();

    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;

    std::vector<std::string> getParameterNames() const override {
        return {"light_gain", "kernel_size", "morph_iterations", "audio_gain"};
    }

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "kernel_size")       return 31.0f;
        if (name == "morph_iterations")  return 20.0f;
        if (name == "light_gain")        return 2.0f;
        return Effect::getParameterNominalMax(name);
    }
};
