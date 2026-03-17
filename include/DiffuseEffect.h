#pragma once

#include "Effect.h"

class DiffuseEffect : public Effect {
public:
    DiffuseEffect();
    
    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;
    
    std::vector<std::string> getParameterNames() const override {
        return {"diffuse_coeff", "iterations", "audio_gain"};
    }
};
