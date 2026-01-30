#pragma once

#include "CircularBuffer.h"
#include <opencv2/opencv.hpp>
#include <string>

class VideoBuffer {
private:
    std::string videoPath;
    cv::VideoCapture videoCapture;
    std::vector<cv::Mat> buffer;
    size_t bufferLength;
    size_t currentIndex;
    int totalFrames;
    int globalFrameIndex;
    bool looping;
    
    void loadNextBuffer();
    
public:
    VideoBuffer(const std::string& path, size_t bufferSize = 100);
    ~VideoBuffer();
    
    const cv::Mat& getFrame();
    void setLooping(bool loop);
    bool hasMoreFrames() const;
    int getTotalFrames() const;
    int getCurrentFrameIndex() const;
    void reset();
};
