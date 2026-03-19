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

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "tint_r" || name == "tint_g" || name == "tint_b") return 255.0f;
        if (name == "wolfram_rule") return 255.0f;  // 8-bit rule index
        if (name == "bit_lo")    return 7.0f;        // bit planes 0-7
        if (name == "bit_count") return 6.0f;        // max planes processed
        if (name == "blend" || name == "sim_scale") return 1.0f;
        return Effect::getParameterNominalMax(name);
    }

private:
    float m_audioEnvelope{0.0f};
};
