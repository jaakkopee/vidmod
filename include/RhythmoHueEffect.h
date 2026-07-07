#pragma once

#include "Effect.h"
#include <memory>

class RhythmogramDSP;

// RhythmoHueEffect
//
// Uses the rhythmogram pipeline to derive a per-frame rhythmic energy value
// and shifts the hue of every pixel by (hue_shift_gain × energy) degrees.
// Hue wraps within [0, 180) (OpenCV 8-bit HSV convention).
class RhythmoHueEffect : public Effect {
private:
    std::unique_ptr<RhythmogramDSP> dsp_;
    int lastSampleRate_;

    void ensureDSP(int sampleRate);

public:
    RhythmoHueEffect();
    ~RhythmoHueEffect();

    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;

    std::vector<std::string> getParameterNames() const override {
        return {"hue_shift_gain", "audio_gain"};
    }

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "hue_shift_gain") return 180.0f;
        return Effect::getParameterNominalMax(name);
    }
};
