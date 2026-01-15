#include "FFTEffect.h"
#include <fftw3.h>
#include <algorithm>
#include <iostream>

FFTEffect::FFTEffect() : Effect("FFT") {
    setParameter("fft_r_coeff", 1.0f);
    setParameter("fft_g_coeff", 1.0f);
    setParameter("fft_b_coeff", 1.0f);
    setParameter("red_bias", 64.0f);
    setParameter("green_bias", 64.0f);
    setParameter("blue_bias", 64.0f);
}

cv::Mat FFTEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    // If no audio buffer, just return the original frame
    if (!audioBuffer) {
        return frame.clone();
    }
    
    int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
    std::vector<float> audioSamples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
    
    if (audioSamples.empty()) {
        return frame.clone();
    }
    
    // Apply Hanning window
    for (size_t i = 0; i < audioSamples.size(); ++i) {
        float window = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (audioSamples.size() - 1)));
        audioSamples[i] *= window;
    }
    
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
    
    // Apply FFT modulation to each channel
    float fft_r = getParameter("fft_r_coeff", 1.0f);
    float fft_g = getParameter("fft_g_coeff", 1.0f);
    float fft_b = getParameter("fft_b_coeff", 1.0f);
    
    // Massive scaling factor to make very small FFT values visible
    // FFT values are typically in range 1e-6 to 1e-3, so we need ~1e6 multiplier
    const float fftScale = 1000000.0f;
    
    // Convert frame to float and split channels
    cv::Mat frameFloat;
    frame.convertTo(frameFloat, CV_32FC3);
    
    std::vector<cv::Mat> channels;
    cv::split(frameFloat, channels);
    
    // Add modulation on top of the original image rather than multiplying
    // This way the image stays visible even with low FFT values
    // Mapping: Bass->Red, Mid->Green, Treble->Blue
    channels[0] += (trebleAvg * fft_b * fftScale);  // Blue channel (OpenCV uses BGR)
    channels[1] += (midAvg * fft_g * fftScale);     // Green channel
    channels[2] += (bassAvg * fft_r * fftScale);    // Red channel
    
    // Merge channels back into frameFloat (reuse allocation)
    cv::merge(channels, frameFloat);
    
    // Clip values to 0-255 range and convert back to uint8
    cv::Mat output;
    frameFloat.convertTo(output, CV_8UC3);
    
    return output;
}
