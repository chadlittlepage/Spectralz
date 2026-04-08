#pragma once

#if SPECTRALZ_ENABLE_ML

#include "ModelManager.h"
#include "StemSeparator.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <memory>
#include <atomic>
#include <functional>

namespace spectralz
{

// Status of separation job
enum class SeparationStatus
{
    Idle,
    Running,
    Completed,
    Failed,
    Cancelled
};

// Callbacks for separation progress and completion
using SeparationCompletedCallback = std::function<void(SeparationResult result)>;

class StemSeparationManager : private juce::Thread
{
public:
    StemSeparationManager();
    ~StemSeparationManager() override;

    // Non-copyable
    StemSeparationManager(const StemSeparationManager&) = delete;
    StemSeparationManager& operator=(const StemSeparationManager&) = delete;

    // Model management
    ModelManager& getModelManager() { return modelManager; }
    const ModelManager& getModelManager() const { return modelManager; }

    // Select engine and model
    void setEngine(SeparationEngine engine);
    [[nodiscard]] SeparationEngine getCurrentEngine() const { return currentEngine; }

    bool selectModel(const std::string& modelId);
    [[nodiscard]] std::optional<ModelInfo> getCurrentModel() const;
    [[nodiscard]] bool isModelLoaded() const;

    // Start separation (runs on background thread)
    bool startSeparation(
        const juce::AudioBuffer<float>& inputAudio,
        double sampleRate,
        SeparationProgressCallback progressCallback = nullptr,
        SeparationCompletedCallback completedCallback = nullptr);

    // Cancel current separation
    void cancelSeparation();

    // Query status
    [[nodiscard]] SeparationStatus getStatus() const { return status.load(); }
    [[nodiscard]] bool isRunning() const { return status.load() == SeparationStatus::Running; }
    [[nodiscard]] float getProgress() const { return progress.load(); }
    [[nodiscard]] std::string getStatusMessage() const;

    // Get last result (valid after Completed status)
    [[nodiscard]] const SeparationResult& getLastResult() const { return lastResult; }

    // Listener interface for status updates
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void separationStarted() {}
        virtual void separationProgress(float progressValue, const std::string& message) { juce::ignoreUnused(progressValue, message); }
        virtual void separationCompleted(const SeparationResult& result) { juce::ignoreUnused(result); }
        virtual void separationCancelled() {}
        virtual void separationFailed(const std::string& errorMsg) { juce::ignoreUnused(errorMsg); }
    };

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    // Thread implementation
    void run() override;

    // Model manager for discovering available models
    ModelManager modelManager;

    // Current engine and separator instance
    SeparationEngine currentEngine = SeparationEngine::Demucs;
    std::unique_ptr<IStemSeparator> separator;
    std::string currentModelId;

    // Job data
    juce::AudioBuffer<float> inputBuffer;
    double inputSampleRate = 44100.0;
    SeparationProgressCallback userProgressCallback;
    SeparationCompletedCallback userCompletedCallback;

    // Status
    std::atomic<SeparationStatus> status{SeparationStatus::Idle};
    std::atomic<float> progress{0.0f};
    std::atomic<bool> cancelRequested{false};
    juce::String statusMessage;
    mutable juce::CriticalSection statusLock;

    // Result
    SeparationResult lastResult;

    // Listeners
    juce::ListenerList<Listener> listeners;

    // Helper to notify listeners on message thread
    void notifyStarted();
    void notifyProgress(float p, const std::string& msg);
    void notifyCompleted(SeparationResult result);
    void notifyCancelled();
    void notifyFailed(const std::string& error);
};

} // namespace spectralz

#endif // SPECTRALZ_ENABLE_ML
