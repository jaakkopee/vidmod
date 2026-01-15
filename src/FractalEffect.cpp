#include "FractalEffect.h"
#include <cmath>
#include <algorithm>

FractalEffect::FractalEffect() : Effect("Fractal") {
    setParameter("zoom", 1.5f);           // Zoom level into fractal space
    setParameter("max_iter", 50.0f);      // Base maximum iterations
    setParameter("audio_depth", 50.0f);   // Additional iterations from audio
    setParameter("cx", -0.4f);            // Julia set constant real part
    setParameter("cy", 0.6f);             // Julia set constant imaginary part
    setParameter("blend", 0.5f);          // Blend original image with fractal
}

int FractalEffect::juliaIteration(double x, double y, double cReal, double cImag, int maxIter) {
    double zReal = x;
    double zImag = y;
    int iter = 0;
    
    // Julia set iteration: z = z^2 + c
    while (iter < maxIter && (zReal * zReal + zImag * zImag) < 4.0) {
        double temp = zReal * zReal - zImag * zImag + cReal;
        zImag = 2.0 * zReal * zImag + cImag;
        zReal = temp;
        iter++;
    }
    
    return iter;
}

cv::Mat FractalEffect::apply(const cv::Mat& frame, AudioBuffer* audioBuffer, float videoFps) {
    float zoom = getParameter("zoom", 1.5f);
    float baseMaxIter = getParameter("max_iter", 50.0f);
    float audioDepthMult = getParameter("audio_depth", 50.0f);
    float cx = getParameter("cx", -0.4f);
    float cy = getParameter("cy", 0.6f);
    float blendAmount = getParameter("blend", 0.5f);
    
    int maxIter = static_cast<int>(baseMaxIter);
    double cReal = cx;
    double cImag = cy;
    
    // Modulate fractal parameters with audio
    if (audioBuffer) {
        int audioFramesPerVideoFrame = static_cast<int>(audioBuffer->getSampleRate() / videoFps);
        std::vector<float> audioSamples = audioBuffer->getBuffer(audioFramesPerVideoFrame);
        
        // Get 8-channel FFT for detailed frequency control
        std::vector<std::vector<float>> bands;
        audioBuffer->get8ChannelFFT(audioSamples, bands);
        
        // Calculate band averages
        std::vector<float> bandAvgs(8, 0.0f);
        for (int i = 0; i < 8; ++i) {
            if (!bands[i].empty()) {
                for (float val : bands[i]) {
                    bandAvgs[i] += val;
                }
                bandAvgs[i] /= bands[i].size();
            }
        }
        
        // Low frequencies control iteration depth (more bass = deeper fractals)
        float bassEnergy = (bandAvgs[0] + bandAvgs[1] + bandAvgs[2]) / 3.0f;
        maxIter += static_cast<int>(bassEnergy * audioDepthMult);
        
        // Mid frequencies rotate the Julia constant around origin
        float midEnergy = (bandAvgs[3] + bandAvgs[4]) / 2.0f;
        double angle = midEnergy * 6.28318530718; // 0 to 2π
        double magnitude = std::sqrt(cReal * cReal + cImag * cImag);
        cReal = magnitude * std::cos(angle);
        cImag = magnitude * std::sin(angle);
        
        // High frequencies modulate zoom
        float trebleEnergy = (bandAvgs[6] + bandAvgs[7]) / 2.0f;
        zoom *= (1.0f + trebleEnergy * 2.0f);
        
        // Overall RMS affects blend amount
        float rms = audioBuffer->getRMS(audioSamples);
        blendAmount = std::min(1.0f, blendAmount + rms * 0.5f);
    }
    
    // Clamp values
    maxIter = std::max(1, std::min(maxIter, 500));
    blendAmount = std::max(0.0f, std::min(1.0f, blendAmount));
    
    cv::Mat fractalImage(frame.size(), CV_8UC3);
    cv::Mat frameFloat;
    frame.convertTo(frameFloat, CV_32FC3);
    
    // Generate fractal using image data
    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            // Map pixel position to complex plane
            double xPos = (x - frame.cols / 2.0) / (frame.cols / 4.0) / zoom;
            double yPos = (y - frame.rows / 2.0) / (frame.rows / 4.0) / zoom;
            
            // Get original pixel brightness to offset fractal coordinates
            cv::Vec3b pixel = frame.at<cv::Vec3b>(y, x);
            double brightness = (pixel[0] + pixel[1] + pixel[2]) / (3.0 * 255.0);
            
            // Brightness modulates starting position (image influences fractal)
            xPos += (brightness - 0.5) * 0.3;
            yPos += (brightness - 0.5) * 0.3;
            
            // Calculate Julia set iteration count
            int iter = juliaIteration(xPos, yPos, cReal, cImag, maxIter);
            
            // Map iteration count to color
            if (iter == maxIter) {
                // Inside the set - use dark version of original color
                fractalImage.at<cv::Vec3b>(y, x) = cv::Vec3b(
                    pixel[0] / 4,
                    pixel[1] / 4,
                    pixel[2] / 4
                );
            } else {
                // Outside the set - colorize based on escape time
                float t = static_cast<float>(iter) / maxIter;
                
                // Sample color from original image at a position based on iteration
                int sampleX = static_cast<int>(t * frame.cols) % frame.cols;
                int sampleY = static_cast<int>(t * frame.rows) % frame.rows;
                cv::Vec3b sampleColor = frame.at<cv::Vec3b>(sampleY, sampleX);
                
                // Blend sampled color with iteration-based gradient
                float gradient = std::sin(t * 3.14159f) * 255.0f;
                fractalImage.at<cv::Vec3b>(y, x) = cv::Vec3b(
                    static_cast<uchar>((sampleColor[0] * 0.7f + gradient * 0.3f)),
                    static_cast<uchar>((sampleColor[1] * 0.7f + gradient * 0.3f)),
                    static_cast<uchar>((sampleColor[2] * 0.7f + gradient * 0.3f))
                );
            }
        }
    }
    
    // Blend fractal with original frame
    cv::Mat fractalFloat;
    fractalImage.convertTo(fractalFloat, CV_32FC3);
    
    cv::Mat result;
    cv::addWeighted(frameFloat, 1.0f - blendAmount, fractalFloat, blendAmount, 0.0, result);
    
    cv::Mat output;
    result.convertTo(output, CV_8UC3);
    
    return output;
}
