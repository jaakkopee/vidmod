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

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "cells_x" || name == "cells_y") return 64.0f;
        if (name == "radius_scale" || name == "edge_softness" || name == "blend") return 1.0f;
        return Effect::getParameterNominalMax(name);
    }
};
