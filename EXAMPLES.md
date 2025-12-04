# Example Effect Chains

This document describes some example effect chains you can create in the application.

## Basic Effects

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

### 2. Shadow Enhancement
**Single Effect**: Shadow
**Parameters**:
- shadow_coeff: 0.3

**Result**: Augmented shadows throughout the video.

### 3. Light Enhancement
**Single Effect**: Light
**Parameters**:
- light_coeff: 0.3

**Result**: Enhanced highlights and bright areas.

## Combined Effects

### 4. FFT with Shadow
**Effect Chain**:
1. FFT (fft_r: 1.2, fft_g: 1.2, fft_b: 1.2)
2. Shadow (shadow_coeff: 0.2)

**Result**: Audio-reactive colors with enhanced shadows.

### 5. Diffuse and Light
**Effect Chain**:
1. Diffuse (diffuse_coeff: 0.15, iterations: 2)
2. Light (light_coeff: 0.25)

**Result**: Smooth, blurred colors with bright highlights.

### 6. Full Audio-Reactive Suite
**Effect Chain**:
1. AudioColor (color_coeff: 1.5)
2. FFT (fft_r: 0.8, fft_g: 0.8, fft_b: 0.8)
3. Diffuse (diffuse_coeff: 0.1, iterations: 1)
4. Shadow (shadow_coeff: 0.15)
5. Light (light_coeff: 0.15)

**Result**: Complex audio-reactive visual effects with enhanced depth.

## Creative Chains

### 7. Psychedelic
**Effect Chain**:
1. FFT (fft_r: 2.0, fft_g: 1.5, fft_b: 2.5, all biases: 0)
2. Diffuse (diffuse_coeff: 0.2, iterations: 3)
3. AudioColor (color_coeff: 2.0)

**Result**: Intense, trippy visual effects.

### 8. Subtle Enhancement
**Effect Chain**:
1. Diffuse (diffuse_coeff: 0.05, iterations: 1)
2. Shadow (shadow_coeff: 0.08)
3. Light (light_coeff: 0.08)

**Result**: Subtle smoothing and depth enhancement.

### 9. Glitch Style
**Effect Chain**:
1. FFT (fft_r: 3.0, fft_g: 0.5, fft_b: 1.0)
2. AudioColor (color_coeff: 1.0)

**Result**: Glitchy, distorted colors that react to audio.

## Tips

1. **Start Simple**: Begin with single effects to understand their behavior
2. **Layer Gradually**: Add effects one at a time and preview
3. **Balance Parameters**: Lower values (0.1-0.3) are usually more subtle
4. **Audio Sync**: Effects with audio reactivity work best with music that has clear beats
5. **Performance**: More effects = slower processing. Consider the trade-off.
6. **Experimentation**: The best results often come from unexpected combinations!

## Parameter Guidelines

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
