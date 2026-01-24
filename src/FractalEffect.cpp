#include "FractalEffect.h"
#include <cmath>
#include <algorithm>

FractalEffect::FractalEffect() : Effect("Fractal") {
    setParameter("zoom", 1.5f);           // Zoom level into fractal space
    setParameter("max_iter", 30.0f);      // Base maximum iterations (reduced for performance)
    setParameter("audio_depth", 30.0f);   // Additional iterations from audio
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
            double xPos = (x - halfCols) / scaleX;
            double yPos = (y - halfRows) / scaleY;
            
            // Get original pixel brightness to offset fractal coordinates
            const int idx = x * 3;
            const uchar b = frameRow[idx];
            const uchar g = frameRow[idx + 1];
            const uchar r = frameRow[idx + 2];
            double brightness = (b + g + r) / (3.0 * 255.0);
            
            // Brightness modulates starting position (image influences fractal)
            xPos += (brightness - 0.5) * 0.3;
            yPos += (brightness - 0.5) * 0.3;
            
            // Calculate Julia set iteration count
            int iter = juliaIteration(xPos, yPos, cReal, cImag, maxIter);
            
            // Map iteration count to color
            if (iter == maxIter) {
                // Inside the set - use dark version of original color
                fractalRow[idx] = b >> 2;      // Fast divide by 4
                fractalRow[idx + 1] = g >> 2;
                fractalRow[idx + 2] = r >> 2;
            } else {
                // Outside the set - colorize based on escape time
                float t = static_cast<float>(iter) * invMaxIter;
                
                // Sample color from original image at a position based on iteration
                int sampleX = static_cast<int>(t * cols) % cols;
                int sampleY = static_cast<int>(t * rows) % rows;
                const uchar* sampleRow = frame.ptr<uchar>(sampleY);
                const int sampleIdx = sampleX * 3;
                
                // Blend sampled color with iteration-based gradient
                float gradient = std::sin(t * 3.14159f) * 255.0f;
                fractalRow[idx] = static_cast<uchar>(sampleRow[sampleIdx] * 0.7f + gradient * 0.3f);
                fractalRow[idx + 1] = static_cast<uchar>(sampleRow[sampleIdx + 1] * 0.7f + gradient * 0.3f);
                fractalRow[idx + 2] = static_cast<uchar>(sampleRow[sampleIdx + 2] * 0.7f + gradient * 0.3f);
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
