# Audio Muxing in FFTVidMod

## Overview
The output video now automatically includes the audio that was used to modulate the video effects.

## How It Works

When you process a video or image loop with audio:

1. **Video Processing**: OpenCV's VideoWriter creates the processed video file (video stream only)
2. **Audio Export**: The audio buffer is saved to a temporary WAV file
3. **FFmpeg Muxing**: FFmpeg combines the video and audio streams into the final output file
4. **Cleanup**: Temporary files are automatically removed

## Requirements

- **FFmpeg** must be installed on your system
- Check if installed: `ffmpeg -version`
- Install on macOS: `brew install ffmpeg`
- Install on Linux: `apt-get install ffmpeg` or `yum install ffmpeg`

## Output Format

- Video codec: Same as input (copied without re-encoding)
- Audio codec: AAC (re-encoded from internal audio buffer)
- Duration: Shortest of video or audio (prevents audio/video desync)

## Behavior

### Video Processing Mode
- If audio is longer than video: Video loops to match audio duration
- If video is longer than audio: Audio loops to match video duration
- Final output: Both streams are muxed with `-shortest` flag

### Image Loop Mode
- Video is generated from a static image at specified FPS
- Audio duration determines the video length
- Audio is perfectly synced with the generated frames

## Error Handling

If FFmpeg muxing fails:
- A warning is displayed in the console
- The video-only file is preserved
- Processing continues (degraded mode without audio)

## Technical Details

The muxing command used:
```bash
ffmpeg -y -i temp_video.mp4 -i temp_audio.wav -c:v copy -c:a aac -shortest output.mp4
```

Flags:
- `-y`: Overwrite output file without asking
- `-i`: Input files (video and audio)
- `-c:v copy`: Copy video stream without re-encoding
- `-c:a aac`: Encode audio to AAC format
- `-shortest`: Finish encoding when shortest stream ends
