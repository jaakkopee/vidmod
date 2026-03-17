#include "DiffuseEffect.h"
#include <iostream>

DiffuseEffect::DiffuseEffect() : Effect("Diffuse") {
    setParameter("diffuse_coeff", 0.1f);
    setParameter("iterations", 1.0f);
    setParameter("audio_gain", 1.0f);
}

cv::Mat DiffuseEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    float diffuseCoeff = getParameter("diffuse_coeff", 0.1f);
    int iterations = static_cast<int>(getParameter("iterations", 1.0f));
    float audioGain = getParameter("audio_gain", 1.0f);
    
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
        cv::boxFilter(newFrame, blurred, -1, cv::Size(3, 3), cv::Point(-1, -1), true, cv::BORDER_REPLICATE);
        
        // Blend current frame with blurred version based on diffuse coefficient
        cv::addWeighted(newFrame, 1.0f - diffuseCoeff, blurred, diffuseCoeff, 0.0, newFrame);
    }
    
    cv::Mat result;
    newFrame.convertTo(result, CV_8UC3);
    
    return result;
}
