#pragma once

#include "Effect.h"
#include <memory>

struct RhythmoBrightnessImpl;

// RhythmoBrightnessEffect
//
// Uses the Todd & Brown (1994) rhythmogram pipeline (gammatone filterbank →
// Meddis hair-cell transduction → channel pooling → multiscale leaky
// integrators) to derive a per-frame rhythmic energy value, then scales the
// brightness of the video frame by that energy.
class RhythmoBrightnessEffect : public Effect {
private:
    std::unique_ptr<RhythmoBrightnessImpl> impl;
    int lastSampleRate;

    void ensureImpl(int sampleRate);

public:
    RhythmoBrightnessEffect();
    ~RhythmoBrightnessEffect();

    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;

    std::vector<std::string> getParameterNames() const override {
        return {"brightness_gain", "audio_gain"};
    }

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "brightness_gain") return 5.0f;
        return Effect::getParameterNominalMax(name);
    }
};
