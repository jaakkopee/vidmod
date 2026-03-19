#pragma once

#include "Effect.h"
#include <vector>
#include <random>

class MoldTrailsEffect : public Effect {
public:
    MoldTrailsEffect();

    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;

    std::vector<std::string> getParameterNames() const override {
        return {
            "agent_count",
            "sensor_angle",
            "sensor_dist",
            "turn_speed",
            "move_speed",
            "decay_rate",
            "blur_size",
            "deposit_amount",
            "blend",
            "tint_r",
            "tint_g",
            "tint_b",
            "audio_gain",
            "sim_scale"
        };
    }

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "tint_r" || name == "tint_g" || name == "tint_b") return 255.0f;
        if (name == "agent_count")   return 10000.0f;
        if (name == "sensor_angle")  return 90.0f;    // degrees
        if (name == "sensor_dist")   return 50.0f;    // sim-space pixels
        if (name == "move_speed")    return 10.0f;
        if (name == "blur_size")     return 31.0f;
        if (name == "blend" || name == "decay_rate" || name == "deposit_amount" ||
            name == "turn_speed"    || name == "sim_scale") return 1.0f;
        return Effect::getParameterNominalMax(name);
    }

private:
    struct Agent { float x, y, angle; };

    cv::Mat            m_trailMap;
    std::vector<Agent> m_agents;
    std::mt19937       m_rng;
    cv::Size           m_frameSize;
    int                m_simW{0}, m_simH{0};
    int                m_agentCount{0};
    int                m_frameCounter{0};
    float              m_smoothedAudioEnergy{0.0f};
    bool               m_initialized{false};

    void  initAgents(int count, int w, int h, const cv::Mat& grayLuma);
    float sampleTrail(float x, float y) const;
    void  depositTrail(float x, float y, float amount);
};
