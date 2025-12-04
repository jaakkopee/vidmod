#pragma once

#include "CircularBuffer.h"
#include <vector>
#include <complex>

class AudioBuffer : public CircularBuffer<float> {
private:
    int sampleRate;

public:
    AudioBuffer(const std::vector<float>& audio, int sr);
    
    int getSampleRate() const { return sampleRate; }
    
    // Get FFT split into 3 frequency bands (bass, mid, treble)
    void getFFT(const std::vector<float>& audioBuffer, 
                std::vector<float>& bass,
                std::vector<float>& mid,
                std::vector<float>& treble);
    
    // Get FFT split into 8 frequency bands
    void get8ChannelFFT(const std::vector<float>& audioBuffer,
                        std::vector<std::vector<float>>& bands);
    
    // Get root mean square of audio buffer
    float getRMS(const std::vector<float>& audioBuffer);
};
