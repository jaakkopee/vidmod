#include "AudioPlaylist.h"
#include <sndfile.h>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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

    std::vector<std::string> trackPaths;
    trackPaths.reserve(tracks.size());
    for (const auto& track : tracks) {
        trackPaths.push_back(track.filePath);
    }

    tracks.clear();
    audioBuffer.reset();

    for (const auto& path : trackPaths) {
        if (!addTrack(path)) {
            std::cerr << "Failed to rebuild playlist track: " << path << std::endl;
            break;
        }
    }

    std::cout << "Playlist rebuilt in new order: " << tracks.size() << " tracks, "
              << getTotalDuration() << " seconds" << std::endl;
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
    
    // Calculate start and end indices
    size_t startIndex = audioBuffer ? audioBuffer->size() : 0;
    size_t endIndex = startIndex + monoAudio.size();
    
    // Create combined audio data
    std::vector<float> combinedAudio;
    if (audioBuffer) {
        combinedAudio = audioBuffer->getData();
        combinedAudio.reserve(combinedAudio.size() + monoAudio.size());
    } else {
        combinedAudio.reserve(monoAudio.size());
    }
    
    // Append new audio
    combinedAudio.insert(combinedAudio.end(), monoAudio.begin(), monoAudio.end());
    
    // Free monoAudio memory immediately
    monoAudio.clear();
    monoAudio.shrink_to_fit();
    
    // Create new audio buffer
    audioBuffer = std::make_unique<AudioBuffer>(combinedAudio, targetSampleRate);
    
    // Add track metadata (without storing the audio data)
    tracks.emplace_back(audioFile, targetSampleRate, startIndex, endIndex);
    
    float duration = (endIndex - startIndex) / static_cast<float>(targetSampleRate);
    std::cout << "Track added: " << audioFile << " (duration: " << duration << "s)" << std::endl;
    
    return true;
}

bool AudioPlaylist::removeTrack(size_t index) {
    if (index >= tracks.size()) {
        std::cerr << "Error: Track index out of range" << std::endl;
        return false;
    }
    
    std::string removedFile = tracks[index].filePath;
    tracks.erase(tracks.begin() + index);
    
    // Rebuild the entire buffer without the removed track
    if (tracks.empty()) {
        audioBuffer.reset();
    } else {
        // Need to reload all tracks - this is expensive but necessary
        // Store track info temporarily
        std::vector<std::string> trackPaths;
        for (const auto& track : tracks) {
            trackPaths.push_back(track.filePath);
        }
        
        // Clear and rebuild
        tracks.clear();
        audioBuffer.reset();
        
        // Reload all tracks
        for (const auto& path : trackPaths) {
            addTrack(path);
        }
    }
    
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

bool AudioPlaylist::saveToJson(const std::string& filePath) const {
    try {
        json j;
        j["version"] = "1.0";
        j["sampleRate"] = targetSampleRate;
        j["tracks"] = json::array();

        for (const auto& track : tracks) {
            json trackJson;
            trackJson["filePath"] = track.filePath;
            j["tracks"].push_back(trackJson);
        }

        std::ofstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open playlist file for writing: " << filePath << std::endl;
            return false;
        }

        file << j.dump(2);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving playlist JSON: " << e.what() << std::endl;
        return false;
    }
}

bool AudioPlaylist::loadFromJson(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open playlist file for reading: " << filePath << std::endl;
            return false;
        }

        json j;
        file >> j;

        if (!j.contains("tracks") || !j["tracks"].is_array()) {
            std::cerr << "Invalid playlist JSON: missing 'tracks' array" << std::endl;
            return false;
        }

        AudioPlaylist loadedPlaylist(targetSampleRate);
        for (const auto& trackJson : j["tracks"]) {
            if (!trackJson.contains("filePath") || !trackJson["filePath"].is_string()) {
                std::cerr << "Invalid playlist JSON: track missing string filePath" << std::endl;
                return false;
            }

            const std::string path = trackJson["filePath"].get<std::string>();
            if (!loadedPlaylist.addTrack(path)) {
                std::cerr << "Failed to load playlist track: " << path << std::endl;
                return false;
            }
        }

        *this = std::move(loadedPlaylist);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading playlist JSON: " << e.what() << std::endl;
        return false;
    }
}

bool AudioPlaylist::moveTrack(size_t fromIndex, size_t toIndex) {
    if (fromIndex >= tracks.size() || toIndex >= tracks.size()) {
        std::cerr << "Error: Track index out of range" << std::endl;
        return false;
    }
    
    if (fromIndex == toIndex) {
        return true;
    }

    std::vector<std::string> reorderedPaths;
    reorderedPaths.reserve(tracks.size());
    for (const auto& track : tracks) {
        reorderedPaths.push_back(track.filePath);
    }

    const std::string movedPath = reorderedPaths[fromIndex];
    reorderedPaths.erase(reorderedPaths.begin() + fromIndex);
    reorderedPaths.insert(reorderedPaths.begin() + toIndex, movedPath);

    tracks.clear();
    audioBuffer.reset();

    for (const auto& path : reorderedPaths) {
        if (!addTrack(path)) {
            std::cerr << "Failed to rebuild reordered playlist" << std::endl;
            return false;
        }
    }
    
    std::cout << "Track moved from position " << fromIndex << " to " << toIndex << std::endl;
    
    return true;
}
