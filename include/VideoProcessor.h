#pragma once

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include "VideoBuffer.h"
#include "AudioBuffer.h"
#include "EffectChain.h"
#include <memory>

class VideoProcessor {
private:
    std::string videoPath;
    std::string audioPath;
    std::unique_ptr<VideoBuffer> videoBuffer;
    std::unique_ptr<AudioBuffer> audioBuffer;
    float fps;
    int width;
    int height;
    int totalFrames;
    int currentFrame;
    
public:
    VideoProcessor();
    
    bool loadVideo(const std::string& videoFile);
    bool loadAudio(const std::string& audioFile);
    
    cv::Mat getNextFrame();
    void reset();
    
    float getFPS() const { return fps; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getTotalFrames() const { return totalFrames; }
    int getCurrentFrame() const { return currentFrame; }
    
    AudioBuffer* getAudioBuffer() { return audioBuffer.get(); }
    
    bool saveProcessedVideo(const std::string& outputPath, EffectChain& effectChain);
    bool processImageLoop(const std::string& imagePath, const std::string& outputPath, 
                          EffectChain& effectChain, float durationSeconds, float targetFPS = 30.0f);
};
