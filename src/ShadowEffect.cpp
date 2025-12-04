#include "ShadowEffect.h"
#include <iostream>

ShadowEffect::ShadowEffect() : Effect("Shadow") {
    setParameter("shadow_coeff", 0.1f);
}

cv::Mat ShadowEffect::findLocalMinima(const cv::Mat& frame, int x, int y) {
    cv::Vec3b minima = frame.at<cv::Vec3b>(y, x);
    
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            if (i == 0 && j == 0) continue;
            
            int nx = x + i;
            int ny = y + j;
            
            if (nx >= 0 && nx < frame.cols && ny >= 0 && ny < frame.rows) {
                cv::Vec3b pixel = frame.at<cv::Vec3b>(ny, nx);
                for (int c = 0; c < 3; ++c) {
                    if (pixel[c] < minima[c]) {
                        minima[c] = pixel[c];
                    }
                }
            }
        }
    }
    
    cv::Mat result(1, 1, CV_8UC3);
    result.at<cv::Vec3b>(0, 0) = minima;
    return result;
}

cv::Mat ShadowEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    std::cout << "Applying Shadow effect..." << std::endl;
    
    float shadowCoeff = getParameter("shadow_coeff", 0.1f);
    
    // Modulate with audio RMS if available
    if (audioBuffer) {
        int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
        std::vector<float> audioSamples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
        float rms = audioBuffer->getRMS(audioSamples);
        shadowCoeff *= rms;
        std::cout << "Audio RMS: " << rms << ", Shadow coefficient: " << shadowCoeff << std::endl;
    }
    
    cv::Mat shadow = cv::Mat::zeros(frame.size(), CV_32FC3);
    cv::Mat frameFloat;
    frame.convertTo(frameFloat, CV_32FC3);
    
    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            cv::Mat minima = findLocalMinima(frame, x, y);
            cv::Vec3b minimaVec = minima.at<cv::Vec3b>(0, 0);
            cv::Vec3f minimaFloat(minimaVec[0], minimaVec[1], minimaVec[2]);
            cv::Vec3f shadowVal = shadow.at<cv::Vec3f>(y, x);
            
            for (int c = 0; c < 3; ++c) {
                shadowVal[c] = shadowVal[c] + (minimaFloat[c] - shadowVal[c]) * shadowCoeff;
            }
            
            shadow.at<cv::Vec3f>(y, x) = shadowVal;
        }
    }
    
    cv::Mat result;
    shadow.convertTo(result, CV_8UC3);
    
    return result;
}
