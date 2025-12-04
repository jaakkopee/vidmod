# Future Enhancements TODO

## High Priority

- [ ] **Progress Bar**: Add visual feedback during video processing
  - Show current frame / total frames
  - Estimated time remaining
  - Cancel button to abort processing

- [ ] **Keyboard Shortcuts**: Add hotkeys for common actions
  - Ctrl+O: Load video
  - Ctrl+A: Load audio
  - Ctrl+P: Preview frame
  - Ctrl+S: Process video
  - Delete: Remove selected effect
  - Arrow keys: Navigate chain list

- [ ] **Error Handling**: Improve error messages and validation
  - Check file exists before loading
  - Validate parameter ranges
  - Show detailed error dialogs
  - Log errors to file

- [ ] **Effect Presets**: Save and load effect chains
  - JSON format for chain configuration
  - Built-in preset library
  - User preset management

## Medium Priority

- [ ] **Additional Effects**: Port remaining Python algorithms
  - FrameFeedback effect
  - AudioColor8 effect (8-channel version)
  - AudioReactiveShadow (neighborhood size variant)
  - Combined AudioLightShadow effect

- [ ] **Video Timeline**: Add timeline for scrubbing
  - Seek to specific frame
  - Show thumbnails
  - Mark keyframes

- [ ] **Batch Processing**: Process multiple videos
  - Queue system
  - Apply same chain to all
  - Sequential or parallel processing

- [ ] **Export Options**: More output formats and settings
  - Codec selection (H.264, H.265, VP9)
  - Quality/bitrate settings
  - Resolution scaling
  - Audio passthrough or re-encode

## Low Priority / Nice to Have

- [ ] **Real-time Preview**: Play video with effects
  - Requires optimization for performance
  - May need GPU acceleration
  - Lower quality/resolution for real-time

- [ ] **GPU Acceleration**: Use SFML shaders for effects
  - Port effects to GLSL
  - Massive speed improvement potential
  - Requires shader programming

- [ ] **Native File Dialogs**: Use OS file choosers
  - Better UX than text input
  - Platform-specific implementation
  - Consider portable-file-dialogs library

- [ ] **Effect Parameters Timeline**: Keyframe animation
  - Animate parameters over time
  - Interpolation between keyframes
  - Visual timeline editor

- [ ] **Audio Visualization**: Show waveform/spectrum
  - Visual feedback for audio analysis
  - Help understand FFT bands
  - Useful for syncing effects

- [ ] **Multi-threaded Processing**: Parallel frame processing
  - Process multiple frames simultaneously
  - Requires thread-safe effect implementation
  - Significant speedup potential

- [ ] **Plugin System**: Load effects from external libraries
  - Dynamic library loading
  - Effect API specification
  - Community-contributed effects

- [ ] **Network Rendering**: Distribute processing
  - Client-server architecture
  - Process on multiple machines
  - Useful for long videos

## Code Quality Improvements

- [ ] **Unit Tests**: Add test suite
  - Test each effect individually
  - Test effect chaining
  - Test audio analysis
  - Test video I/O

- [ ] **Code Documentation**: Improve inline comments
  - Doxygen-style comments
  - Generate API documentation
  - Usage examples in headers

- [ ] **Performance Profiling**: Identify bottlenecks
  - Profile each effect
  - Optimize hot paths
  - Memory usage analysis

- [ ] **Cross-platform Testing**: Verify on multiple OS
  - Linux testing
  - Windows testing (requires dependency setup)
  - CI/CD pipeline

## Documentation Improvements

- [ ] **Video Tutorials**: Create screencasts
  - Basic usage walkthrough
  - Effect showcase
  - Advanced techniques

- [ ] **Effect Gallery**: Visual examples
  - Before/after images
  - Parameter impact demonstrations
  - Best use cases

- [ ] **API Documentation**: For developers
  - How to add new effects
  - Architecture overview
  - Code organization guide

## User Experience

- [ ] **Undo/Redo**: For effect chain modifications
  - Command pattern implementation
  - History stack
  - Ctrl+Z / Ctrl+Y shortcuts

- [ ] **Drag and Drop**: For file loading
  - Drop video files on window
  - Drop audio files on window
  - More intuitive than dialogs

- [ ] **Tooltips**: Explain parameters
  - Hover over parameter name
  - Show description and range
  - Include examples

- [ ] **Recent Files**: Quick access to recent videos
  - Remember last 10 files
  - Stored in config file
  - Quick re-open

- [ ] **Settings Panel**: Application preferences
  - Default parameter values
  - Output directory
  - Performance settings
  - UI theme

## Advanced Features

- [ ] **Audio Synthesis**: Generate audio from video
  - Reverse of current process
  - Creative sound design tool
  - Experimental feature

- [ ] **3D Effects**: Depth-based processing
  - Depth estimation from video
  - Depth-aware diffusion
  - Layered effects

- [ ] **Machine Learning**: AI-powered effects
  - Style transfer
  - Content-aware processing
  - Automatic parameter tuning

- [ ] **VR Support**: 360° video processing
  - Equirectangular format support
  - Spherical coordinate effects
  - Specialized for VR content

---

## How to Contribute

If you want to implement any of these features:

1. Check this list to see what's planned
2. Create a branch for your feature
3. Implement and test thoroughly
4. Update relevant documentation
5. Submit a pull request

## Priority Legend
- High Priority: Should be implemented soon, significant user benefit
- Medium Priority: Useful features, implement when time permits
- Low Priority: Nice to have, not critical for core functionality
