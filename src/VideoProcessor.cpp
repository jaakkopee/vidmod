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
    currentFrame = 0;
    
    cap.release();
    
    std::cout << "Video metadata loaded: " << videoFile << std::endl;
    std::cout << "FPS: " << fps << ", Size: " << width << "x" << height << ", Frames: " << totalFrames << std::endl;
    
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
    if (videoPath.empty()) {
        return cv::Mat();
    }
    
    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        return cv::Mat();
    }
    
    // Seek to current frame
    cap.set(cv::CAP_PROP_POS_FRAMES, currentFrame);
    
    cv::Mat frame;
    if (!cap.read(frame)) {
        return cv::Mat();
    }
    
    currentFrame++;
    cap.release();
    
    return frame;
}

cv::Mat VideoProcessor::getFrameAt(int frameIndex) {
    if (videoPath.empty()) {
        return cv::Mat();
    }
    
    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        return cv::Mat();
    }
    
    // Seek to specific frame
    cap.set(cv::CAP_PROP_POS_FRAMES, frameIndex);
    
    cv::Mat frame;
    if (!cap.read(frame)) {
        return cv::Mat();
    }
    
    cap.release();
    return frame;
}

void VideoProcessor::reset() {
    currentFrame = 0;
    if (audioBuffer) {
        audioBuffer->setIndex(0);
    }
}

void VideoProcessor::setAudioBuffer(AudioBuffer* externalBuffer) {
    // This is a temporary holder - doesn't take ownership
    // We create a copy to maintain ownership semantics
    if (externalBuffer) {
        audioBuffer = std::make_unique<AudioBuffer>(externalBuffer->getData(), externalBuffer->getSampleRate());
        std::cout << "External audio buffer set (" << externalBuffer->size() << " samples)" << std::endl;
    }
}

bool VideoProcessor::saveProcessedVideo(const std::string& outputPath, EffectChain& effectChain, float durationSeconds) {
    return saveProcessedVideo(outputPath, effectChain, audioBuffer.get(), durationSeconds);
}

