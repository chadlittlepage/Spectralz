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

## Platform Support

| Platform | Standalone | VST3 | AU | ARA |
|----------|------------|------|-----|-----|
| macOS (Intel + Apple Silicon) | ✅ | ✅ | ✅ | ✅ |
| Windows (x64) | ✅ | ✅ | ❌ | ✅ |
| Linux (x64) | ✅ | ✅ | ❌ | ✅ |

## ML Engine Support

Users can choose their stem separation engine:

| Engine | Stems | Quality | Speed |
|--------|-------|---------|-------|
| Demucs htdemucs_6s | 6 (vocals, drums, bass, guitar, piano, other) | Best | Slower |
| Demucs htdemucs_ft | 4 (vocals, drums, bass, other) | Very Good | Medium |
| Spleeter 5stems | 5 (vocals, drums, bass, piano, other) | Good | Fast |
| Spleeter 2stems | 2 (vocals, accompaniment) | Good | Fastest |
| Custom ONNX | Variable | Variable | Variable |

All engines converted to ONNX format for unified inference pipeline.

### Engine Architecture
```cpp
class StemSeparator {
public:
    virtual ~StemSeparator() = default;
    virtual std::vector<std::string> getAvailableStems() = 0;
    virtual void separate(const AudioBuffer& input,
                         std::map<std::string, AudioBuffer>& outputs) = 0;
};
```

## Development Phases

### Phase 1: Foundation (Standalone)
- [x] Project structure
- [ ] Audio file I/O (WAV, AIFF, MP3, FLAC)
- [ ] Audio playback engine
- [ ] FFT/STFT processor
- [ ] Spectrogram display (OpenGL)
- [ ] Waveform overview
- [ ] Basic transport controls

### Phase 2: Spectral Editing (Standalone)
- [ ] Selection tools (time/frequency)
- [ ] Brush tools (attenuate/boost)
- [ ] Eraser tool
- [ ] Copy/paste spectral data
- [ ] Undo/redo system
- [ ] Layer system
- [ ] Phase vocoder

### Phase 3: ML Stem Separation (Standalone)
- [ ] ONNX Runtime integration
- [ ] Demucs model export to ONNX
- [ ] Spleeter model export to ONNX
- [ ] Engine selection UI
- [ ] Background processing
- [ ] Model manager (download/cache)

### Phase 3.5: Audio-to-MIDI Conversion (Standalone)
- [ ] Pitch detection (YIN, pYIN, or CREPE-based)
- [ ] Onset detection for note timing
- [ ] Velocity estimation from amplitude
- [ ] Drum transcription (kick, snare, hi-hat, etc.)
- [ ] Polyphonic pitch detection for piano/guitar
- [ ] MIDI file export (.mid)
- [ ] Per-stem MIDI generation:
  - Vocals → Melody MIDI
  - Bass → Bass MIDI (monophonic)
  - Drums → Drum Map MIDI (GM standard)
  - Guitar → Guitar MIDI (polyphonic)
  - Piano → Piano MIDI (polyphonic)
  - Other → Best-effort MIDI
- [ ] Quantization options (1/4, 1/8, 1/16, triplets)
- [ ] Key/scale detection for note correction

### Phase 4: Plugin Formats
- [ ] VST3 plugin build
- [ ] AU plugin build
- [ ] Parameter automation
- [ ] Preset system

### Phase 5: ARA Integration
- [ ] ARA 2.0 SDK integration
- [ ] DAW timeline access
- [ ] Edit transfer

### Phase 6: Audio Repair Tools
- [ ] De-noise
- [ ] De-click
- [ ] De-clip
- [ ] De-reverb
- [ ] Spectral repair

### Phase 7: Advanced Features
- [ ] Batch processing
- [ ] Custom model import
- [ ] Surround support

### Phase 8: Polish & Ship
- [ ] Performance optimization
- [ ] UI/UX polish
- [ ] Installers (DMG/MSI)
- [ ] Documentation

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
