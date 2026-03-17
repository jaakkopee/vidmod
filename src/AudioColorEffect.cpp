#include "AudioColorEffect.h"
#include <iostream>
#include <numeric>

AudioColorEffect::AudioColorEffect() : Effect("AudioColor") {
    setParameter("color_coeff", 1.0f);
    setParameter("mode", 0.0f); // 0=RGB, 1=HSV dominant, 2=HSV spectrum
    setParameter("hue_strength", 1.0f);
    setParameter("saturation_strength", 1.0f);
    setParameter("value_strength", 1.0f);
    setParameter("audio_gain", 1.0f);
}

cv::Mat AudioColorEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    if (!audioBuffer) return frame.clone();
    
    int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
    std::vector<float> audioSamples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
    
    float rms = audioBuffer->getRMS(audioSamples);
    float colorCoeff = getParameter("color_coeff", 1.0f);
    int mode = static_cast<int>(getParameter("mode", 0.0f));
    float hueStrength = getParameter("hue_strength", 1.0f);
    float saturationStrength = getParameter("saturation_strength", 1.0f);
    float valueStrength = getParameter("value_strength", 1.0f);
    float audioGain = getParameter("audio_gain", 1.0f);
    float scaledRms = rms * audioGain;
    
    // Get FFT of audio split into 8 bands
    std::vector<std::vector<float>> bands;
    audioBuffer->get8ChannelFFT(audioSamples, bands);
    
    // Calculate mean values for each of the 8 bands
    std::vector<float> bandAvgs(8, 0.0f);
    for (int i = 0; i < 8; ++i) {
        if (!bands[i].empty()) {
            bandAvgs[i] = std::accumulate(bands[i].begin(), bands[i].end(), 0.0f) / bands[i].size();
        }
    }
    
    if (mode == 1) {
        // HSV Mode: Map frequency bands to hue spectrum
        cv::Mat hsvFrame;
        cv::cvtColor(frame, hsvFrame, cv::COLOR_BGR2HSV);
        
        // Calculate dominant frequency for hue rotation
        float dominantBand = 0.0f;
        float maxBand = 0.0f;
        for (int i = 0; i < 8; ++i) {
            if (bandAvgs[i] > maxBand) {
                maxBand = bandAvgs[i];
                dominantBand = static_cast<float>(i);
            }
        }
        
        // Map bands to hue (0-180 in OpenCV HSV)
        float hueShift = (dominantBand / 8.0f) * 180.0f * colorCoeff * audioGain * hueStrength;
        float saturationMod = 1.0f + (scaledRms * colorCoeff * 2.0f * saturationStrength);
        float valueMod = 1.0f + (scaledRms * colorCoeff * valueStrength);
        
        // Process in-place for better cache performance
        hsvFrame.convertTo(hsvFrame, CV_32FC3);
        std::vector<cv::Mat> hsvChannels;
        cv::split(hsvFrame, hsvChannels);
        
        hsvChannels[0] += hueShift;
        hsvChannels[1] *= saturationMod;
        hsvChannels[2] *= valueMod;
        
        // Clamp HSV values
        cv::threshold(hsvChannels[0], hsvChannels[0], 180, 180, cv::THRESH_TRUNC);
        cv::threshold(hsvChannels[1], hsvChannels[1], 255, 255, cv::THRESH_TRUNC);
        cv::threshold(hsvChannels[2], hsvChannels[2], 255, 255, cv::THRESH_TRUNC);
        
        cv::merge(hsvChannels, hsvFrame);
        hsvFrame.convertTo(hsvFrame, CV_8UC3);
        
        cv::Mat output;
        cv::cvtColor(hsvFrame, output, cv::COLOR_HSV2BGR);
        
        return output;
    } else if (mode == 2) {
        // HSV Spectrum Mode: Each band contributes to hue across the spectrum
        cv::Mat hsvFrame;
        cv::cvtColor(frame, hsvFrame, cv::COLOR_BGR2HSV);
        
        // Calculate weighted hue based on all 8 bands
        // Each band maps to a portion of the hue spectrum (0-180)
        float weightedHue = 0.0f;
        float totalWeight = 0.0f;
        
        for (int i = 0; i < 8; ++i) {
            float hueValue = (static_cast<float>(i) / 8.0f) * 180.0f;
            weightedHue += hueValue * bandAvgs[i];
            totalWeight += bandAvgs[i];
        }
        
        if (totalWeight > 0.0f) {
            weightedHue /= totalWeight;
        }
        
        float saturationMod = 1.0f + (scaledRms * colorCoeff * 3.0f * saturationStrength);
        float peakEnergy = *std::max_element(bandAvgs.begin(), bandAvgs.end());
        float valueMod = 1.0f + (peakEnergy * colorCoeff * 2.0f * audioGain * valueStrength);
        
        // Process in-place
        hsvFrame.convertTo(hsvFrame, CV_32FC3);
        std::vector<cv::Mat> hsvChannels;
        cv::split(hsvFrame, hsvChannels);
        
        hsvChannels[0] += weightedHue * colorCoeff * audioGain * hueStrength;
        hsvChannels[1] *= saturationMod;
        hsvChannels[2] *= valueMod;
        
        // Clamp HSV values
        cv::threshold(hsvChannels[0], hsvChannels[0], 180, 180, cv::THRESH_TRUNC);
        cv::threshold(hsvChannels[1], hsvChannels[1], 255, 255, cv::THRESH_TRUNC);
        cv::threshold(hsvChannels[2], hsvChannels[2], 255, 255, cv::THRESH_TRUNC);
        
        cv::merge(hsvChannels, hsvFrame);
        hsvFrame.convertTo(hsvFrame, CV_8UC3);
        
        cv::Mat output;
        cv::cvtColor(hsvFrame, output, cv::COLOR_HSV2BGR);
        
        return output;
    } else {
        // RGB Mode: Map 8 bands to RGB channels
        // Bands 0-2 (sub-bass, bass, low-mid) → Red
        // Bands 3-5 (mid, high-mid) → Green  
        // Bands 6-7 (treble, high-treble) → Blue
        
        float redEnergy = (bandAvgs[0] + bandAvgs[1] + bandAvgs[2]) / 3.0f;
        float greenEnergy = (bandAvgs[3] + bandAvgs[4] + bandAvgs[5]) / 3.0f;
        float blueEnergy = (bandAvgs[6] + bandAvgs[7]) / 2.0f;
        redEnergy *= audioGain;
        greenEnergy *= audioGain;
        blueEnergy *= audioGain;
        
        // Apply audio reactive color modulation (BGR order in OpenCV)
        float blueMod = 1.0f + (blueEnergy * colorCoeff * saturationStrength);
        float greenMod = 1.0f + (greenEnergy * colorCoeff * valueStrength);
        float redMod = 1.0f + (redEnergy * colorCoeff * hueStrength);
        
        // Use cv::Scalar multiplication for efficient per-channel scaling
        cv::Mat frameFloat;
        frame.convertTo(frameFloat, CV_32FC3);
        
        std::vector<cv::Mat> channels;
        cv::split(frameFloat, channels);
        
        channels[0] *= blueMod;   // Blue (high frequencies)
        channels[1] *= greenMod;  // Green (mid frequencies)
        channels[2] *= redMod;    // Red (low frequencies)
        
        cv::merge(channels, frameFloat);
        
        // Clamp and convert back to uint8
        cv::Mat output;
        frameFloat.convertTo(output, CV_8UC3);
        
        return output;
    }
}
