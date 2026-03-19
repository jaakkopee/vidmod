#pragma once

#include "Effect.h"

class EdgeInkEffect : public Effect {
public:
    EdgeInkEffect();

    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;

    std::vector<std::string> getParameterNames() const override {
        return {
            "edge_threshold",
            "blur_size",
            "edge_strength",
            "invert",
            "ink_r",
            "ink_g",
            "ink_b",
            "blend",
            "audio_gain"
        };
    }

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "ink_r" || name == "ink_g" || name == "ink_b") return 255.0f;
        if (name == "edge_threshold") return 255.0f;
        if (name == "edge_strength") return 4.0f;
        if (name == "blur_size") return 31.0f;
        if (name == "invert") return 1.0f;
        if (name == "blend") return 1.0f;
        if (name == "audio_gain") return 1.0f;
        return Effect::getParameterNominalMax(name);
    }
};
