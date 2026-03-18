#pragma once

#include "Effect.h"

class BitplaneReactorEffect : public Effect {
public:
    BitplaneReactorEffect();

    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;

    std::vector<std::string> getParameterNames() const override {
        return {
            "bit_lo",
            "bit_count",
            "wolfram_rule",
            "sim_scale",
            "blend",
            "tint_r",
            "tint_g",
            "tint_b",
            "audio_gain"
        };
    }

private:
    float m_audioEnvelope{0.0f};
};
