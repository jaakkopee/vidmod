#include "AudioColorEffect.h"
#include <iostream>

AudioColorEffect::AudioColorEffect() : Effect("AudioColor") {
    setParameter("color_coeff", 1.0f);
}

cv::Mat AudioColorEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    if (!audioBuffer) return frame.clone();
    
    std::cout << "Applying AudioColor effect..." << std::endl;
    
    int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
    std::vector<float> audioSamples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
    
    float rms = audioBuffer->getRMS(audioSamples);
    float colorCoeff = getParameter("color_coeff", 1.0f) * rms;
    
    std::cout << "RMS: " << rms << ", Color coefficient: " << colorCoeff << std::endl;
    
    // Get FFT of audio split into 3 bands
    std::vector<float> bass, mid, treble;
    audioBuffer->getFFT(audioSamples, bass, mid, treble);
    
    // Calculate mean values for each band
    float bassAvg = 0.0f, midAvg = 0.0f, trebleAvg = 0.0f;
    if (!bass.empty()) {
        for (float val : bass) bassAvg += val;
        bassAvg /= bass.size();
    }
    if (!mid.empty()) {
        for (float val : mid) midAvg += val;
        midAvg /= mid.size();
    }
    if (!treble.empty()) {
        for (float val : treble) trebleAvg += val;
        trebleAvg /= treble.size();
    }
    
    // Convert frame to float
    cv::Mat frameFloat;
    frame.convertTo(frameFloat, CV_32FC3);
    
    std::vector<cv::Mat> channels;
    cv::split(frameFloat, channels);
    
    // Apply audio reactive color modulation (BGR order in OpenCV)
    channels[0] *= (bassAvg * colorCoeff);   // Blue (bass)
    channels[1] *= (midAvg * colorCoeff);    // Green (mid)
    channels[2] *= (trebleAvg * colorCoeff); // Red (treble)
    
    // Merge channels
    cv::Mat result;
    cv::merge(channels, result);
    
    // Convert back to uint8
    cv::Mat output;
    result.convertTo(output, CV_8UC3);
    
    return output;
}
