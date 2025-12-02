# Spectralz

A professional spectral audio editor with AI-powered stem separation.

![Build Status](https://github.com/chadlittlepage/Spectralz/actions/workflows/build.yml/badge.svg)

## Features

- **Spectral Editing**: Visual frequency-based audio editing
- **AI Stem Separation**: Powered by Demucs/ONNX (6 stems: vocals, drums, bass, guitar, piano, other)
- **Layer System**: Non-destructive editing with multiple layers
- **Plugin Formats**: VST3, AU, Standalone (ARA planned)
- **Cross-Platform**: macOS and Windows

## Requirements

- CMake 3.22+
- C++20 compiler (Clang 14+, GCC 12+, MSVC 2022+)
- JUCE 7.0.9+
- (Optional) ONNX Runtime for ML features

## Building

### macOS

```bash
# Clone with submodules
git clone --recursive https://github.com/chadlittlepage/Spectralz.git
cd Spectralz

# Ensure JUCE is available (adjust path if needed)
# Default expects JUCE at ../JUCE

# Configure and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Outputs in build/Spectralz_artefacts/Release/
```

### Windows

```bash
cmake -B build
cmake --build build --config Release
```

## Project Structure

```
Spectralz/
├── src/
│   ├── Core/       # Audio engine, project state
│   ├── DSP/        # FFT, spectral processing, stem separation
│   ├── ML/         # ONNX inference
│   ├── UI/         # Spectrogram, waveform, panels
│   └── Plugin/     # JUCE plugin wrapper
├── resources/      # Models, presets
├── tests/          # Unit and integration tests
└── docs/           # Documentation
```

## Development

### Code Style

This project uses clang-format and clang-tidy. Format before committing:

```bash
# Format all source files
find src -name '*.cpp' -o -name '*.h' | xargs clang-format -i
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```

## License

Proprietary - All Rights Reserved

## Acknowledgments

- [JUCE](https://juce.com/) - Audio application framework
- [Demucs](https://github.com/facebookresearch/demucs) - ML stem separation model
- [ONNX Runtime](https://onnxruntime.ai/) - ML inference
