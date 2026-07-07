#pragma once

#include "Effect.h"
#include <memory>

class RhythmogramDSP;

// RhythmoSaturationEffect
//
// Uses the rhythmogram pipeline to derive a per-frame rhythmic energy value
// and scales the saturation of every pixel by (1 + saturation_gain × energy).
// Saturates at the maximum OpenCV HSV saturation value (255).
class RhythmoSaturationEffect : public Effect {
private:
    std::unique_ptr<RhythmogramDSP> dsp_;
    int lastSampleRate_;

    void ensureDSP(int sampleRate);

public:
    RhythmoSaturationEffect();
    ~RhythmoSaturationEffect();

    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;

    std::vector<std::string> getParameterNames() const override {
        return {"saturation_gain", "audio_gain"};
    }

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "saturation_gain") return 5.0f;
        return Effect::getParameterNominalMax(name);
    }
};
