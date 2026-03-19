#pragma once

#include "Effect.h"

class DiffuseEffect : public Effect {
public:
    DiffuseEffect();
    
    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;
    
    std::vector<std::string> getParameterNames() const override {
        return {
            "diffuse_coeff",
            "iterations",
            "kernel_size",
            "kernel_growth",
            "iteration_decay",
            "audio_gain"
        };
    }

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "kernel_size")  return 31.0f;
        if (name == "iterations")   return 20.0f;
        if (name == "diffuse_coeff" || name == "kernel_growth" || name == "iteration_decay") return 1.0f;
        return Effect::getParameterNominalMax(name);
    }
};
