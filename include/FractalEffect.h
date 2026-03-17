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

private:
    // Julia set iteration: z = z^2 + c
    int juliaIteration(double x, double y, double cReal, double cImag, int maxIter);
};

#endif
