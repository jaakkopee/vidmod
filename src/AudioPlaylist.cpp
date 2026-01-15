#include "AudioPlaylist.h"
#include <sndfile.h>
#include <iostream>
#include <algorithm>

AudioPlaylist::AudioPlaylist(int sampleRate) 
    : audioBuffer(nullptr), targetSampleRate(sampleRate) {}

bool AudioPlaylist::loadAudioFile(const std::string& filePath, std::vector<float>& monoAudio, int& sampleRate) {
    SF_INFO sfInfo;
    SNDFILE* sndFile = sf_open(filePath.c_str(), SFM_READ, &sfInfo);
    
    if (!sndFile) {
        std::cerr << "Error: Could not open audio file: " << filePath << std::endl;
        return false;
    }
    
    std::cout << "Loading audio: " << filePath << std::endl;
    std::cout << "Sample rate: " << sfInfo.samplerate << ", Channels: " << sfInfo.channels << std::endl;
    
    // Read audio data
    std::vector<float> audioData(sfInfo.frames * sfInfo.channels);
    sf_count_t count = sf_readf_float(sndFile, audioData.data(), sfInfo.frames);
    
    sf_close(sndFile);
    
    // Convert to mono if stereo
    monoAudio.clear();
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
    
    sampleRate = sfInfo.samplerate;
    return true;
}

std::vector<float> AudioPlaylist::resampleAudio(const std::vector<float>& input, int inputRate, int outputRate) {
    if (inputRate == outputRate) {
        return input;
    }
    
    // Simple linear interpolation resampling
    double ratio = static_cast<double>(outputRate) / inputRate;
    size_t outputSize = static_cast<size_t>(input.size() * ratio);
    std::vector<float> output;
    output.reserve(outputSize);
    
    for (size_t i = 0; i < outputSize; ++i) {
        double srcPos = i / ratio;
        size_t idx = static_cast<size_t>(srcPos);
        
        if (idx + 1 < input.size()) {
            double frac = srcPos - idx;
            float sample = input[idx] * (1.0 - frac) + input[idx + 1] * frac;
            output.push_back(sample);
        } else if (idx < input.size()) {
            output.push_back(input[idx]);
        }
    }
    
    return output;
}

void AudioPlaylist::rebuildBuffer() {
    if (tracks.empty()) {
        audioBuffer.reset();
        return;
    }
    
    // Concatenate all track audio data
    std::vector<float> combinedAudio;
    size_t totalSamples = 0;
    
    for (const auto& track : tracks) {
        totalSamples += track.audioData.size();
    }
    
    combinedAudio.reserve(totalSamples);
    
    // Update start/end indices and concatenate
    size_t currentIndex = 0;
    for (auto& track : tracks) {
        track.startIndex = currentIndex;
        combinedAudio.insert(combinedAudio.end(), track.audioData.begin(), track.audioData.end());
        currentIndex += track.audioData.size();
        track.endIndex = currentIndex;
    }
    
    // Create new audio buffer with combined data
    audioBuffer = std::make_unique<AudioBuffer>(combinedAudio, targetSampleRate);
    
    std::cout << "Playlist rebuilt: " << tracks.size() << " tracks, " 
              << combinedAudio.size() << " samples, "
              << (combinedAudio.size() / static_cast<float>(targetSampleRate)) << " seconds" << std::endl;
}

bool AudioPlaylist::addTrack(const std::string& audioFile) {
    std::vector<float> monoAudio;
    int sampleRate;
    
    if (!loadAudioFile(audioFile, monoAudio, sampleRate)) {
        return false;
    }
    
    // Resample if necessary
    if (sampleRate != targetSampleRate) {
        std::cout << "Resampling from " << sampleRate << " Hz to " << targetSampleRate << " Hz..." << std::endl;
        monoAudio = resampleAudio(monoAudio, sampleRate, targetSampleRate);
    }
    
    // Calculate start index (end of current buffer)
    size_t startIndex = 0;
    if (!tracks.empty()) {
        startIndex = tracks.back().endIndex;
    }
    
    // Add track
    tracks.emplace_back(audioFile, monoAudio, targetSampleRate, startIndex);
    
    // Rebuild buffer
    rebuildBuffer();
    
    std::cout << "Track added: " << audioFile << " (duration: " 
              << (monoAudio.size() / static_cast<float>(targetSampleRate)) << "s)" << std::endl;
    
    return true;
}

bool AudioPlaylist::removeTrack(size_t index) {
    if (index >= tracks.size()) {
        std::cerr << "Error: Track index out of range" << std::endl;
        return false;
    }
    
    std::string removedFile = tracks[index].filePath;
    tracks.erase(tracks.begin() + index);
    
    // Rebuild buffer with remaining tracks
    rebuildBuffer();
    
    std::cout << "Track removed: " << removedFile << std::endl;
    
    return true;
}

void AudioPlaylist::clear() {
    tracks.clear();
    audioBuffer.reset();
    std::cout << "Playlist cleared" << std::endl;
}

float AudioPlaylist::getTotalDuration() const {
    if (!audioBuffer) {
        return 0.0f;
    }
    return static_cast<float>(audioBuffer->size()) / targetSampleRate;
}

bool AudioPlaylist::moveTrack(size_t fromIndex, size_t toIndex) {
    if (fromIndex >= tracks.size() || toIndex >= tracks.size()) {
        std::cerr << "Error: Track index out of range" << std::endl;
        return false;
    }
    
    if (fromIndex == toIndex) {
        return true;
    }
    
    // Move track
    AudioTrack track = std::move(tracks[fromIndex]);
    tracks.erase(tracks.begin() + fromIndex);
    tracks.insert(tracks.begin() + toIndex, std::move(track));
    
    // Rebuild buffer to update indices
    rebuildBuffer();
    
    std::cout << "Track moved from position " << fromIndex << " to " << toIndex << std::endl;
    
    return true;
}
