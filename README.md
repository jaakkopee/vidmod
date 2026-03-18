# VidMod -- A Video Modulator

A C++ application for building chained video effects driven by image processing, fractal rendering, neighborhood-state updates, and audio analysis. VidMod provides a GUI for assembling effect chains, previewing frames, and rendering synchronized output from video, still images, and audio.

## Features

- **Effect Chaining**: Add multiple effects in sequence and reorder them freely
- **Mixed Algorithm Set**: Frequency-domain, spatial, iterative, fractal, and neural-style effects
- **Audio-Reactive**: Effects can respond to RMS or FFT-derived band energy
- **Real-time Preview**: Preview individual frames with effects applied
- **Automation**: Per-parameter timeline automation across effect chains
- **Save/Load Chains**: JSON serialization for effect setups

## Algorithm Families

VidMod is not a single FFT effect processor. The current effects fall into several algorithm classes:

| Effect | Algorithm Type | Core Operation |
| --- | --- | --- |
| FFT | Frequency-domain audio analysis + channel modulation | Window audio, FFT bands, add energy to B/G/R channels |
| AudioColor | Frequency-domain audio analysis + color transform | Map multi-band audio energy into RGB or HSV transforms |
| Shadow | Spatial morphology | Erode image to local minima, then blend into original |
| Light | Spatial morphology | Dilate image to local maxima, then blend into original |
| Diffuse | Iterative spatial filtering | Repeated box blur with configurable kernel growth and decay |
| Fractal | Escape-time fractal rendering | Render Julia set in image space, then blend with source |
| CircleQuilt | Grid mosaic with radial masks | Sample cell-center color, blend circular quilt over image |
| EdgeInk | Gradient edge extraction + tint composite | Sobel edge map thresholded into ink-like lines |
| CAGlow | Discrete cellular automaton glow map | Low-res CA states + structure emphasis + glow compositing |
| BitplaneReactor | Bitplane Wolfram CA | Run elementary CA on selected image bitplanes, tint active cells |
| MoldTrails | Agent-based slime simulation | Persistent trail map with steer/deposit/decay dynamics |
| NeuralTile | Grid-based neighborhood update | Build activation grid, iterate local averaging, render rectangles |
| NeuralCircle | Grid-based neighborhood update | Build activation grid, iterate local averaging, render circles |

## Effect Algorithms

### FFT
**Type**: Frequency-domain audio feature extraction

**Operation**:
- Pull one audio chunk per video frame
- Apply a Hann/Hanning window
- Split FFT output into bass, mid, and treble bands
- Add those band averages into the image channels

**Pseudocode**:
```text
audio_chunk = get_audio_for_frame()
windowed = hann(audio_chunk)
bass, mid, treble = fft_split_3(windowed)

output.B += mean(treble) * fft_b_coeff * audio_gain
output.G += mean(mid)    * fft_g_coeff * audio_gain
output.R += mean(bass)   * fft_r_coeff * audio_gain
```

**Main parameters**:
- `fft_r_coeff`, `fft_g_coeff`, `fft_b_coeff`
- `audio_gain`

### AudioColor
**Type**: Frequency-domain audio analysis + color remapping

**Operation**:
- Compute 8-band FFT features from the current audio slice
- In RGB mode, scale B/G/R channels from different band groups
- In HSV modes, rotate hue and modulate saturation/value from band energy

**Pseudocode**:
```text
bands = fft_split_8(audio_chunk)

if mode == RGB:
   red_energy   = mean(bands[0..2])
   green_energy = mean(bands[3..5])
   blue_energy  = mean(bands[6..7])
   scale channels by energy

if mode == HSV:
   hue_shift        = band-derived hue rotation
   saturation_scale = rms-derived modulation
   value_scale      = peak/rms-derived modulation
```

**Main parameters**:
- `color_coeff`, `mode`
- `hue_strength`, `saturation_strength`, `value_strength`
- `audio_gain`

### Shadow
**Type**: Spatial morphology

**Operation**:
- Compute a local-minima image using erosion
- Blend minima back into the source to deepen dark structure

**Pseudocode**:
```text
minima = erode(frame, kernel_size, morph_iterations)
output = lerp(frame, minima, shadow_coeff * audio_scale)
```

**Main parameters**:
- `shadow_coeff`
- `kernel_size`, `morph_iterations`
- `audio_gain`

### Light
**Type**: Spatial morphology

**Operation**:
- Compute a local-maxima image using dilation
- Blend maxima back into the source to emphasize highlights

**Pseudocode**:
```text
maxima = dilate(frame, kernel_size, morph_iterations)
output = lerp(frame, maxima, light_coeff * audio_scale)
```

**Main parameters**:
- `light_coeff`
- `kernel_size`, `morph_iterations`
- `audio_gain`

### Diffuse
**Type**: Iterative spatial filtering

**Operation**:
- Convert the frame to float
- Repeatedly blur it with a configurable box filter
- Blend the blurred result back into the current image each pass
- Optionally grow kernel size and decay the blend strength over iterations

**Pseudocode**:
```text
state = frame
for i in 0..iterations-1:
   k = kernel_size + 2 * kernel_growth * i
   blurred = box_filter(state, k)
   coeff_i = diffuse_coeff * iteration_decay^i
   state = lerp(state, blurred, coeff_i * audio_scale)
```

**Main parameters**:
- `diffuse_coeff`, `iterations`
- `kernel_size`, `kernel_growth`, `iteration_decay`
- `audio_gain`

### Fractal
**Type**: Escape-time fractal rendering

**Operation**:
- Map each pixel into the complex plane
- Iterate the Julia recurrence $z_{n+1} = z_n^2 + c$
- Use escape iteration count to color the pixel
- Optionally blend the fractal with the source image

