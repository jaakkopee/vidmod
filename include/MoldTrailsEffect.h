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
