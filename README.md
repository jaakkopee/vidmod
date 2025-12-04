# FFT Video Modulator - C++ Version

A C++ application for applying audio-reactive visual effects to videos using FFT analysis. This application provides a GUI for chaining multiple effects and processing videos with audio synchronization.

## Features

- **Effect Chaining**: Add multiple effects in sequence and reorder them freely
- **Audio-Reactive**: Effects respond to audio analysis using FFT
- **Real-time Preview**: Preview individual frames with effects applied
- **Multiple Effects**:
  - **FFT Effect**: Modulates video channels based on audio frequency bands
  - **Shadow Effect**: Enhances shadows using local minima detection
  - **Light Effect**: Enhances highlights using local maxima detection
  - **Diffuse Effect**: Applies color diffusion across neighboring pixels
  - **Audio Color Effect**: Maps audio frequency bands to RGB channels

## Dependencies

The following libraries are required:

- **SFML 2.5+**: Graphics and window management
- **TGUI 0.9+**: GUI framework
- **FFTW3**: Fast Fourier Transform library
- **OpenCV 4**: Video processing
- **libsndfile**: Audio file loading

### Installing Dependencies on macOS

```bash
# Using Homebrew
brew install sfml tgui fftw opencv libsndfile pkg-config
```

### Installing Dependencies on Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install libsfml-dev libtgui-dev libfftw3-dev libopencv-dev libsndfile1-dev pkg-config
```

## Building

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build
make

# The executable will be in build/bin/
./bin/FFTVidMod
```

## Usage

1. **Launch the application**:
   ```bash
   ./bin/FFTVidMod
   ```

2. **Load Media Files**:
   - Click "Load Video" to select a video file
   - Click "Load Audio" to select an audio file (or use video's audio)

3. **Build Effect Chain**:
   - Select an effect from "Available Effects" list
   - Click "Add to Chain" to add it to the processing chain
   - Use "Move Up"/"Move Down" to reorder effects
   - Use "Remove" to delete selected effect

4. **Configure Parameters**:
   - Select an effect in the chain
   - Adjust parameters in the "Parameters" panel
   - Press Enter after editing each parameter

5. **Preview**:
   - Click "Preview Frame" to see effects applied to a single frame
   - The preview appears on the right side of the window

6. **Process Video**:
   - Click "Process Video"
   - Enter output file path when prompted
   - Wait for processing to complete

## Effect Parameters

### FFT Effect
- `fft_r_coeff`: Red channel FFT coefficient (default: 1.0)
- `fft_g_coeff`: Green channel FFT coefficient (default: 1.0)
- `fft_b_coeff`: Blue channel FFT coefficient (default: 1.0)
- `red_bias`: Red channel bias (default: 64)
- `green_bias`: Green channel bias (default: 64)
- `blue_bias`: Blue channel bias (default: 64)

### Shadow Effect
- `shadow_coeff`: Shadow intensity coefficient (default: 0.1)

### Light Effect
- `light_coeff`: Light intensity coefficient (default: 0.1)

### Diffuse Effect
- `diffuse_coeff`: Diffusion coefficient (default: 0.1)
- `iterations`: Number of diffusion iterations (default: 1)

### Audio Color Effect
- `color_coeff`: Color modulation coefficient (default: 1.0)

## Project Structure

```
fftvidmodcpp/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── AudioBuffer.h
│   ├── AudioColorEffect.h
│   ├── CircularBuffer.h
│   ├── DiffuseEffect.h
│   ├── Effect.h
│   ├── EffectChain.h
│   ├── FFTEffect.h
│   ├── GUI.h
│   ├── LightEffect.h
│   ├── ShadowEffect.h
│   ├── VideoBuffer.h
│   └── VideoProcessor.h
└── src/
    ├── AudioBuffer.cpp
    ├── AudioColorEffect.cpp
    ├── CircularBuffer.cpp
    ├── DiffuseEffect.cpp
    ├── Effect.cpp
    ├── EffectChain.cpp
    ├── FFTEffect.cpp
    ├── GUI.cpp
    ├── LightEffect.cpp
    ├── ShadowEffect.cpp
    ├── VideoBuffer.cpp
    ├── VideoProcessor.cpp
    └── main.cpp
```

## Notes

- **Memory Usage**: The application loads entire videos into memory for processing. Large videos may require significant RAM.
- **Performance**: Effect processing is not yet optimized for real-time. Processing time depends on video resolution, duration, and number of effects.
- **File Formats**: Supports common video formats (MP4, AVI, MOV) and audio formats (WAV, FLAC, OGG)

## Future Enhancements

- Save/load effect chain presets
- Real-time video playback with effects
- GPU acceleration using shaders
- Additional effects (blur, edge detection, etc.)
- Batch processing multiple videos
- Timeline-based effect keyframing

## Credits

Based on the Python `fftvidmod2.py` script, converted to C++ with a modern GUI interface.
