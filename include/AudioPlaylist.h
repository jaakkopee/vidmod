#pragma once

#include "AudioBuffer.h"
#include <string>
#include <vector>
#include <memory>

struct AudioTrack {
    std::string filePath;
    int sampleRate;
    size_t startIndex;  // Start index in the combined buffer
    size_t endIndex;    // End index in the combined buffer
    
    AudioTrack(const std::string& path, int sr, size_t start, size_t end)
        : filePath(path), sampleRate(sr), startIndex(start), endIndex(end) {}
};

class AudioPlaylist {
private:
    std::vector<AudioTrack> tracks;
    std::unique_ptr<AudioBuffer> audioBuffer;
    int targetSampleRate;  // Target sample rate for all tracks
    
    // Load audio file and convert to mono
    bool loadAudioFile(const std::string& filePath, std::vector<float>& monoAudio, int& sampleRate);
    
    // Resample audio to match target sample rate
    std::vector<float> resampleAudio(const std::vector<float>& input, int inputRate, int outputRate);
    
    // Rebuild the combined audio buffer from all tracks
    void rebuildBuffer();
    
public:
    AudioPlaylist(int sampleRate = 44100);
    
    // Add audio file to playlist
    bool addTrack(const std::string& audioFile);
    
    // Remove track by index
    bool removeTrack(size_t index);
    
    // Clear all tracks
    void clear();
    
    // Get number of tracks
    size_t getTrackCount() const { return tracks.size(); }
    
    // Get track info
    const AudioTrack& getTrack(size_t index) const { return tracks[index]; }
    
    // Get the combined audio buffer
    AudioBuffer* getAudioBuffer() { return audioBuffer.get(); }
    const AudioBuffer* getAudioBuffer() const { return audioBuffer.get(); }
    
    // Get total duration in seconds
    float getTotalDuration() const;

    // JSON serialization
    bool saveToJson(const std::string& filePath) const;
    bool loadFromJson(const std::string& filePath);
    
    // Move track to different position
    bool moveTrack(size_t fromIndex, size_t toIndex);
};
