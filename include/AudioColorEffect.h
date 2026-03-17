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
};
