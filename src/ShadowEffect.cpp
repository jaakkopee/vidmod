#include "ShadowEffect.h"
#include <iostream>

ShadowEffect::ShadowEffect() : Effect("Shadow") {
    setParameter("shadow_coeff", 0.1f);
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
    
    // Modulate with audio RMS if available
    if (audioBuffer) {
        int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
        std::vector<float> audioSamples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
        float rms = audioBuffer->getRMS(audioSamples);
        shadowCoeff *= rms;
    }
    
    // Use morphological erosion to find local minima (much faster than pixel loops)
    cv::Mat minima;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::erode(frame, minima, kernel);
    
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
