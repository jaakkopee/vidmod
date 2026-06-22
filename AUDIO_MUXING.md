# Audio Muxing in VidMod

## Overview
The output video now automatically includes the audio that was used to modulate the video effects.

This applies to both video processing and image-loop rendering. If a playlist is loaded, playlist audio is used; otherwise, the currently loaded media audio buffer is used.

Muxing progress in the GUI is derived from FFmpeg timeline output (`out_time`) relative to expected mux duration, so the progress bar reflects real work instead of synthetic line-count progress.

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

- Video codec: H.264 (`libx264`) for robust MP4 output compatibility
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

### Audio-Reactive Effects Notes
- Effects read one audio slice per rendered frame, synchronized by FPS
- All effects in the chain read the same slice for that frame
- This keeps audio-driven parameters aligned across stacked effects

## Error Handling

If FFmpeg muxing fails:
- A warning is displayed in the console
- The original video-only temporary output is restored automatically
- Processing continues (degraded mode without audio)

If audio has long silent tails, muxing can still take substantial time near the end of processing. With timeline-driven progress this is expected and accurately reflected.

## Technical Details

The muxing command used:
```bash
ffmpeg -y -nostdin -hide_banner -loglevel error -progress pipe:1 -nostats \
	-i temp_video.mp4 -i temp_audio.wav \
	-map 0:v:0 -map 1:a:0 \
	-c:v libx264 -preset fast -crf 18 \
	-c:a aac -b:a 192k -shortest -disposition:a:0 default output.mp4
```

Flags:
- `-y`: Overwrite output file without asking
- `-nostdin`: Avoid interactive blocking during GUI-driven runs
- `-i`: Input files (video and audio)
- `-progress pipe:1 -nostats`: Emit machine-readable mux progress
- `-c:v libx264 -preset fast -crf 18`: Re-encode video for stable MP4 output
- `-c:a aac`: Encode audio to AAC format
- `-shortest`: Finish encoding when shortest stream ends
