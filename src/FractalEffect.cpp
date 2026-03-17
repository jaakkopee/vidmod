#include "FractalEffect.h"
#include <cmath>
#include <algorithm>

FractalEffect::FractalEffect() : Effect("Fractal") {
    setParameter("zoom", 1.0f);           // Zoom magnification (higher = closer zoom)
    setParameter("center_x", 0.0f);       // View center in complex plane (real)
    setParameter("center_y", 0.0f);       // View center in complex plane (imag)
    setParameter("max_iter", 80.0f);      // Base maximum iterations for detail
    setParameter("audio_depth", 30.0f);   // Additional iterations from audio
    setParameter("cx", -0.7f);            // Julia set constant real part
    setParameter("cy", 0.27015f);         // Julia set constant imaginary part
    setParameter("blend", 0.85f);         // Blend original image with fractal
    setParameter("input_warp", 0.0f);     // How much source image warps fractal coordinates
    setParameter("color_source_mix", 0.0f); // How much source image colors fractal output
    setParameter("audio_gain", 0.0f);     // Keep manual fractal control predictable by default
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
    float zoom = getParameter("zoom", 1.0f);
    float centerX = getParameter("center_x", 0.0f);
    float centerY = getParameter("center_y", 0.0f);
    float baseMaxIter = getParameter("max_iter", 80.0f);
    float audioDepthMult = getParameter("audio_depth", 50.0f);
    float cx = getParameter("cx", -0.7f);
    float cy = getParameter("cy", 0.27015f);
    float blendAmount = getParameter("blend", 0.85f);
    float inputWarp = getParameter("input_warp", 0.0f);
    float colorSourceMix = getParameter("color_source_mix", 0.0f);
    float audioGain = getParameter("audio_gain", 1.0f);
    
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
        maxIter += static_cast<int>(bassEnergy * audioDepthMult * 0.35f * audioGain);
        
        // Mid frequencies rotate the Julia constant by a small offset around its base angle.
        // This preserves the chosen cx/cy character instead of replacing it each frame.
        float midEnergy = (bandAvgs[3] + bandAvgs[4]) / 2.0f;
        double baseAngle = std::atan2(cImag, cReal);
        double angleOffset = midEnergy * 1.57079632679 * audioGain; // up to ~pi/2
        double angle = baseAngle + angleOffset;
        double magnitude = std::sqrt(cReal * cReal + cImag * cImag);
        cReal = magnitude * std::cos(angle);
        cImag = magnitude * std::sin(angle);
        
        // High frequencies modulate zoom
        float trebleEnergy = (bandAvgs[6] + bandAvgs[7]) / 2.0f;
        zoom *= (1.0f + trebleEnergy * 0.35f * audioGain);
        
        // Overall RMS affects blend amount
        float rms = audioBuffer->getRMS(audioSamples);
        blendAmount = std::min(1.0f, blendAmount + rms * 0.15f * audioGain);
    }
    
    // Clamp values
    zoom = std::max(0.05f, std::min(zoom, 200.0f));
    inputWarp = std::max(0.0f, std::min(inputWarp, 2.0f));
    colorSourceMix = std::max(0.0f, std::min(colorSourceMix, 1.0f));
    maxIter = std::max(1, std::min(maxIter, 200));  // Reduced from 500
    blendAmount = std::max(0.0f, std::min(1.0f, blendAmount));
    
    cv::Mat fractalImage(frame.size(), CV_8UC3);
    
    // Precalculate constants outside the loop
    const int cols = frame.cols;
    const int rows = frame.rows;
    const double halfCols = cols / 2.0;
    const double halfRows = rows / 2.0;
    const double scaleX = (cols / 4.0) * zoom;
    const double scaleY = (rows / 4.0) * zoom;
    const float invMaxIter = 1.0f / maxIter;
    const int blendAmountInt = static_cast<int>(blendAmount * 256.0f);  // Fixed-point blend
    const int invBlendAmountInt = 256 - blendAmountInt;
    
    // Generate fractal using image data with OpenMP parallelization
    #pragma omp parallel for schedule(dynamic, 16)
    for (int y = 0; y < rows; ++y) {
        // Use row pointers for faster access
        const uchar* frameRow = frame.ptr<uchar>(y);
        uchar* fractalRow = fractalImage.ptr<uchar>(y);
        
        for (int x = 0; x < cols; ++x) {
            // Map pixel position to complex plane
            double xPos = (x - halfCols) / scaleX + centerX;
            double yPos = (y - halfRows) / scaleY + centerY;
            
            // Get original pixel brightness to offset fractal coordinates
            const int idx = x * 3;
            const uchar b = frameRow[idx];
            const uchar g = frameRow[idx + 1];
            const uchar r = frameRow[idx + 2];
            double brightness = (b + g + r) / (3.0 * 255.0);
            
            // Brightness modulates starting position (image influences fractal)
            xPos += (brightness - 0.5) * inputWarp / zoom;
            yPos += (brightness - 0.5) * inputWarp / zoom;
            
            // Calculate Julia set iteration count
            int iter = juliaIteration(xPos, yPos, cReal, cImag, maxIter);
            
            // Map iteration count to color
            if (iter == maxIter) {
                // Inside the set: keep detail visible instead of collapsing to black.
                fractalRow[idx] = static_cast<uchar>(b * 0.18f + 12.0f);
                fractalRow[idx + 1] = static_cast<uchar>(g * 0.18f + 10.0f);
                fractalRow[idx + 2] = static_cast<uchar>(r * 0.18f + 18.0f);
            } else {
                // Outside the set - colorize based on escape time
                float t = static_cast<float>(iter) * invMaxIter;
                
                // Sample color from original image at a position based on iteration
                int sampleX = static_cast<int>(t * cols) % cols;
                int sampleY = static_cast<int>(t * rows) % rows;
                const uchar* sampleRow = frame.ptr<uchar>(sampleY);
                const int sampleIdx = sampleX * 3;
                
                // Use a vivid fractal palette so color_source_mix=0 still produces visible output.
                float oneMinusT = 1.0f - t;
                float pr = 9.0f * oneMinusT * t * t * t * 255.0f;
                float pg = 15.0f * oneMinusT * oneMinusT * t * t * 255.0f;
                float pb = 8.5f * oneMinusT * oneMinusT * oneMinusT * t * 255.0f;
                float sourceMix = colorSourceMix;
                float gradientMix = 1.0f - sourceMix;
                fractalRow[idx] = static_cast<uchar>(sampleRow[sampleIdx] * sourceMix + pb * gradientMix);
                fractalRow[idx + 1] = static_cast<uchar>(sampleRow[sampleIdx + 1] * sourceMix + pg * gradientMix);
                fractalRow[idx + 2] = static_cast<uchar>(sampleRow[sampleIdx + 2] * sourceMix + pr * gradientMix);
            }
        }
    }
    
    // Blend fractal with original frame using fast integer math
    cv::Mat output(frame.size(), CV_8UC3);
    #pragma omp parallel for
    for (int y = 0; y < rows; ++y) {
        const uchar* frameRow = frame.ptr<uchar>(y);
        const uchar* fractalRow = fractalImage.ptr<uchar>(y);
        uchar* outRow = output.ptr<uchar>(y);
        
        for (int x = 0; x < cols * 3; ++x) {
            outRow[x] = (frameRow[x] * invBlendAmountInt + fractalRow[x] * blendAmountInt) >> 8;
        }
    }
    
    return output;
}