bool VideoProcessor::saveProcessedVideo(const std::string& outputPath, EffectChain& effectChain, AudioBuffer* externalAudio, float durationSeconds) {
    if (videoPath.empty()) {
        std::cerr << "Error: No video loaded!" << std::endl;
        return false;
    }
    
    // Open video file for reading
    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open video file: " << videoPath << std::endl;
        return false;
    }
    
    // Calculate target frame count based on duration parameter or longer of video/audio
    int targetFrames = totalFrames;
    
    if (durationSeconds > 0) {
        // Use specified duration
        targetFrames = static_cast<int>(durationSeconds * fps);
        std::cout << "Using specified duration: " << durationSeconds << "s, Target frames: " << targetFrames << std::endl;
    } else if (externalAudio) {
        // Use the longer of video or audio duration to allow looping the shorter one
        float audioDuration = static_cast<float>(externalAudio->size()) / externalAudio->getSampleRate();
        int audioFrames = static_cast<int>(audioDuration * fps);
        targetFrames = std::max(totalFrames, audioFrames);
        
        if (totalFrames > audioFrames) {
            std::cout << "Video is longer (" << totalFrames << " frames) than audio (" << audioFrames 
                      << " frames) - audio will loop" << std::endl;
        } else if (audioFrames > totalFrames) {
            std::cout << "Audio is longer (" << audioDuration << "s) than video - video will loop" << std::endl;
        } else {
            std::cout << "Video and audio have matching duration: " << audioDuration << "s" << std::endl;
        }
        std::cout << "Target frames: " << targetFrames << std::endl;
    } else {
        // Use original video length
        std::cout << "No duration specified, using video length: " << targetFrames << " frames" << std::endl;
    }
    
    // Create video writer
    int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    cv::VideoWriter writer(outputPath, fourcc, fps, cv::Size(width, height));
    
    if (!writer.isOpened()) {
        std::cerr << "Error: Could not open video writer for: " << outputPath << std::endl;
        cap.release();
        return false;
    }
    
    std::cout << "Processing video (target: " << targetFrames << " frames)..." << std::endl;
    
    // Reset audio buffer
    if (externalAudio) {
        externalAudio->setIndex(0);
    }
    
    // Process frames - loop video if shorter than audio, or cut if longer
    int frameCount = 0;
    
    while (frameCount < targetFrames) {
        cv::Mat frame;
        
        // Try to read next frame
        if (!cap.read(frame)) {
            // Video ended but audio continues - loop back to start
            cap.set(cv::CAP_PROP_POS_FRAMES, 0);
            if (!cap.read(frame)) {
                std::cerr << "Error: Could not read frame after seeking to start" << std::endl;
                break;
            }
            std::cout << "Video looped at frame " << frameCount << std::endl;
        }
        
        frameCount++;
        
        if (frameCount % 30 == 0 || frameCount == 1 || frameCount == targetFrames) {
            std::cout << "Processing frame " << frameCount << " / " << targetFrames 
                      << " (" << (100 * frameCount / targetFrames) << "%)" << std::endl;
        }
        
        cv::Mat processedFrame = effectChain.applyEffects(frame, externalAudio, fps);
        writer.write(processedFrame);
    }
    
    std::cout << "Completed processing: " << frameCount << " / " << targetFrames << " frames" << std::endl;
    
    cap.release();
    writer.release();
    
    std::cout << "Video saved to: " << outputPath << std::endl;
    
    // Mux audio into the video if available
    if (externalAudio) {
        std::cout << "Muxing audio into video..." << std::endl;
        
        // Save audio to temporary WAV file
        std::string tempAudioPath = outputPath + ".temp_audio.wav";
        if (!externalAudio->saveToWAV(tempAudioPath)) {
            std::cerr << "Warning: Could not save audio to temporary file" << std::endl;
            return true;  // Video saved successfully, but no audio
        }
        
        // Create output path with audio
        std::string videoWithAudioPath = outputPath;
        std::string tempVideoPath = outputPath + ".temp_video.mp4";
        
        // Rename the video-only file to temp
        if (rename(outputPath.c_str(), tempVideoPath.c_str()) != 0) {
            std::cerr << "Warning: Could not rename temporary video file" << std::endl;
            remove(tempAudioPath.c_str());
            return true;
        }
        
        // Build FFmpeg command to mux audio and video
        std::string ffmpegCmd = "ffmpeg -y -i \"" + tempVideoPath + "\" -i \"" + tempAudioPath + 
            "\" -c:v copy -c:a aac -shortest \"" + videoWithAudioPath + "\" 2>&1";
        
        std::cout << "Running FFmpeg: " << ffmpegCmd << std::endl;
        int result = system(ffmpegCmd.c_str());
        
        // Clean up temporary files
        remove(tempVideoPath.c_str());
        remove(tempAudioPath.c_str());
        
        if (result != 0) {
            std::cerr << "Warning: FFmpeg muxing failed. Video saved without audio." << std::endl;
            // Restore the video-only file if FFmpeg failed
            rename(tempVideoPath.c_str(), outputPath.c_str());
        } else {
            std::cout << "Audio successfully muxed into video!" << std::endl;
        }
    }
    
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
    
    // Clear video path since we're processing an image
    videoPath.clear();
    
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
        if (i % 30 == 0 || i == 0 || i == totalFrames - 1) {
            std::cout << "Processing frame " << (i + 1) << " / " << totalFrames 
                      << " (" << (100 * (i + 1) / totalFrames) << "%)" << std::endl;
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
    
    std::cout << "Completed processing: " << totalFrames << " / " << totalFrames << " frames" << std::endl;
    
    writer.release();
    
    std::cout << "Image loop video saved to: " << outputPath << std::endl;
    
    // Mux audio into the video if available
    if (audioBuffer) {
        std::cout << "Muxing audio into video..." << std::endl;
        
        // Save audio to temporary WAV file
        std::string tempAudioPath = outputPath + ".temp_audio.wav";
        if (!audioBuffer->saveToWAV(tempAudioPath)) {
            std::cerr << "Warning: Could not save audio to temporary file" << std::endl;
            return true;  // Video saved successfully, but no audio
        }
        
        // Create output path with audio
        std::string videoWithAudioPath = outputPath;
        std::string tempVideoPath = outputPath + ".temp_video.mp4";
        
        // Rename the video-only file to temp
        if (rename(outputPath.c_str(), tempVideoPath.c_str()) != 0) {
            std::cerr << "Warning: Could not rename temporary video file" << std::endl;
            remove(tempAudioPath.c_str());
            return true;
        }
        
        // Build FFmpeg command to mux audio and video
        std::string ffmpegCmd = "ffmpeg -y -i \"" + tempVideoPath + "\" -i \"" + tempAudioPath + 
            "\" -c:v copy -c:a aac -shortest \"" + videoWithAudioPath + "\" 2>&1";
        
        std::cout << "Running FFmpeg: " << ffmpegCmd << std::endl;
        int result = system(ffmpegCmd.c_str());
        
        // Clean up temporary files
        remove(tempVideoPath.c_str());
        remove(tempAudioPath.c_str());
        
        if (result != 0) {
            std::cerr << "Warning: FFmpeg muxing failed. Video saved without audio." << std::endl;
            // Restore the video-only file if FFmpeg failed
            rename(tempVideoPath.c_str(), outputPath.c_str());
        } else {
            std::cout << "Audio successfully muxed into video!" << std::endl;
        }
    }
    
    return true;
}

