#pragma once

#if SPECTRALZ_ENABLE_ML

#include <juce_core/juce_core.h>
#include <vector>
#include <string>
#include <optional>
#include <functional>

namespace spectralz
{

// Supported stem separation engines
enum class SeparationEngine
{
    Demucs,
    Spleeter,
    PythonDemucs,  // Calls terminal Python demucs directly (guaranteed identical output)
    PythonSpleeter // Calls terminal Python spleeter directly (guaranteed identical output)
};

// Model metadata
struct ModelInfo
{
    std::string id;                      // Unique identifier (e.g., "htdemucs_6s")
    std::string displayName;             // Human-readable name
    SeparationEngine engine;             // Which engine this model uses
    std::vector<std::string> stems;      // Output stem names
    juce::File modelFile;                // Path to .onnx file (for single-file models)
    juce::File modelDir;                 // Path to model directory (for multi-file models like Spleeter)
    int sampleRate = 44100;              // Expected sample rate
    int64_t fileSizeBytes = 0;           // Model file size
    bool isValid = false;                // Whether model file exists and is readable
    bool usesMultipleFiles = false;      // True for Spleeter (one .onnx per stem)

    [[nodiscard]] size_t getStemCount() const { return stems.size(); }
    [[nodiscard]] std::string getEngineString() const
    {
        switch (engine)
        {
            case SeparationEngine::Demucs:  return "Demucs";
            case SeparationEngine::Spleeter: return "Spleeter";
            case SeparationEngine::PythonDemucs: return "Python Demucs";
            case SeparationEngine::PythonSpleeter: return "Python Spleeter";
            default: return "Unknown";
        }
    }

    // Get the ONNX file for a specific stem (for multi-file models)
    [[nodiscard]] juce::File getStemModelFile(const std::string& stemName) const
    {
        if (usesMultipleFiles && modelDir.isDirectory())
            return modelDir.getChildFile(juce::String(stemName) + ".onnx");
        return modelFile;
    }
};

// Progress callback for model downloads
using ModelDownloadProgress = std::function<void(float progress, const std::string& status)>;

class ModelManager
{
public:
    ModelManager();
    ~ModelManager() = default;

    // Non-copyable
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

    // Directory management
    void setModelsDirectory(const juce::File& directory);
    [[nodiscard]] juce::File getModelsDirectory() const { return modelsDirectory; }

    // Scanning
    void scanForModels();
    [[nodiscard]] bool needsRescan() const { return modelsDirty; }

    // Model access
    [[nodiscard]] std::vector<ModelInfo> getAvailableModels() const;
    [[nodiscard]] std::vector<ModelInfo> getModelsForEngine(SeparationEngine engine) const;
    [[nodiscard]] std::optional<ModelInfo> getModelById(const std::string& id) const;
    [[nodiscard]] std::optional<ModelInfo> getDefaultModel() const;

    // Model validation
    [[nodiscard]] bool isModelValid(const std::string& id) const;
    [[nodiscard]] bool hasAnyModels() const { return !models.empty(); }

    // Listeners for model changes
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void modelsChanged() = 0;
    };

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    juce::File modelsDirectory;
    std::vector<ModelInfo> models;
    juce::ListenerList<Listener> listeners;
    bool modelsDirty = true;

    void scanEngineDirectory(const juce::File& dir, SeparationEngine engine);
    bool parseManifest(const juce::File& manifestFile, SeparationEngine engine);
    void notifyListeners();

    // Built-in model definitions (fallback if no manifest)
    static std::vector<ModelInfo> getBuiltInModelDefinitions();
};

} // namespace spectralz

#endif // SPECTRALZ_ENABLE_ML
