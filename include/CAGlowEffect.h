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
};
