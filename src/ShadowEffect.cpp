#include "ShadowEffect.h"
#include <iostream>
#include <algorithm>

ShadowEffect::ShadowEffect() : Effect("Shadow") {
    setParameter("shadow_coeff", 0.1f);
    setParameter("kernel_size", 3.0f);
    setParameter("morph_iterations", 1.0f);
    setParameter("audio_gain", 1.0f);
}

cv::Mat ShadowEffect::findLocalMinima(const cv::Mat& frame, int x, int y) {
    // Kept for compatibility but not used in optimized version
    cv::Vec3b minima = frame.at<cv::Vec3b>(y, x);
    cv::Mat result(1, 1, CV_8UC3);
    result.at<cv::Vec3b>(0, 0) = minima;
    return result;
}

cv::Mat ShadowEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    float shadowCoeff = getParameter("shadow_coeff", 0.1f);
    int kernelSize = static_cast<int>(getParameter("kernel_size", 3.0f));
    int morphIterations = static_cast<int>(getParameter("morph_iterations", 1.0f));
    float audioGain = getParameter("audio_gain", 1.0f);

    kernelSize = std::max(1, std::min(kernelSize, 31));
    if ((kernelSize % 2) == 0) {
        kernelSize += 1;
    }
    morphIterations = std::max(1, std::min(morphIterations, 10));
    
    // Modulate with audio RMS if available
    if (audioBuffer) {
        int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
        std::vector<float> audioSamples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
        float rms = audioBuffer->getRMS(audioSamples);
        float audioScale = (1.0f - audioGain) + (audioGain * rms);
        shadowCoeff *= audioScale;
    }
    
    // Use morphological erosion to find local minima (much faster than pixel loops)
    cv::Mat minima;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kernelSize, kernelSize));
    cv::erode(frame, minima, kernel, cv::Point(-1, -1), morphIterations);
    
    // Convert to float for blending
    cv::Mat frameFloat, minimaFloat;
    frame.convertTo(frameFloat, CV_32FC3);
    minima.convertTo(minimaFloat, CV_32FC3);
    
    // Blend using addWeighted (optimized operation)
    cv::Mat result;
    cv::addWeighted(frameFloat, 1.0f - shadowCoeff, minimaFloat, shadowCoeff, 0.0, result);
    
    // Convert back to uint8
    cv::Mat output;
    result.convertTo(output, CV_8UC3);
    
    return output;
}
