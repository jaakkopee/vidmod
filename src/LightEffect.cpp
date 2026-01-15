#include "LightEffect.h"
#include <iostream>

LightEffect::LightEffect() : Effect("Light") {
    setParameter("light_coeff", 0.1f);
}

cv::Mat LightEffect::findLocalMaxima(const cv::Mat& frame, int x, int y) {
    // Kept for compatibility but not used in optimized version
    cv::Vec3b maxima = frame.at<cv::Vec3b>(y, x);
    cv::Mat result(1, 1, CV_8UC3);
    result.at<cv::Vec3b>(0, 0) = maxima;
    return result;
}

cv::Mat LightEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    std::cout << "Applying Light effect..." << std::endl;
    
    float lightCoeff = getParameter("light_coeff", 0.1f);
    
    // Modulate with audio RMS if available
    if (audioBuffer) {
        int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
        std::vector<float> audioSamples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
        float rms = audioBuffer->getRMS(audioSamples);
        lightCoeff *= rms;
        std::cout << "Audio RMS: " << rms << ", Light coefficient: " << lightCoeff << std::endl;
    }
    
    // Use morphological dilation to find local maxima (much faster than pixel loops)
    cv::Mat maxima;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::dilate(frame, maxima, kernel);
    
    // Convert to float for blending
    cv::Mat frameFloat, maximaFloat;
    frame.convertTo(frameFloat, CV_32FC3);
    maxima.convertTo(maximaFloat, CV_32FC3);
    
    // Blend using addWeighted (optimized operation)
    cv::Mat result;
    cv::addWeighted(frameFloat, 1.0f - lightCoeff, maximaFloat, lightCoeff, 0.0, result);
    
    // Convert back to uint8
    cv::Mat output;
    result.convertTo(output, CV_8UC3);
    
    return output;
}
