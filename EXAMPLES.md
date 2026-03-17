# Example Effect Chains

This document describes some example effect chains you can create in the application.

## By Algorithm Family

This document is organized by the kind of processing each effect performs, rather than by a vague “basic vs creative” split.

## Frequency-Domain Audio Effects

### 1. Audio-Reactive FFT
**Single Effect**: FFT
**Parameters**:
- fft_r_coeff: 1.5
- fft_g_coeff: 1.2
- fft_b_coeff: 1.8
- red_bias: 32
- green_bias: 32
- blue_bias: 32

**Result**: Video colors modulated by audio frequency bands.

### 2. AudioColor Spectrum Mapping
**Single Effect**: AudioColor
**Parameters**:
- color_coeff: 1.0
- mode: 2
- hue_strength: 1.0
- saturation_strength: 1.2
- value_strength: 0.8
- audio_gain: 0.7

**Result**: Frequency bands map into hue, saturation, and brightness shifts.

### 3. Glitch Style
**Effect Chain**:
1. FFT (fft_r: 3.0, fft_g: 0.5, fft_b: 1.0)
2. AudioColor (color_coeff: 1.0)

**Result**: Glitchy, distorted colors that react strongly to audio.

## Spatial Morphology Effects

### 4. Shadow Enhancement
**Single Effect**: Shadow
**Parameters**:
- shadow_coeff: 0.3
- kernel_size: 5
- morph_iterations: 1
- audio_gain: 0.6

**Result**: Augmented shadows throughout the video.

### 5. Light Enhancement
**Single Effect**: Light
**Parameters**:
- light_coeff: 0.3
- kernel_size: 5
- morph_iterations: 1
- audio_gain: 0.6

**Result**: Enhanced highlights and bright areas.

### 6. FFT with Shadow
**Effect Chain**:
1. FFT (fft_r: 1.2, fft_g: 1.2, fft_b: 1.2)
2. Shadow (shadow_coeff: 0.2)

**Result**: Audio-reactive colors with enhanced shadows.

## Iterative Spatial Filtering

### 7. Diffuse and Light
**Effect Chain**:
1. Diffuse (diffuse_coeff: 0.15, iterations: 2, kernel_size: 5, kernel_growth: 1, iteration_decay: 0.9)
2. Light (light_coeff: 0.25, kernel_size: 5, morph_iterations: 1)

**Result**: Smooth, blurred colors with bright highlights.

### 8. Subtle Enhancement
**Effect Chain**:
1. Diffuse (diffuse_coeff: 0.05, iterations: 1, kernel_size: 3, kernel_growth: 0, iteration_decay: 1.0)
2. Shadow (shadow_coeff: 0.08, kernel_size: 3, morph_iterations: 1)
3. Light (light_coeff: 0.08, kernel_size: 3, morph_iterations: 1)

**Result**: Subtle smoothing and depth enhancement.

### 9. Evolving Bloom (Advanced)
**Effect Chain**:
1. Diffuse (diffuse_coeff: 0.16, iterations: 4, kernel_size: 3, kernel_growth: 1, iteration_decay: 0.82, audio_gain: 0.55)
2. Light (light_coeff: 0.12, kernel_size: 5, morph_iterations: 2, audio_gain: 0.40)
3. AudioColor (color_coeff: 0.9, mode: 2, hue_strength: 0.7, saturation_strength: 1.1, value_strength: 0.9, audio_gain: 0.5)

**Result**: A soft bloom that grows across iterations while preserving detail in bright zones.

**Automation idea**:
- Animate `Diffuse.kernel_growth` from `0 -> 2` over the song section for expanding glow.
- Animate `Diffuse.iteration_decay` from `1.0 -> 0.75` to make later diffusion passes stronger relative to early ones.
- Keep `Light.morph_iterations` fixed to avoid flicker while the bloom evolves.

## Composite Multi-Algorithm Chains

### 10. Full Audio-Reactive Suite
**Effect Chain**:
1. AudioColor (color_coeff: 1.5, mode: 2, hue_strength: 1.0, saturation_strength: 1.2, value_strength: 0.8, audio_gain: 0.7)
2. FFT (fft_r: 0.8, fft_g: 0.8, fft_b: 0.8)
3. Diffuse (diffuse_coeff: 0.1, iterations: 1, kernel_size: 3, kernel_growth: 0, iteration_decay: 1.0, audio_gain: 0.5)
4. Shadow (shadow_coeff: 0.15, kernel_size: 3, morph_iterations: 1, audio_gain: 0.5)
5. Light (light_coeff: 0.15, kernel_size: 3, morph_iterations: 1, audio_gain: 0.5)

