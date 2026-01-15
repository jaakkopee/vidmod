# Audio Playlist Usage

The `AudioPlaylist` class manages multiple audio files and combines them into a single `AudioBuffer` for processing.

## Features

- **Add tracks**: Load audio files and automatically combine them
- **Remove tracks**: Remove individual tracks and automatically rebuild the buffer
- **Reorder tracks**: Move tracks to different positions in the playlist
- **Automatic resampling**: All tracks are resampled to a common sample rate
- **Efficient buffer management**: Uses the existing `AudioBuffer` infrastructure

## Example Usage

```cpp
#include "AudioPlaylist.h"

// Create a playlist with 44100 Hz sample rate
AudioPlaylist playlist(44100);

// Add audio files
playlist.addTrack("song1.wav");
playlist.addTrack("song2.mp3");
playlist.addTrack("ambient.flac");

// Get total duration
float duration = playlist.getTotalDuration();
std::cout << "Total duration: " << duration << " seconds\n";

// Get the combined audio buffer for processing
AudioBuffer* buffer = playlist.getAudioBuffer();

// Remove a track (rebuilds the buffer automatically)
playlist.removeTrack(1);  // Remove second track

// Move a track to a different position
playlist.moveTrack(1, 0);  // Move track at index 1 to position 0

// Clear all tracks
playlist.clear();
```

## Integration with VideoProcessor

```cpp
#include "VideoProcessor.h"
#include "AudioPlaylist.h"
#include "EffectChain.h"

// Create video processor and playlist
VideoProcessor videoProcessor;
AudioPlaylist playlist(44100);

// Load video
videoProcessor.loadVideo("input.mp4");

// Build playlist
playlist.addTrack("track1.wav");
playlist.addTrack("track2.wav");
playlist.addTrack("track3.wav");

// Get the combined audio buffer
AudioBuffer* combinedAudio = playlist.getAudioBuffer();

// Use the playlist's audio buffer for video processing
// (You can manually set this or pass it to effects)

// Example with effect chain
EffectChain effects;
// Add effects...

// Process video with playlist audio
// Note: You may need to temporarily swap the audio buffer
// or modify VideoProcessor to accept an external AudioBuffer
```

## API Reference

### Constructor
- `AudioPlaylist(int sampleRate = 44100)` - Create playlist with target sample rate

### Track Management
- `bool addTrack(const std::string& audioFile)` - Add audio file to end of playlist
- `bool removeTrack(size_t index)` - Remove track at index
- `bool moveTrack(size_t fromIndex, size_t toIndex)` - Reorder tracks
- `void clear()` - Remove all tracks

### Information
- `size_t getTrackCount() const` - Get number of tracks
- `const AudioTrack& getTrack(size_t index) const` - Get track info
- `float getTotalDuration() const` - Get total duration in seconds
- `AudioBuffer* getAudioBuffer()` - Get the combined audio buffer

## Implementation Details

- All tracks are automatically converted to mono
- Tracks with different sample rates are resampled to the target rate
- The buffer is rebuilt whenever tracks are added, removed, or reordered
- Each track maintains its start/end index in the combined buffer
- Uses simple linear interpolation for resampling
