#pragma once

#include "Effect.h"

class ShadowEffect : public Effect {
private:
    cv::Mat findLocalMinima(const cv::Mat& frame, int x, int y);

public:
    ShadowEffect();
    
    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;
    
    std::vector<std::string> getParameterNames() const override {
        return {"shadow_coeff", "kernel_size", "morph_iterations", "audio_gain"};
    }
};
