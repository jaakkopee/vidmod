#pragma once

#include "Effect.h"

class FFTEffect : public Effect {
public:
    FFTEffect();
    
    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;
    
    std::vector<std::string> getParameterNames() const override {
        return {"fft_r_coeff", "fft_g_coeff", "fft_b_coeff", "red_bias", "green_bias", "blue_bias", "audio_gain"};
    }

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "red_bias" || name == "green_bias" || name == "blue_bias") return 255.0f;
        return Effect::getParameterNominalMax(name);
    }
};
