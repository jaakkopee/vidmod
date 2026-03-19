#pragma once

#include "Effect.h"

class CAGlowEffect : public Effect {
public:
    CAGlowEffect();

    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;

    std::vector<std::string> getParameterNames() const override {
        return {
            "iterations",
            "state_count",
            "neighbor_mix",
            "sim_scale",
            "blur_size",
            "contrast",
            "glow_strength",
            "blend",
            "tint_r",
            "tint_g",
            "tint_b",
            "audio_gain"
        };
    }

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "tint_r" || name == "tint_g" || name == "tint_b") return 255.0f;
        if (name == "blur_size")     return 31.0f;
        if (name == "iterations")    return 20.0f;
        if (name == "state_count")   return 16.0f;
        if (name == "contrast")      return 4.0f;
        if (name == "glow_strength") return 4.0f;
        if (name == "blend" || name == "neighbor_mix" || name == "sim_scale") return 1.0f;
        return Effect::getParameterNominalMax(name);
    }
};
