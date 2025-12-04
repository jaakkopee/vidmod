#include "VideoProcessor.h"
#include <sndfile.h>
#include <iostream>

VideoProcessor::VideoProcessor() 
    : fps(30.0f), width(0), height(0), totalFrames(0), currentFrame(0) {}

bool VideoProcessor::loadVideo(const std::string& videoFile) {
    videoPath = videoFile;
    
    cv::VideoCapture cap(videoFile);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open video file: " << videoFile << std::endl;
        return false;
    }
    
    fps = cap.get(cv::CAP_PROP_FPS);
    width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    totalFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    
    std::cout << "Loading video: " << videoFile << std::endl;
    std::cout << "FPS: " << fps << ", Size: " << width << "x" << height << ", Frames: " << totalFrames << std::endl;
    
    // Load all frames into memory
    std::vector<cv::Mat> frames;
    frames.reserve(totalFrames);
    
    cv::Mat frame;
    while (cap.read(frame)) {
        frames.push_back(frame.clone());
    }
    
    cap.release();
    
    videoBuffer = std::make_unique<VideoBuffer>(frames);
    currentFrame = 0;
    
    std::cout << "Video loaded successfully!" << std::endl;
    return true;
}

bool VideoProcessor::loadAudio(const std::string& audioFile) {
    audioPath = audioFile;
    
    SF_INFO sfInfo;
    SNDFILE* sndFile = sf_open(audioFile.c_str(), SFM_READ, &sfInfo);
    
    if (!sndFile) {
        std::cerr << "Error: Could not open audio file: " << audioFile << std::endl;
        return false;
    }
    
    std::cout << "Loading audio: " << audioFile << std::endl;
    std::cout << "Sample rate: " << sfInfo.samplerate << ", Channels: " << sfInfo.channels << std::endl;
    
    // Read audio data
    std::vector<float> audioData(sfInfo.frames * sfInfo.channels);
    sf_count_t count = sf_readf_float(sndFile, audioData.data(), sfInfo.frames);
    
    sf_close(sndFile);
    
    // Convert to mono if stereo
    std::vector<float> monoAudio;
    if (sfInfo.channels > 1) {
        monoAudio.reserve(count);
        for (sf_count_t i = 0; i < count; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < sfInfo.channels; ++ch) {
                sum += audioData[i * sfInfo.channels + ch];
            }
            monoAudio.push_back(sum / sfInfo.channels);
        }
    } else {
        monoAudio = std::move(audioData);
    }
    
    audioBuffer = std::make_unique<AudioBuffer>(monoAudio, sfInfo.samplerate);
    
    std::cout << "Audio loaded successfully!" << std::endl;
    return true;
}

cv::Mat VideoProcessor::getNextFrame() {
    if (!videoBuffer) {
        return cv::Mat();
    }
    
    currentFrame++;
    return videoBuffer->getFrame();
}

void VideoProcessor::reset() {
    currentFrame = 0;
    if (videoBuffer) {
        videoBuffer->setIndex(0);
    }
    if (audioBuffer) {
        audioBuffer->setIndex(0);
    }
}

bool VideoProcessor::saveProcessedVideo(const std::string& outputPath, EffectChain& effectChain) {
    if (!videoBuffer) {
        std::cerr << "Error: No video loaded!" << std::endl;
        return false;
    }
    
    reset();
    
    // Create video writer
    int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    cv::VideoWriter writer(outputPath, fourcc, fps, cv::Size(width, height));
    
    if (!writer.isOpened()) {
        std::cerr << "Error: Could not open video writer for: " << outputPath << std::endl;
        return false;
    }
    
    std::cout << "Processing video..." << std::endl;
    
    for (int i = 0; i < totalFrames; ++i) {
        cv::Mat frame = getNextFrame();
        
        std::cout << "Processing frame " << (i + 1) << " of " << totalFrames << std::endl;
        
        cv::Mat processedFrame = effectChain.applyEffects(frame, audioBuffer.get(), fps);
        
        writer.write(processedFrame);
    }
    
    writer.release();
    
    std::cout << "Video saved to: " << outputPath << std::endl;
    return true;
}

bool VideoProcessor::processImageLoop(const std::string& imagePath, const std::string& outputPath, 
                                       EffectChain& effectChain, float durationSeconds, float targetFPS) {
    // Load the image
    cv::Mat image = cv::imread(imagePath);
    if (image.empty()) {
        std::cerr << "Error: Could not load image: " << imagePath << std::endl;
        return false;
    }
    
    std::cout << "Loaded image: " << imagePath << std::endl;
    std::cout << "Image size: " << image.cols << "x" << image.rows << std::endl;
    
    // Calculate number of frames needed
    int numFrames = static_cast<int>(durationSeconds * targetFPS);
    
    std::cout << "Creating " << numFrames << " frames at " << targetFPS << " FPS" << std::endl;
    std::cout << "Duration: " << durationSeconds << " seconds" << std::endl;
    
    // Set up video processor state (without loading all frames into memory)
    fps = targetFPS;
    width = image.cols;
    height = image.rows;
    totalFrames = numFrames;
    currentFrame = 0;
    
    // Clear video buffer to save memory
    videoBuffer.reset();
    
    // If audio is loaded, synchronize the frame count
    if (audioBuffer) {
        std::cout << "Using loaded audio for modulation" << std::endl;
    } else {
        std::cout << "No audio loaded - effects will use default values" << std::endl;
    }
    
    // Create video writer
    int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    cv::VideoWriter writer(outputPath, fourcc, fps, cv::Size(width, height));
    
    if (!writer.isOpened()) {
        std::cerr << "Error: Could not open video writer for: " << outputPath << std::endl;
        return false;
    }
    
    std::cout << "Processing image loop..." << std::endl;
    
    // Process each frame on-the-fly (without storing all frames in memory)
    for (int i = 0; i < totalFrames; ++i) {
        if (i % 30 == 0) {  // Progress update every 30 frames
            std::cout << "Processing frame " << (i + 1) << " of " << totalFrames 
                      << " (" << (100 * i / totalFrames) << "%)" << std::endl;
        }
        
        // Advance audio buffer to the correct position for this frame
        if (audioBuffer) {
            size_t audioPos = static_cast<size_t>((static_cast<double>(i) / totalFrames) * audioBuffer->size());
            audioBuffer->setIndex(audioPos);
        }
        
        // Clone the image for each frame and apply effects
        cv::Mat frame = image.clone();
        cv::Mat processedFrame = effectChain.applyEffects(frame, audioBuffer.get(), fps);
        
        writer.write(processedFrame);
    }
    
    writer.release();
    
    std::cout << "Image loop video saved to: " << outputPath << std::endl;
    return true;
}