bool VideoProcessor::processImageLoop(const std::string& imagePath, const std::string& outputPath, 
                                       EffectChain& effectChain, AudioBuffer* externalAudio, float durationSeconds, float targetFPS) {
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
    
    // Set up video processor state
    fps = targetFPS;
    width = image.cols;
    height = image.rows;
    totalFrames = numFrames;
    currentFrame = 0;
    videoPath.clear();
    
    if (externalAudio) {
        std::cout << "Using external audio for modulation" << std::endl;
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
    
    // Process each frame
    for (int i = 0; i < totalFrames; ++i) {
        if (i % 30 == 0 || i == 0 || i == totalFrames - 1) {
            std::cout << "Processing frame " << (i + 1) << " / " << totalFrames 
                      << " (" << (100 * (i + 1) / totalFrames) << "%)" << std::endl;
        }
        
        // Advance audio buffer to the correct position
        if (externalAudio) {
            size_t audioPos = static_cast<size_t>((static_cast<double>(i) / totalFrames) * externalAudio->size());
            externalAudio->setIndex(audioPos);
        }
        
        // Clone the image and apply effects
        cv::Mat frame = image.clone();
        cv::Mat processedFrame = effectChain.applyEffects(frame, externalAudio, fps);
        
        writer.write(processedFrame);
    }
    
    std::cout << "Completed processing: " << totalFrames << " / " << totalFrames << " frames" << std::endl;
    
    writer.release();
    
    std::cout << "Image loop video saved to: " << outputPath << std::endl;
    
    // Mux audio into the video if available
    if (externalAudio) {
        std::cout << "Muxing audio into video..." << std::endl;
        
        // Save audio to temporary WAV file
        std::string tempAudioPath = outputPath + ".temp_audio.wav";
        if (!externalAudio->saveToWAV(tempAudioPath)) {
            std::cerr << "Warning: Could not save audio to temporary file" << std::endl;
            return true;  // Video saved successfully, but no audio
        }
        
        // Create output path with audio
        std::string videoWithAudioPath = outputPath;
        std::string tempVideoPath = outputPath + ".temp_video.mp4";
        
        // Rename the video-only file to temp
        if (rename(outputPath.c_str(), tempVideoPath.c_str()) != 0) {
            std::cerr << "Warning: Could not rename temporary video file" << std::endl;
            remove(tempAudioPath.c_str());
            return true;
        }
        
        // Build FFmpeg command to mux audio and video
        std::string ffmpegCmd = "ffmpeg -y -i \"" + tempVideoPath + "\" -i \"" + tempAudioPath + 
            "\" -c:v copy -c:a aac -shortest \"" + videoWithAudioPath + "\" 2>&1";
        
        std::cout << "Running FFmpeg: " << ffmpegCmd << std::endl;
        int result = system(ffmpegCmd.c_str());
        
        // Clean up temporary files
        remove(tempVideoPath.c_str());
        remove(tempAudioPath.c_str());
        
        if (result != 0) {
            std::cerr << "Warning: FFmpeg muxing failed. Video saved without audio." << std::endl;
            // Restore the video-only file if FFmpeg failed
            rename(tempVideoPath.c_str(), outputPath.c_str());
        } else {
            std::cout << "Audio successfully muxed into video!" << std::endl;
        }
    }
    
    return true;
}
