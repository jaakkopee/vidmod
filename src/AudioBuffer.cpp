#include "AudioBuffer.h"
#include <fftw3.h>
#include <sndfile.h>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <cstring>

AudioBuffer::AudioBuffer(const std::vector<float>& audio, int sr) 
    : CircularBuffer<float>(audio), sampleRate(sr) {}

void AudioBuffer::getFFT(const std::vector<float>& audioBuffer,
                         std::vector<float>& bass,
                         std::vector<float>& mid,
                         std::vector<float>& treble) {
    size_t n = audioBuffer.size();
    
    // Allocate FFTW arrays
    double* in = fftw_alloc_real(n);
    fftw_complex* out = fftw_alloc_complex(n / 2 + 1);
    
    // Copy input data
    for (size_t i = 0; i < n; ++i) {
        in[i] = audioBuffer[i];
    }
    
    // Create plan and execute FFT
    fftw_plan plan = fftw_plan_dft_r2c_1d(n, in, out, FFTW_ESTIMATE);
    fftw_execute(plan);
    
    // Get magnitudes
    std::vector<float> magnitudes;
    magnitudes.reserve(n / 2);
    float maxMag = 0.0f;
    
    for (size_t i = 0; i < n / 2; ++i) {
        float mag = std::sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);
        magnitudes.push_back(mag);
        if (mag > maxMag) maxMag = mag;
    }
    
    // Normalize
    if (maxMag > 0.0f) {
        for (auto& mag : magnitudes) {
            mag /= maxMag;
        }
    }
    
    // Split into 3 bands
    size_t bandSize = magnitudes.size() / 3;
    bass.clear();
    mid.clear();
    treble.clear();
    
    bass.assign(magnitudes.begin(), magnitudes.begin() + bandSize);
    mid.assign(magnitudes.begin() + bandSize, magnitudes.begin() + 2 * bandSize);
    treble.assign(magnitudes.begin() + 2 * bandSize, magnitudes.end());
    
    // Cleanup
    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);
}

void AudioBuffer::get8ChannelFFT(const std::vector<float>& audioBuffer,
                                 std::vector<std::vector<float>>& bands) {
    size_t n = audioBuffer.size();
    
    // Allocate FFTW arrays
    double* in = fftw_alloc_real(n);
    fftw_complex* out = fftw_alloc_complex(n / 2 + 1);
    
    // Copy input data
    for (size_t i = 0; i < n; ++i) {
        in[i] = audioBuffer[i];
    }
    
    // Create plan and execute FFT
    fftw_plan plan = fftw_plan_dft_r2c_1d(n, in, out, FFTW_ESTIMATE);
    fftw_execute(plan);
    
    // Get magnitudes
    std::vector<float> magnitudes;
    magnitudes.reserve(n / 2);
    float maxMag = 0.0f;
    
    for (size_t i = 0; i < n / 2; ++i) {
        float mag = std::sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);
        magnitudes.push_back(mag);
        if (mag > maxMag) maxMag = mag;
    }
    
    // Normalize
    if (maxMag > 0.0f) {
        for (auto& mag : magnitudes) {
            mag /= maxMag;
        }
    }
    
    // Split into 8 bands
    size_t bandSize = magnitudes.size() / 8;
    bands.clear();
    bands.resize(8);
    
    for (int i = 0; i < 8; ++i) {
        size_t start = i * bandSize;
        size_t end = (i == 7) ? magnitudes.size() : (i + 1) * bandSize;
        bands[i].assign(magnitudes.begin() + start, magnitudes.begin() + end);
    }
    
    // Cleanup
    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);
}

float AudioBuffer::getRMS(const std::vector<float>& audioBuffer) {
    if (audioBuffer.empty()) return 0.0f;
    
    float sum = 0.0f;
    for (float sample : audioBuffer) {
        sum += sample * sample;
    }
    
    return std::sqrt(sum / audioBuffer.size());
}

bool AudioBuffer::saveToWAV(const std::string& outputPath) const {
    SF_INFO sfInfo;
    memset(&sfInfo, 0, sizeof(sfInfo));
    sfInfo.samplerate = sampleRate;
    sfInfo.channels = 1;  // Mono
    sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    
    SNDFILE* sndFile = sf_open(outputPath.c_str(), SFM_WRITE, &sfInfo);
    if (!sndFile) {
        std::cerr << "Error: Could not open audio file for writing: " << outputPath << std::endl;
        return false;
    }
    
    sf_count_t written = sf_write_float(sndFile, getData().data(), getData().size());
    sf_close(sndFile);
    
    if (written != static_cast<sf_count_t>(getData().size())) {
        std::cerr << "Error: Could not write all audio samples" << std::endl;
        return false;
    }
    
    return true;
}
