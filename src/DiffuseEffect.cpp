#include "DiffuseEffect.h"
#include <iostream>
#include <algorithm>
#include <cmath>

DiffuseEffect::DiffuseEffect() : Effect("Diffuse") {
    setParameter("diffuse_coeff", 0.1f);
    setParameter("iterations", 1.0f);
    setParameter("kernel_size", 3.0f);
    setParameter("kernel_growth", 0.0f);
    setParameter("iteration_decay", 1.0f);
    setParameter("audio_gain", 1.0f);
}

cv::Mat DiffuseEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    float diffuseCoeff = getParameter("diffuse_coeff", 0.1f);
    int iterations = static_cast<int>(getParameter("iterations", 1.0f));
    int kernelSize = static_cast<int>(getParameter("kernel_size", 3.0f));
    int kernelGrowth = static_cast<int>(getParameter("kernel_growth", 0.0f));
    float iterationDecay = getParameter("iteration_decay", 1.0f);
    float audioGain = getParameter("audio_gain", 1.0f);

    iterations = std::max(1, std::min(iterations, 50));
    kernelSize = std::max(1, std::min(kernelSize, 31));
    if ((kernelSize % 2) == 0) {
        kernelSize += 1;
    }
    kernelGrowth = std::max(0, std::min(kernelGrowth, 5));
    iterationDecay = std::max(0.0f, std::min(iterationDecay, 2.0f));
    
    // Modulate with audio RMS if available
    if (audioBuffer) {
        int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
        std::vector<float> audioSamples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
        float rms = audioBuffer->getRMS(audioSamples);
        float audioScale = (1.0f - audioGain) + (audioGain * rms);
        diffuseCoeff *= audioScale;
    }
    
    cv::Mat newFrame;
    frame.convertTo(newFrame, CV_32FC3);
    
    // Use box filter (moving average) for diffusion - much faster than pixel loops
    // This approximates the diffusion equation efficiently
    for (int iter = 0; iter < iterations; ++iter) {
        cv::Mat blurred;
        int currentKernelSize = kernelSize + (iter * kernelGrowth * 2);
        currentKernelSize = std::max(1, std::min(currentKernelSize, 31));
        if ((currentKernelSize % 2) == 0) {
            currentKernelSize += 1;
        }

        cv::boxFilter(
            newFrame,
            blurred,
            -1,
            cv::Size(currentKernelSize, currentKernelSize),
            cv::Point(-1, -1),
            true,
            cv::BORDER_REPLICATE
        );

        float iterCoeff = diffuseCoeff * std::pow(iterationDecay, static_cast<float>(iter));
        iterCoeff = std::max(0.0f, std::min(iterCoeff, 1.0f));
        
        // Blend current frame with blurred version based on diffuse coefficient
        cv::addWeighted(newFrame, 1.0f - iterCoeff, blurred, iterCoeff, 0.0, newFrame);
    }
    
    cv::Mat result;
    newFrame.convertTo(result, CV_8UC3);
    
    return result;
}
