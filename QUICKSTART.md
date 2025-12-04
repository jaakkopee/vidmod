# Quick Start Guide

Get started with FFT Video Modulator in 5 minutes!

## 1. Install Dependencies

### macOS (Homebrew)
```bash
brew install sfml tgui fftw opencv libsndfile pkg-config cmake
```

### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install cmake build-essential libsfml-dev libtgui-dev \
  libfftw3-dev libopencv-dev libsndfile1-dev pkg-config
```

## 2. Verify Dependencies
```bash
./check_dependencies.sh
```

## 3. Build the Application
```bash
./build.sh
```

## 4. Run the Application
```bash
cd build/bin
./FFTVidMod
```

## 5. Process Your First Video

### In the GUI:

1. **Load Media**:
   - Click "Load Video" button (left panel)
   - Enter path to your video file (e.g., `/path/to/video.mp4`)
   - Click "Load Audio" button
   - Enter path to your audio file (or use the same video file)

2. **Add Effects**:
   - Click "FFT" in the "Available Effects" list
   - Click "Add to Chain"
   - The effect appears in the "Effect Chain" list

3. **Configure** (optional):
   - Click on "1. FFT" in the chain list
   - Modify parameters in the right panel
   - Press Enter after each change

4. **Preview**:
   - Click "Preview Frame" to see the effect on one frame
   - Preview appears on the right side

5. **Process**:
   - Click "Process Video"
   - Enter output path (e.g., `output.mp4`)
   - Wait for processing to complete

## Example: Simple Audio-Reactive Video

```
1. Load video: your_video.mp4
2. Load audio: your_video.mp4 (same file)
3. Add effects in this order:
   - AudioColor (keep defaults)
   - Shadow (shadow_coeff: 0.2)
4. Preview Frame
5. Process Video -> output.mp4
```

## Troubleshooting

### "Could not open video file"
- Check the file path is correct
- Ensure OpenCV supports the video codec
- Try converting to MP4 with H.264

### "Could not open audio file"
- Check the file path is correct
- Supported formats: WAV, FLAC, OGG, MP3 (with proper codecs)

### Application crashes
- Check console output for error messages
- Verify all dependencies are installed
- Ensure video file is not corrupted

### Slow processing
- This is normal! Processing is not real-time yet
- Reduce video resolution if needed
- Use fewer effects in the chain
- Lower iteration counts for Diffuse effect

## Tips for Best Results

1. **Start with one effect** - Understand each effect before combining
2. **Use Preview** - Save time by previewing before full processing
3. **Keep parameters moderate** - Values between 0.1-0.5 are usually good
4. **Match audio to video** - Best results with synchronized audio
5. **Experiment!** - There's no wrong way to use the effects

## Next Steps

- Read [EXAMPLES.md](EXAMPLES.md) for effect chain recipes
- Read [README.md](README.md) for detailed documentation
- Experiment with different effect combinations
- Try different parameter values

Happy video processing! 🎥✨
