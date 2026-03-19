#ifndef FRACTALEFFECT_H
#define FRACTALEFFECT_H

#include "Effect.h"
#include <opencv2/opencv.hpp>

class FractalEffect : public Effect {
public:
    FractalEffect();
    cv::Mat apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) override;
    
    std::vector<std::string> getParameterNames() const override {
        return {
            "zoom",
            "center_x",
            "center_y",
            "max_iter",
            "audio_depth",
            "cx",
            "cy",
            "blend",
            "input_warp",
            "color_source_mix",
            "audio_gain"
        };
    }

    float getParameterNominalMax(const std::string& name) const override {
        if (name == "max_iter")    return 500.0f;
        if (name == "audio_depth") return 100.0f;
        if (name == "center_x" || name == "center_y") return 2.0f;  // complex plane extent
        if (name == "cx" || name == "cy") return 2.0f;              // Julia constant range
        if (name == "blend" || name == "input_warp" || name == "color_source_mix") return 1.0f;
        return Effect::getParameterNominalMax(name);
    }

private:
    // Julia set iteration: z = z^2 + c
    int juliaIteration(double x, double y, double cReal, double cImag, int maxIter);
};

#endif