**Result**: Complex audio-reactive visual effects with enhanced depth.

### 11. Psychedelic
**Effect Chain**:
1. FFT (fft_r: 2.0, fft_g: 1.5, fft_b: 2.5, all biases: 0)
2. Diffuse (diffuse_coeff: 0.2, iterations: 3)
3. AudioColor (color_coeff: 2.0)

**Result**: Intense, trippy visual effects.

## Tips

1. **Start Simple**: Begin with single effects to understand their behavior
2. **Layer Gradually**: Add effects one at a time and preview
3. **Balance Parameters**: Lower values (0.1-0.3) are usually more subtle
4. **Audio Sync**: Effects with audio reactivity work best with music that has clear beats
5. **Performance**: More effects = slower processing. Consider the trade-off.
6. **Experimentation**: The best results often come from unexpected combinations!

## Newly Exposed Parameters

### Shadow
- `shadow_coeff`: Blend amount for local-minima shadowing
- `kernel_size`: Erosion kernel size (odd values work best)
- `morph_iterations`: Number of erosion passes
- `audio_gain`: Audio influence on `shadow_coeff`

Suggested start: `shadow_coeff=0.15`, `kernel_size=3`, `morph_iterations=1`, `audio_gain=0.5`

### Light
- `light_coeff`: Blend amount for local-maxima highlighting
- `kernel_size`: Dilation kernel size (odd values work best)
- `morph_iterations`: Number of dilation passes
- `audio_gain`: Audio influence on `light_coeff`

Suggested start: `light_coeff=0.15`, `kernel_size=3`, `morph_iterations=1`, `audio_gain=0.5`

### AudioColor
- `color_coeff`: Overall color modulation amount
- `mode`: `0=RGB`, `1=HSV dominant`, `2=HSV spectrum`
- `hue_strength`: Strength of hue rotation/shifts
- `saturation_strength`: Strength of saturation modulation
- `value_strength`: Strength of brightness/value modulation
- `audio_gain`: Overall audio influence scale

Suggested start: `color_coeff=1.0`, `mode=2`, `hue_strength=1.0`, `saturation_strength=1.2`, `value_strength=0.8`, `audio_gain=0.7`

### Diffuse
- `diffuse_coeff`: Base blend amount per iteration
- `iterations`: Number of diffusion passes
- `kernel_size`: Base box-filter kernel size (odd values)
- `kernel_growth`: Per-iteration kernel growth (0 keeps fixed kernel)
- `iteration_decay`: Multiplies diffusion strength each pass
- `audio_gain`: Audio influence on `diffuse_coeff`

Suggested start: `diffuse_coeff=0.12`, `iterations=3`, `kernel_size=5`, `kernel_growth=1`, `iteration_decay=0.9`, `audio_gain=0.5`

## Audio Gain Guidelines

`audio_gain` is now available on all effects and controls how strongly audio affects processing coefficients.

- `audio_gain = 0.0`: disable audio influence (effect uses only its base parameters)
- `audio_gain = 1.0`: default behavior
- `audio_gain > 1.0`: exaggerated audio reaction (can become unstable/noisy)

Recommended starting ranges:

- FFT: `0.3 - 1.2`
- Shadow: `0.2 - 1.0`
- Light: `0.2 - 1.0`
- Diffuse: `0.2 - 0.9`
- AudioColor: `0.4 - 1.5`
- Fractal: `0.2 - 1.0`
- NeuralTile: `0.3 - 1.2`
- NeuralCircle: `0.3 - 1.2`

Quick tuning workflow:

1. Set your main effect parameters first (coefficients, iterations, etc.).
2. Start with `audio_gain = 0.5`.
3. Increase in small steps (`+0.1`) until beat response is visible.
4. If visuals flicker too hard, reduce `audio_gain` before lowering other parameters.

### Conservative (Subtle Effects)
- Coefficients: 0.05 - 0.15
- Iterations: 1
- Biases: 32-64

### Moderate (Visible Effects)
- Coefficients: 0.2 - 0.5
- Iterations: 1-2
- Biases: 16-48

### Aggressive (Intense Effects)
- Coefficients: 0.6 - 2.0+
- Iterations: 2-5
- Biases: 0-16 or 80+
