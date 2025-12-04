#include "VideoBuffer.h"

VideoBuffer::VideoBuffer(const std::vector<cv::Mat>& frames) 
    : CircularBuffer<cv::Mat>(frames) {}

const cv::Mat& VideoBuffer::getFrame() {
    return getNext();
}
