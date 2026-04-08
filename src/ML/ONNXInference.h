#pragma once

#if SPECTRALZ_ENABLE_ML

#include <onnxruntime_cxx_api.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <functional>

namespace spectralz
{

// Progress callback: (progress 0.0-1.0, status message)
using InferenceProgressCallback = std::function<void(float, const std::string&)>;

// Tensor shape descriptor
struct TensorShape
{
    std::vector<int64_t> dims;

    [[nodiscard]] size_t elementCount() const
    {
        if (dims.empty()) return 0;
        size_t count = 1;
        for (auto d : dims)
            count *= static_cast<size_t>(d);
        return count;
    }
};

// Result container for inference
struct InferenceResult
{
    std::vector<std::vector<float>> outputs;  // One vector per output tensor
    std::vector<TensorShape> outputShapes;
    bool success = false;
    std::string errorMessage;
};

class ONNXInference
{
public:
    ONNXInference();
    ~ONNXInference();

    // Non-copyable, movable
    ONNXInference(const ONNXInference&) = delete;
    ONNXInference& operator=(const ONNXInference&) = delete;
    ONNXInference(ONNXInference&&) noexcept;
    ONNXInference& operator=(ONNXInference&&) noexcept;

    // Session management
    [[nodiscard]] bool loadModel(const juce::File& modelPath);
    [[nodiscard]] bool isModelLoaded() const { return sessionLoaded; }
    void unloadModel();

    // Model introspection
    [[nodiscard]] std::vector<std::string> getInputNames() const;
    [[nodiscard]] std::vector<std::string> getOutputNames() const;
    [[nodiscard]] TensorShape getInputShape(size_t index) const;
    [[nodiscard]] TensorShape getOutputShape(size_t index) const;
    [[nodiscard]] size_t getNumInputs() const { return inputNames.size(); }
    [[nodiscard]] size_t getNumOutputs() const { return outputNames.size(); }

    // Inference execution - single input tensor (most common for audio)
    [[nodiscard]] InferenceResult run(
        const std::vector<float>& inputData,
        const TensorShape& inputShape);

    // Inference execution - multiple input tensors
    [[nodiscard]] InferenceResult run(
        const std::vector<std::vector<float>>& inputs,
        const std::vector<TensorShape>& inputShapes);

    // Memory info
    [[nodiscard]] size_t getApproximateMemoryUsage() const;

    // Get model path (for debugging)
    [[nodiscard]] const juce::File& getModelPath() const { return modelPath; }

private:
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    std::unique_ptr<Ort::SessionOptions> sessionOptions;
    Ort::AllocatorWithDefaultOptions allocator;

    std::vector<std::string> inputNames;
    std::vector<std::string> outputNames;
    std::vector<const char*> inputNamesCStr;
    std::vector<const char*> outputNamesCStr;
    std::vector<TensorShape> inputShapes;
    std::vector<TensorShape> outputShapes;

    juce::File modelPath;
    bool sessionLoaded = false;

    mutable std::mutex sessionMutex;

    void initializeEnvironment();
    void configureSessionOptions();
    void extractModelMetadata();
    void unloadModelInternal();  // Internal unload - assumes mutex is already held
};

} // namespace spectralz

#endif // SPECTRALZ_ENABLE_ML
