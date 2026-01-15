#include "AudioColorEffect.h"
#include <iostream>
#include <numeric>

AudioColorEffect::AudioColorEffect() : Effect("AudioColor") {
    setParameter("color_coeff", 1.0f);
    setParameter("mode", 0.0f); // 0=RGB, 1=HSV dominant, 2=HSV spectrum
}

cv::Mat AudioColorEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    if (!audioBuffer) return frame.clone();
    
    std::cout << "Applying AudioColor effect..." << std::endl;
    
    int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
    std::vector<float> audioSamples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
    
    float rms = audioBuffer->getRMS(audioSamples);
    float colorCoeff = getParameter("color_coeff", 1.0f);
    int mode = static_cast<int>(getParameter("mode", 0.0f));
    
    std::cout << "RMS: " << rms << ", Color coefficient: " << colorCoeff << ", Mode: " << mode << std::endl;
    
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
        hsvFrame.convertTo(hsvFrame, CV_32FC3);
        
        std::vector<cv::Mat> hsvChannels;
        cv::split(hsvFrame, hsvChannels);
        
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
        float hueShift = (dominantBand / 8.0f) * 180.0f * colorCoeff;
        hsvChannels[0] += hueShift;
        
        // Modulate saturation with overall energy
        float saturationMod = 1.0f + (rms * colorCoeff * 2.0f);
        hsvChannels[1] *= saturationMod;
        
        // Modulate value/brightness with RMS
        float valueMod = 1.0f + (rms * colorCoeff);
        hsvChannels[2] *= valueMod;
        
        // Clamp HSV values
        cv::threshold(hsvChannels[0], hsvChannels[0], 180, 180, cv::THRESH_TRUNC);
        cv::threshold(hsvChannels[1], hsvChannels[1], 255, 255, cv::THRESH_TRUNC);
        cv::threshold(hsvChannels[2], hsvChannels[2], 255, 255, cv::THRESH_TRUNC);
        
        cv::Mat result;
        cv::merge(hsvChannels, result);
        result.convertTo(result, CV_8UC3);
        
        cv::Mat output;
        cv::cvtColor(result, output, cv::COLOR_HSV2BGR);
        
        return output;
    } else if (mode == 2) {
        // HSV Spectrum Mode: Each band contributes to hue across the spectrum
        cv::Mat hsvFrame;
        cv::cvtColor(frame, hsvFrame, cv::COLOR_BGR2HSV);
        hsvFrame.convertTo(hsvFrame, CV_32FC3);
        
        std::vector<cv::Mat> hsvChannels;
        cv::split(hsvFrame, hsvChannels);
        
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
        
        // Apply weighted hue shift
        hsvChannels[0] += weightedHue * colorCoeff;
        
        // Saturation from RMS
        float saturationMod = 1.0f + (rms * colorCoeff * 3.0f);
        hsvChannels[1] *= saturationMod;
        
        // Value from peak energy across all bands
        float peakEnergy = *std::max_element(bandAvgs.begin(), bandAvgs.end());
        float valueMod = 1.0f + (peakEnergy * colorCoeff * 2.0f);
        hsvChannels[2] *= valueMod;
        
        std::cout << "Weighted Hue: " << weightedHue 
                  << ", RMS: " << rms 
                  << ", Peak: " << peakEnergy << std::endl;
        
        // Clamp HSV values
        cv::threshold(hsvChannels[0], hsvChannels[0], 180, 180, cv::THRESH_TRUNC);
        cv::threshold(hsvChannels[1], hsvChannels[1], 255, 255, cv::THRESH_TRUNC);
        cv::threshold(hsvChannels[2], hsvChannels[2], 255, 255, cv::THRESH_TRUNC);
        
        cv::Mat result;
        cv::merge(hsvChannels, result);
        result.convertTo(result, CV_8UC3);
        
        cv::Mat output;
        cv::cvtColor(result, output, cv::COLOR_HSV2BGR);
        
        return output;
    } else {
        // RGB Mode: Map 8 bands to RGB channels
        // Bands 0-2 (sub-bass, bass, low-mid) → Red
        // Bands 3-5 (mid, high-mid) → Green  
        // Bands 6-7 (treble, high-treble) → Blue
        
        float redEnergy = (bandAvgs[0] + bandAvgs[1] + bandAvgs[2]) / 3.0f;
        float greenEnergy = (bandAvgs[3] + bandAvgs[4] + bandAvgs[5]) / 3.0f;
        float blueEnergy = (bandAvgs[6] + bandAvgs[7]) / 2.0f;
        
        std::cout << "Band energies - R(0-2): " << redEnergy 
                  << ", G(3-5): " << greenEnergy 
                  << ", B(6-7): " << blueEnergy << std::endl;
        
        // Convert frame to float
        cv::Mat frameFloat;
        frame.convertTo(frameFloat, CV_32FC3);
        
        std::vector<cv::Mat> channels;
        cv::split(frameFloat, channels);
        
        // Apply audio reactive color modulation (BGR order in OpenCV)
        float blueMod = 1.0f + (blueEnergy * colorCoeff);
        float greenMod = 1.0f + (greenEnergy * colorCoeff);
        float redMod = 1.0f + (redEnergy * colorCoeff);
        
        channels[0] *= blueMod;   // Blue (high frequencies)
        channels[1] *= greenMod;  // Green (mid frequencies)
        channels[2] *= redMod;    // Red (low frequencies)
        
        // Merge channels
        cv::Mat result;
        cv::merge(channels, result);
        
        // Clamp and convert back to uint8
        cv::Mat output;
        result.convertTo(output, CV_8UC3);
        
        return output;
    }
}