**Pseudocode**:
```text
for each pixel (x, y):
   z0 = map_pixel_to_complex_plane(x, y, zoom, center_x, center_y)
   z0 += source_brightness * input_warp
   iter = julia_escape_time(z0, c=(cx, cy), max_iter)
   fractal_color = palette(iter / max_iter)
   output = lerp(source_color, fractal_color, blend)
```

**Main parameters**:
- `zoom`, `center_x`, `center_y`
- `cx`, `cy`, `max_iter`
- `blend`, `input_warp`, `color_source_mix`
- `audio_depth`, `audio_gain`

### CircleQuilt
**Type**: Grid-based mosaic sampling

**Operation**:
- Split the frame into a configurable grid
- Sample representative color per cell center
- Build circular masks with soft edges
- Composite a quilt-like color field over the original frame

**Main parameters**:
- `cells_x`, `cells_y`
- `radius_scale`, `edge_softness`
- `blend`, `audio_gain`

### EdgeInk
**Type**: Edge extraction + stylized compositing

**Operation**:
- Blur input to stabilize gradients
- Compute Sobel edge magnitude
- Threshold and optionally invert edges
- Composite selected ink tint color over source image where edges are active

**Main parameters**:
- `edge_threshold`, `blur_size`
- `edge_strength`, `invert`
- `ink_r`, `ink_g`, `ink_b`
- `blend`, `audio_gain`

### CAGlow
**Type**: Low-resolution discrete cellular automaton + glow

**Operation**:
- Downsample frame to simulation grid
- Quantize to CA states and iterate neighborhood transitions
- Emphasize structure from CA activation and Laplacian energy
- Blur and upsample activation map, then tint-composite as glow

**Main parameters**:
- `iterations`, `state_count`, `neighbor_mix`
- `sim_scale`, `blur_size`, `contrast`
- `glow_strength`, `blend`
- `tint_r`, `tint_g`, `tint_b`, `audio_gain`

### BitplaneReactor
**Type**: Bitplane cellular automaton pattern synthesis

**Operation**:
- Extract selected grayscale bitplanes
- Run Wolfram elementary CA row-to-row on each plane
- Aggregate active states and tint-composite onto the source
- Audio can modulate blend and dynamic CA behavior

**Main parameters**:
- `bit_lo`, `bit_count`
- `wolfram_rule`, `sim_scale`
- `blend`
- `tint_r`, `tint_g`, `tint_b`, `audio_gain`

### MoldTrails
**Type**: Agent-based trail simulation (physarum-style)

**Operation**:
- Maintain persistent trail map and moving agent population
- Agents sample left/center/right sensors, steer, move, and deposit trail
- Diffuse and decay trail field each frame
- Composite trail intensity back with configurable tint

**Main parameters**:
- `agent_count`, `sensor_angle`, `sensor_dist`
- `turn_speed`, `move_speed`
- `deposit_amount`, `decay_rate`, `blur_size`
- `sim_scale`, `blend`
- `tint_r`, `tint_g`, `tint_b`, `audio_gain`

### NeuralTile
**Type**: Grid-based iterative neighborhood system

**Operation**:
- Build a low-resolution neuron grid from frame brightness
- Update activations from neighboring cells for several iterations
- Render the grid back as rectangles with activation-based size and motion

**Pseudocode**:
```text
grid = sample_frame_into_tiles(frame)
for i in 0..iterations-1:
   for each cell:
      input = self + selected_neighbors(mode)
      next = activation_function(average(input))
render each cell as rectangle(size, offset, color from activation)
```

**Main parameters**:
- `tileSize`, `threshold`, `mode`, `iterations`
- `movement`, `audioMod`, `feedback`, `audio_gain`

### NeuralCircle
**Type**: Grid-based iterative neighborhood system

**Operation**:
- Same update model as `NeuralTile`
- Render the activation grid as circles instead of rectangles

**Pseudocode**:
```text
grid = sample_frame_into_cells(frame)
iterate neighbor-averaged activations
render each cell as circle(radius, offset, intensity)
```

**Main parameters**:
- `circleSize`, `threshold`, `mode`, `iterations`
- `movement`, `audioMod`, `feedback`, `audio_gain`

## Processing Model

Effect chains are applied sequentially. All effects in one video frame read the same audio slice, then the audio cursor advances once for the next frame.

**Pseudocode**:
```text
saved_audio_index = audio.index
result = frame

for effect in chain:
   audio.index = saved_audio_index
   result = effect.apply(result, audio, fps)

audio.index = saved_audio_index + samples_per_video_frame
```

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
./bin/VidMod
```

## Usage

1. **Launch the application**:
   ```bash
   ./bin/VidMod
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

## Notes On Parameters

- Parameters are stored as floats, but some are treated as integer-like during processing, such as `iterations`, `kernel_size`, and `morph_iterations`.
- Audio-reactive effects usually expose an `audio_gain` parameter, which scales how strongly audio influences the main coefficients.
- For more concrete recipes, see [EXAMPLES.md](EXAMPLES.md).

## Project Structure

```
vidmod/
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
- **Performance**: Spatial filters, fractal rendering, and iterative grid effects can be significantly heavier than simple channel modulation.
- **File Formats**: Supports common video formats (MP4, AVI, MOV) and audio formats (WAV, FLAC, OGG).

## Future Enhancements

- Save/load effect chain presets
- Real-time video playback with effects
- GPU acceleration using shaders
- Additional effects (blur, edge detection, etc.)
- Batch processing multiple videos
- Timeline-based effect keyframing

## Credits

Based on the Python `fftvidmod2.py` script, converted to C++ with a modern GUI interface for VidMod.
