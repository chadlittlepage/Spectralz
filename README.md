# Spectralz

A professional spectral audio editor with AI-powered stem separation. Built in C++20 with JUCE 7+, ships as a Standalone app, VST3 plugin, and AU plugin on macOS and Windows.

## Features

- **Spectral editing** — visual, frequency-based audio editing inspired by SpectraLayers and iZotope RX
- **AI stem separation** — pluggable engine architecture supporting Demucs (6 stems: vocals, drums, bass, guitar, piano, other), Demucs htdemucs_ft (4 stems), Spleeter (2 / 4 / 5 stems), and arbitrary custom ONNX models
- **Layer system** — non-destructive editing with multiple stacked layers
- **Plugin formats** — Standalone, VST3, AU (ARA 2.0 planned)
- **Cross-platform** — macOS (Intel + Apple Silicon) and Windows
- **Audio-to-MIDI** (in development) — pitch detection, onset detection, polyphonic transcription per stem

## Requirements

- CMake 3.22+
- C++20 compiler (Clang 14+, GCC 12+, MSVC 2022+)
- JUCE 7.0.9+
- (Optional) ONNX Runtime for ML features

## Building

### 1. Get JUCE

The CMake config expects JUCE to live at `../JUCE` relative to the project root (sibling directory). Pinned to 7.0.9:

```bash
cd ..
git clone --depth 1 --branch 7.0.9 https://github.com/juce-framework/JUCE.git
cd Spectralz
```

### 2. Get the ML models (optional, only needed for stem separation)

The Spleeter and Demucs ONNX model files are large (~1 GB total) and **not stored in this repo**. They are fetched from upstream on first run. See `scripts/download_models.sh` (if present) or download manually:

- **Demucs:** https://github.com/facebookresearch/demucs (use the `htdemucs` and `htdemucs_6s` checkpoints, exported to ONNX)
- **Spleeter:** https://github.com/deezer/spleeter (2 / 4 / 5-stem checkpoints, exported to ONNX via the conversion scripts in `scripts/spleeter_conversion/`)

Place the resulting `.onnx` files under `resources/models/<engine>/<variant>/`. The app discovers them automatically at runtime.

### 3. Build

**macOS:**
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
# Outputs: build/Spectralz_artefacts/Release/{Standalone,VST3,AU}/
```

**Windows:**
```bash
cmake -B build
cmake --build build --config Release
# Outputs: build/Spectralz_artefacts/Release/{Standalone,VST3}/
```

## Project Structure

```
Spectralz/
├── src/
│   ├── Core/         # Audio engine, project state, file I/O, undo/redo
│   ├── DSP/          # FFT/STFT, phase vocoder, spectral processing
│   ├── ML/           # ONNX runtime wrapper, model management, inference threading
│   ├── UI/
│   │   ├── Components/   # Spectrogram (OpenGL), waveform overview, panels
│   │   └── ...           # Editors, layout
│   ├── Plugin/       # JUCE plugin processor + editor (VST3 / AU)
│   └── Standalone/   # Standalone application target
├── resources/        # Presets and models (models gitignored, see Building § 2)
├── scripts/          # Conversion utilities for Spleeter / Demucs → ONNX
├── tests/            # Unit + integration tests (Catch2)
└── docs/             # Architecture notes
```

### Engine architecture (stem separation)

All stem-separation engines implement a single abstract base:

```cpp
class StemSeparator {
public:
    virtual ~StemSeparator() = default;
    virtual std::vector<std::string> getAvailableStems() = 0;
    virtual void separate(const AudioBuffer& input,
                         std::map<std::string, AudioBuffer>& outputs) = 0;
};
```

Concrete implementations cover Demucs, Spleeter, and a `CustomONNX` engine for arbitrary user-supplied models. The user picks an engine in the UI; the audio thread is never blocked (inference runs on a dedicated `juce::ThreadPool`).

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
