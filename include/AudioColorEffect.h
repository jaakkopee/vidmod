#pragma once

#include "Effect.h"

class AudioColorEffect : public Effect {
public:
    AudioColorEffect();
    
    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;
    
    std::vector<std::string> getParameterNames() const override {
        return {
            "color_coeff",
            "mode",
            "hue_strength",
            "saturation_strength",
            "value_strength",
            "audio_gain"
        };
    }

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "mode") return 2.0f;          // enum: 0=RGB, 1=HSV dominant, 2=HSV spectrum
        return Effect::getParameterNominalMax(name);
    }
};
