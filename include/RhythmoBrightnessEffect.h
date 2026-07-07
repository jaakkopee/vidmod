#pragma once

#include "Effect.h"
#include <memory>

class RhythmogramDSP;

// RhythmoBrightnessEffect
//
// Uses the rhythmogram pipeline to derive a per-frame rhythmic energy value
// and scales the brightness of every pixel by (1 + brightness_gain × energy).
class RhythmoBrightnessEffect : public Effect {
private:
    std::unique_ptr<RhythmogramDSP> dsp_;
    int lastSampleRate_;

    void ensureDSP(int sampleRate);

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
