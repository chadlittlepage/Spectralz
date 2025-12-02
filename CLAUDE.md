# Spectralz - AI-Powered Spectral Audio Editor

## Project Overview
Spectralz is a professional spectral audio editing application inspired by SpectraLayers and iZotope RX. It provides advanced audio visualization, editing, and AI-powered stem separation capabilities.

## Architecture

### Tech Stack
- **Framework**: JUCE 7+ (C++20)
- **Build System**: CMake 3.22+
- **ML Inference**: ONNX Runtime
- **Plugin Formats**: VST3, AU, Standalone (ARA planned)
- **Testing**: Catch2

### Directory Structure
```
Spectralz/
├── src/
│   ├── Core/           # Audio engine, project state, core processing
│   ├── DSP/            # FFT, spectral editing, stem separation
│   ├── ML/             # ONNX inference, model management
│   ├── UI/
│   │   ├── Components/ # Spectrogram, waveform, panels
│   │   └── Editors/    # Main editor window
│   └── Plugin/         # JUCE plugin processor/editor
├── resources/
│   ├── models/         # ONNX models (Demucs, etc.)
│   └── presets/        # User presets
├── tests/              # Unit tests
├── scripts/            # Build and utility scripts
└── docs/               # Documentation
```

## Key Components

### SpectralProcessor (src/Core/)
Central audio processing engine. Handles:
- Audio file loading/saving
- Real-time spectral analysis
- Layer management
- Undo/redo system

### FFTProcessor (src/DSP/)
FFT and STFT processing using JUCE DSP or FFTW.
- Window functions (Hann, Blackman-Harris, etc.)
- Overlap-add synthesis
- Phase vocoder

### SpectrogramView (src/UI/Components/)
OpenGL-accelerated spectrogram display.
- Logarithmic frequency scale
- Color mapping (magnitude to color)
- Selection and editing tools
- Real-time updates

### ONNXInference (src/ML/)
Wrapper for ONNX Runtime.
- Model loading and caching
- Batched inference
- Thread-safe processing

## Coding Standards

### Style Guide
- Use clang-format (config in .clang-format)
- Follow JUCE naming conventions:
  - Classes: PascalCase
  - Methods: camelCase
  - Members: camelCase with no prefix
  - Constants: UPPER_SNAKE_CASE
- Prefer `auto` when type is obvious
- Use `[[nodiscard]]` for non-void returns
- Use `override` and `final` appropriately

### Memory Management
- Use JUCE's memory management (OwnedArray, ReferenceCountedObject)
- Prefer `std::unique_ptr` over raw pointers
- Use `juce::ScopedPointer` sparingly (deprecated)

### Threading
- Audio thread: No allocations, no locks, no I/O
- Use `juce::MessageManager` for UI updates from background threads
- Use `juce::ThreadPool` for background processing
- ML inference runs on dedicated thread pool

## Build Commands

```bash
# Configure (macOS)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Build specific target
cmake --build build --target Spectralz_Standalone

# Run tests
cd build && ctest --output-on-failure
```

## Current Development Phase
Phase 1: Foundation
- [x] Project structure
- [ ] Basic audio I/O
- [ ] Spectrogram display
- [ ] FFT processing

## Important Notes
- JUCE path: `../JUCE` (relative to project root)
- Primary color: #4a556c
- Target: macOS 11+, Windows 10+
- Min C++ standard: C++20

## External Dependencies
- JUCE: Audio framework (submodule or external)
- ONNX Runtime: ML inference (vcpkg or manual)
- Catch2: Testing (fetched via CMake)
- Sentry: Crash reporting (optional)

## Testing Strategy
- Unit tests for DSP algorithms
- Integration tests for audio processing chain
- Visual regression tests for UI components

## Performance Targets
- Spectrogram: 60fps at 4K resolution
- FFT: < 5ms for 4096-point FFT
- Stem separation: Near real-time on M1/M2
