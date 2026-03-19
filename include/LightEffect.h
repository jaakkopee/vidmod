#pragma once

#include "Effect.h"

class LightEffect : public Effect {
private:
    cv::Mat findLocalMaxima(const cv::Mat& frame, int x, int y);

public:
    LightEffect();
    
    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;
    
    std::vector<std::string> getParameterNames() const override {
        return {"light_coeff", "kernel_size", "morph_iterations", "audio_gain"};
    }

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "kernel_size")       return 31.0f;
        if (name == "morph_iterations")  return 20.0f;
        if (name == "light_coeff")       return 1.0f;
        return Effect::getParameterNominalMax(name);
    }
};
