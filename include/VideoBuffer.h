#pragma once

#include "CircularBuffer.h"
#include <opencv2/opencv.hpp>

class VideoBuffer : public CircularBuffer<cv::Mat> {
public:
    VideoBuffer(const std::vector<cv::Mat>& frames);
    
    const cv::Mat& getFrame();
};
