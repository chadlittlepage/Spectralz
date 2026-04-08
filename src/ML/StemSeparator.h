#pragma once

#if SPECTRALZ_ENABLE_ML

#include "ONNXInference.h"
#include "ModelManager.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <map>
#include <functional>
#include <atomic>

namespace spectralz
{

// Separation result container
struct SeparationResult
{
    std::map<std::string, juce::AudioBuffer<float>> stems;  // stem name -> audio buffer
    bool success = false;
    std::string errorMessage;
    double sampleRate = 44100.0;

    [[nodiscard]] size_t getStemCount() const { return stems.size(); }
    [[nodiscard]] bool hasStem(const std::string& name) const
    {
        return stems.find(name) != stems.end();
    }
};

// Progress callback: (progress 0.0-1.0, current operation description)
using SeparationProgressCallback = std::function<void(float, const std::string&)>;

// Abstract interface for stem separation engines
class IStemSeparator
{
public:
    virtual ~IStemSeparator() = default;

    // Load a model for this engine
    virtual bool loadModel(const ModelInfo& model) = 0;
    virtual void unloadModel() = 0;
    [[nodiscard]] virtual bool isModelLoaded() const = 0;

    // Get available stems for the currently loaded model
    [[nodiscard]] virtual std::vector<std::string> getAvailableStems() const = 0;

    // Perform separation
    [[nodiscard]] virtual SeparationResult separate(
        const juce::AudioBuffer<float>& input,
        double sampleRate,
        SeparationProgressCallback progressCallback = nullptr,
        std::atomic<bool>* cancelFlag = nullptr) = 0;

    // Engine info
    [[nodiscard]] virtual std::string getEngineName() const = 0;
    [[nodiscard]] virtual SeparationEngine getEngineType() const = 0;
};

// Factory function to create appropriate engine
std::unique_ptr<IStemSeparator> createStemSeparator(SeparationEngine engine);

// Demucs-specific stem separator
class DemucsEngine : public IStemSeparator
{
public:
    DemucsEngine();
    ~DemucsEngine() override;

    bool loadModel(const ModelInfo& model) override;
    void unloadModel() override;
    [[nodiscard]] bool isModelLoaded() const override;

    [[nodiscard]] std::vector<std::string> getAvailableStems() const override;

    [[nodiscard]] SeparationResult separate(
        const juce::AudioBuffer<float>& input,
        double sampleRate,
        SeparationProgressCallback progressCallback = nullptr,
        std::atomic<bool>* cancelFlag = nullptr) override;

    [[nodiscard]] std::string getEngineName() const override { return "Demucs"; }
    [[nodiscard]] SeparationEngine getEngineType() const override { return SeparationEngine::Demucs; }

private:
    std::unique_ptr<ONNXInference> inference;
    ModelInfo currentModel;
    bool modelLoaded = false;

    // Demucs-specific processing parameters (htdemucs v4 hybrid transformer)
    // The ONNX model expects:
    //   Input 1: "input" - waveform [1, 2, SEGMENT_SAMPLES]
    //   Input 2: "x" - spectrogram [1, 4, STFT_FREQ_BINS, STFT_TIME_FRAMES] (complex-as-channels)
    //   Output: "add_67" - stems [1, 4, 2, SEGMENT_SAMPLES] (4 stems, stereo)
    static constexpr int SEGMENT_SAMPLES = 343980;  // ~7.8 seconds at 44.1kHz
    static constexpr int OVERLAP_SAMPLES = 44100;   // 1 second overlap for chunked processing
    static constexpr int TARGET_SAMPLE_RATE = 44100;
    static constexpr int STFT_NFFT = 4096;          // FFT size for spectrogram
    static constexpr int STFT_HOP = 1024;           // Hop size (nfft/4)
    static constexpr int STFT_FREQ_BINS = 2048;     // nfft/2 (DC to Nyquist)
    static constexpr int STFT_TIME_FRAMES = 336;    // Time frames for segment

    // Process a single chunk through the model
    SeparationResult processChunk(
        const juce::AudioBuffer<float>& chunk,
        double sampleRate);

    // Resample if needed
    juce::AudioBuffer<float> resampleBuffer(
        const juce::AudioBuffer<float>& input,
        double sourceSampleRate,
        double targetSampleRate);

    // Compute STFT and format as complex-as-channels for Demucs
    void computeDemucsSTFT(
        const juce::AudioBuffer<float>& stereoInput,
        std::vector<float>& spectrogramOutput);
};

// Spleeter-specific stem separator
class SpleeterEngine : public IStemSeparator
{
public:
    SpleeterEngine();
    ~SpleeterEngine() override;

    bool loadModel(const ModelInfo& model) override;
    void unloadModel() override;
    [[nodiscard]] bool isModelLoaded() const override;

    [[nodiscard]] std::vector<std::string> getAvailableStems() const override;

    [[nodiscard]] SeparationResult separate(
        const juce::AudioBuffer<float>& input,
        double sampleRate,
        SeparationProgressCallback progressCallback = nullptr,
        std::atomic<bool>* cancelFlag = nullptr) override;

