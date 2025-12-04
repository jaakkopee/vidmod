#include "DiffuseEffect.h"
#include <iostream>

DiffuseEffect::DiffuseEffect() : Effect("Diffuse") {
    setParameter("diffuse_coeff", 0.1f);
    setParameter("iterations", 1.0f);
}

cv::Mat DiffuseEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    std::cout << "Applying Diffuse effect..." << std::endl;
    
    float diffuseCoeff = getParameter("diffuse_coeff", 0.1f);
    int iterations = static_cast<int>(getParameter("iterations", 1.0f));
    
    // Modulate with audio RMS if available
    if (audioBuffer) {
        int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
        std::vector<float> audioSamples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
        float rms = audioBuffer->getRMS(audioSamples);
        diffuseCoeff *= rms;
        std::cout << "Audio RMS: " << rms << ", Diffuse coefficient: " << diffuseCoeff << std::endl;
    }
    
    cv::Mat newFrame;
    frame.convertTo(newFrame, CV_32FC3);
    
    // Pre-compute neighbor offsets
    std::vector<std::pair<int, int>> offsets;
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            if (i != 0 || j != 0) {
                offsets.push_back({i, j});
            }
        }
    }
    
    for (int iter = 0; iter < iterations; ++iter) {
        std::cout << "Iteration " << iter << std::endl;
        cv::Mat tempFrame = newFrame.clone();
        
        for (int y = 0; y < frame.rows; ++y) {
            for (int x = 0; x < frame.cols; ++x) {
                cv::Vec3f diff(0.0f, 0.0f, 0.0f);
                cv::Vec3f current = tempFrame.at<cv::Vec3f>(y, x);
                
                for (const auto& offset : offsets) {
                    int nx = x + offset.first;
                    int ny = y + offset.second;
                    
                    if (nx >= 0 && nx < frame.cols && ny >= 0 && ny < frame.rows) {
                        cv::Vec3f neighbor = tempFrame.at<cv::Vec3f>(ny, nx);
                        for (int c = 0; c < 3; ++c) {
                            diff[c] += neighbor[c] - current[c];
                        }
                    }
                }
                
                for (int c = 0; c < 3; ++c) {
                    diff[c] *= diffuseCoeff;
                    newFrame.at<cv::Vec3f>(y, x)[c] = current[c] + diff[c];
                }
            }
        }
    }
    
    cv::Mat result;
    newFrame.convertTo(result, CV_8UC3);
    
    return result;
}
