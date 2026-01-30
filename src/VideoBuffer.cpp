#include "VideoBuffer.h"
#include <iostream>

VideoBuffer::VideoBuffer(const std::string& path, size_t bufferSize)
    : videoPath(path), bufferLength(bufferSize), currentIndex(0), 
      globalFrameIndex(0), looping(true) {
    
    videoCapture.open(videoPath);
    if (!videoCapture.isOpened()) {
        std::cerr << "Error: Could not open video file: " << videoPath << std::endl;
        totalFrames = 0;
        return;
    }
    
    totalFrames = static_cast<int>(videoCapture.get(cv::CAP_PROP_FRAME_COUNT));
    std::cout << "VideoBuffer initialized: " << totalFrames << " total frames, buffer size: " << bufferLength << std::endl;
    
    // Load initial buffer
    loadNextBuffer();
}

VideoBuffer::~VideoBuffer() {
    if (videoCapture.isOpened()) {
        videoCapture.release();
    }
}

void VideoBuffer::loadNextBuffer() {
    buffer.clear();
    buffer.reserve(bufferLength);
    
    cv::Mat frame;
    size_t framesLoaded = 0;
    
    while (framesLoaded < bufferLength && videoCapture.read(frame)) {
        buffer.push_back(frame.clone());
        framesLoaded++;
    }
    
    currentIndex = 0;
    
    std::cout << "Loaded buffer: " << framesLoaded << " frames (global frame " 
              << globalFrameIndex << ")" << std::endl;
    
    // If we reached end and looping is enabled, rewind
    if (buffer.empty() && looping && totalFrames > 0) {
        std::cout << "Rewinding video for loop..." << std::endl;
        videoCapture.set(cv::CAP_PROP_POS_FRAMES, 0);
        globalFrameIndex = 0;
        
        // Try loading again
        while (framesLoaded < bufferLength && videoCapture.read(frame)) {
            buffer.push_back(frame.clone());
            framesLoaded++;
        }
        currentIndex = 0;
    }
}

const cv::Mat& VideoBuffer::getFrame() {
    // Check if we need to load next buffer
    if (currentIndex >= buffer.size()) {
        loadNextBuffer();
        
        // If still empty after loading, return empty frame
        if (buffer.empty()) {
            static cv::Mat emptyFrame;
            return emptyFrame;
        }
    }
    
    const cv::Mat& frame = buffer[currentIndex];
    currentIndex++;
    globalFrameIndex++;
    
    return frame;
}

void VideoBuffer::setLooping(bool loop) {
    looping = loop;
}

bool VideoBuffer::hasMoreFrames() const {
    if (looping) {
        return true; // Always has more frames when looping
    }
    return currentIndex < buffer.size() || videoCapture.isOpened();
}

int VideoBuffer::getTotalFrames() const {
    return totalFrames;
}

int VideoBuffer::getCurrentFrameIndex() const {
    return globalFrameIndex;
}

void VideoBuffer::reset() {
    if (videoCapture.isOpened()) {
        videoCapture.set(cv::CAP_PROP_POS_FRAMES, 0);
    }
    globalFrameIndex = 0;
    currentIndex = 0;
    buffer.clear();
    loadNextBuffer();
}