    [[nodiscard]] std::string getEngineName() const override { return "Spleeter"; }
    [[nodiscard]] SeparationEngine getEngineType() const override { return SeparationEngine::Spleeter; }

private:
    // For multi-file models: one inference per stem
    std::map<std::string, std::unique_ptr<ONNXInference>> stemInferences;
    // For single-file models (legacy support)
    std::unique_ptr<ONNXInference> inference;
    ModelInfo currentModel;
    bool modelLoaded = false;

    // Spleeter-specific processing parameters
    // For sherpa-onnx converted models: input shape [2, num_splits, 512, 1024]
    static constexpr int STFT_FRAME_LENGTH = 1024;  // FFT size -> 512 bins
    static constexpr int STFT_HOP_LENGTH = 512;     // 50% overlap
    static constexpr int SPEC_TIME_FRAMES = 1024;   // Time frames per chunk
    static constexpr int SPEC_FREQ_BINS = 512;      // Frequency bins (FFT_size/2)
    static constexpr int TARGET_SAMPLE_RATE = 44100;

    // STFT processing helpers
    std::vector<std::complex<float>> computeSTFT(
        const float* input,
        int numSamples,
        int fftSize,
        int hopSize);

    juce::AudioBuffer<float> computeISTFT(
        const std::vector<std::complex<float>>& stft,
        int numSamples,
        int fftSize,
        int hopSize);

    // Run inference on a single stem model
    InferenceResult runStemInference(
        const std::string& stemName,
        const std::vector<float>& inputData,
        const TensorShape& inputShape);
};

// Python Demucs engine - calls terminal demucs directly for 100% identical output
class PythonDemucsEngine : public IStemSeparator
{
public:
    PythonDemucsEngine();
    ~PythonDemucsEngine() override;

    bool loadModel(const ModelInfo& model) override;
    void unloadModel() override;
    [[nodiscard]] bool isModelLoaded() const override;

    [[nodiscard]] std::vector<std::string> getAvailableStems() const override;

    [[nodiscard]] SeparationResult separate(
        const juce::AudioBuffer<float>& input,
        double sampleRate,
        SeparationProgressCallback progressCallback = nullptr,
        std::atomic<bool>* cancelFlag = nullptr) override;

    [[nodiscard]] std::string getEngineName() const override { return "Python Demucs"; }
    [[nodiscard]] SeparationEngine getEngineType() const override { return SeparationEngine::PythonDemucs; }

    // Check if Python demucs is available in the system
    bool isDemucsAvailable();

private:
    ModelInfo currentModel;
    bool modelLoaded = false;
    std::string demucsModelName;  // e.g., "htdemucs" or "htdemucs_6s"
    juce::String demucsPath;      // Full path to demucs executable

    // Find demucs in common installation locations
    static juce::String findDemucsPath();

    // Helper to run subprocess and capture output
    bool runDemucsCommand(const juce::File& inputFile,
                          const juce::File& outputDir,
                          SeparationProgressCallback progressCallback);

    // Load audio file into buffer
    static juce::AudioBuffer<float> loadAudioFile(const juce::File& file, double& sampleRate);

    // Resample buffer to target sample rate
    static juce::AudioBuffer<float> resampleBuffer(
        const juce::AudioBuffer<float>& input,
        double sourceSampleRate,
        double targetSampleRate);
};

// Python Spleeter engine - calls terminal spleeter directly for 100% identical output
class PythonSpleeterEngine : public IStemSeparator
{
public:
    PythonSpleeterEngine();
    ~PythonSpleeterEngine() override;

    bool loadModel(const ModelInfo& model) override;
    void unloadModel() override;
    [[nodiscard]] bool isModelLoaded() const override;

    [[nodiscard]] std::vector<std::string> getAvailableStems() const override;

    [[nodiscard]] SeparationResult separate(
        const juce::AudioBuffer<float>& input,
        double sampleRate,
        SeparationProgressCallback progressCallback = nullptr,
        std::atomic<bool>* cancelFlag = nullptr) override;

    [[nodiscard]] std::string getEngineName() const override { return "Python Spleeter"; }
    [[nodiscard]] SeparationEngine getEngineType() const override { return SeparationEngine::PythonSpleeter; }

    // Check if Python spleeter is available in the system
    bool isSpleeterAvailable();

private:
    ModelInfo currentModel;
    bool modelLoaded = false;
    std::string spleeterModelName;  // e.g., "spleeter:2stems" or "spleeter:5stems"
    juce::String spleeterPath;      // Full path to spleeter executable

    // Find spleeter in common installation locations
    static juce::String findSpleeterPath();

    // Helper to run subprocess and capture output
    bool runSpleeterCommand(const juce::File& inputFile,
                            const juce::File& outputDir,
                            SeparationProgressCallback progressCallback);

    // Load audio file into buffer
    static juce::AudioBuffer<float> loadAudioFile(const juce::File& file, double& sampleRate);

    // Resample buffer to target sample rate
    static juce::AudioBuffer<float> resampleBuffer(
        const juce::AudioBuffer<float>& input,
        double sourceSampleRate,
        double targetSampleRate);
};

} // namespace spectralz

#endif // SPECTRALZ_ENABLE_ML
