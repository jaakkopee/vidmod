#pragma once

#include "Effect.h"

class CircleQuiltEffect : public Effect {
public:
    CircleQuiltEffect();

    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;

    std::vector<std::string> getParameterNames() const override {
        return {
            "cells_x",
            "cells_y",
            "radius_scale",
            "edge_softness",
            "blend",
            "audio_gain"
        };
    }
};
