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
};
