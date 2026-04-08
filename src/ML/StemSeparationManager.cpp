#if SPECTRALZ_ENABLE_ML

#include "StemSeparationManager.h"

namespace spectralz
{

StemSeparationManager::StemSeparationManager()
    : juce::Thread("StemSeparation")
{
    // Scan for available models
    modelManager.scanForModels();

    // Create default engine
    separator = createStemSeparator(currentEngine);
}

StemSeparationManager::~StemSeparationManager()
{
    cancelSeparation();

    // Wait for thread to finish
    stopThread(5000);
}

void StemSeparationManager::setEngine(SeparationEngine engine)
{
    if (isRunning())
    {
        DBG("Cannot change engine while separation is running");
        return;
    }

    if (currentEngine != engine)
    {
        currentEngine = engine;
        separator = createStemSeparator(engine);
        currentModelId.clear();
    }
}

bool StemSeparationManager::selectModel(const std::string& modelId)
{
    DBG("StemSeparationManager::selectModel called with: " << modelId);

    if (isRunning())
    {
        DBG("Cannot change model while separation is running");
        return false;
    }

    // Special handling for Python Demucs - don't look up in ModelManager
    // because ModelManager only contains ONNX models which would cause engine switch
    if (currentEngine == SeparationEngine::PythonDemucs)
    {
        DBG("PythonDemucs engine active - creating synthetic model info for: " << modelId);

        // Create a synthetic ModelInfo for Python Demucs
        ModelInfo pythonModel;
        pythonModel.id = modelId;
        pythonModel.engine = SeparationEngine::PythonDemucs;
        pythonModel.isValid = true;  // Python models are always "valid" (demucs downloads them)

        // Set stems based on model name
        if (modelId == "htdemucs_6s")
        {
            pythonModel.displayName = "htdemucs_6s (6 stems)";
            pythonModel.stems = {"drums", "bass", "other", "vocals", "guitar", "piano"};
        }
        else if (modelId == "htdemucs_ft")
        {
            pythonModel.displayName = "htdemucs_ft (4 stems, fine-tuned)";
            pythonModel.stems = {"drums", "bass", "other", "vocals"};
        }
        else  // Default to htdemucs
        {
            pythonModel.displayName = "htdemucs (4 stems)";
            pythonModel.stems = {"drums", "bass", "other", "vocals"};
        }

        if (!separator)
        {
            DBG("No separator available for PythonDemucs");
            return false;
        }

        DBG("Calling PythonDemucsEngine->loadModel for: " << pythonModel.displayName);
        try
        {
            if (separator->loadModel(pythonModel))
            {
                currentModelId = modelId;
                DBG("PythonDemucs model ready: " << pythonModel.displayName);
                return true;
            }
            else
            {
                DBG("PythonDemucsEngine->loadModel returned false");
                return false;
            }
        }
        catch (const std::exception& e)
        {
            DBG("Exception in PythonDemucsEngine->loadModel: " << e.what());
            return false;
        }
        catch (...)
        {
            DBG("Unknown exception in PythonDemucsEngine->loadModel");
            return false;
        }
    }

    // Special handling for Python Spleeter - don't look up in ModelManager
    // because ModelManager only contains ONNX models which would cause engine switch
    if (currentEngine == SeparationEngine::PythonSpleeter)
    {
        DBG("PythonSpleeter engine active - creating synthetic model info for: " << modelId);

        // Create a synthetic ModelInfo for Python Spleeter
        ModelInfo pythonModel;
        pythonModel.id = modelId;
        pythonModel.engine = SeparationEngine::PythonSpleeter;
        pythonModel.isValid = true;  // Python models are always "valid" (spleeter downloads them)

        // Set stems based on model name
        if (modelId == "spleeter:5stems")
        {
            pythonModel.displayName = "spleeter:5stems";
            pythonModel.stems = {"vocals", "drums", "bass", "piano", "other"};
        }
        else if (modelId == "spleeter:4stems")
        {
            pythonModel.displayName = "spleeter:4stems";
            pythonModel.stems = {"vocals", "drums", "bass", "other"};
        }
        else  // Default to spleeter:2stems
        {
            pythonModel.displayName = "spleeter:2stems";
            pythonModel.stems = {"vocals", "accompaniment"};
        }

        if (!separator)
        {
            DBG("No separator available for PythonSpleeter");
            return false;
        }

        DBG("Calling PythonSpleeterEngine->loadModel for: " << pythonModel.displayName);
        try
        {
            if (separator->loadModel(pythonModel))
            {
                currentModelId = modelId;
                DBG("PythonSpleeter model ready: " << pythonModel.displayName);
                return true;
            }
            else
            {
                DBG("PythonSpleeterEngine->loadModel returned false");
                return false;
            }
        }
        catch (const std::exception& e)
        {
            DBG("Exception in PythonSpleeterEngine->loadModel: " << e.what());
            return false;
        }
        catch (...)
        {
            DBG("Unknown exception in PythonSpleeterEngine->loadModel");
            return false;
        }
    }

    // For ONNX-based engines (Demucs, Spleeter), use ModelManager
    auto model = modelManager.getModelById(modelId);
    if (!model.has_value())
    {
        DBG("Model not found: " << modelId);
        return false;
    }

    DBG("Model found: " << model->displayName << ", file: " << model->modelFile.getFullPathName());

    // Switch engine if needed (only for ONNX engines)
    if (model->engine != currentEngine)
    {
        DBG("Switching engine...");
        setEngine(model->engine);
    }

    if (!separator)
    {
        DBG("Failed to create separator for engine");
        return false;
    }

    if (!model->isValid)
    {
        DBG("Model file not found: " << model->modelFile.getFullPathName());
        return false;
    }

    DBG("Calling separator->loadModel...");
    try
    {
        if (separator->loadModel(*model))
        {
            currentModelId = modelId;
            DBG("Loaded model: " << model->displayName);
            return true;
        }
        else
        {
            DBG("separator->loadModel returned false");
            return false;
        }
    }
    catch (const std::exception& e)
    {
        DBG("Exception in separator->loadModel: " << e.what());
        return false;
    }
    catch (...)
    {
        DBG("Unknown exception in separator->loadModel");
        return false;
    }
}

std::optional<ModelInfo> StemSeparationManager::getCurrentModel() const
{
    return modelManager.getModelById(currentModelId);
}

bool StemSeparationManager::isModelLoaded() const
{
    return separator && separator->isModelLoaded();
}

bool StemSeparationManager::startSeparation(
    const juce::AudioBuffer<float>& inputAudio,
    double sampleRate,
    SeparationProgressCallback progressCallback,
    SeparationCompletedCallback completedCallback)
{
    if (isRunning())
    {
        DBG("Separation already in progress");
        return false;
    }

    if (!isModelLoaded())
    {
        DBG("No model loaded");
        return false;
    }

    if (inputAudio.getNumSamples() == 0)
    {
        DBG("Input buffer is empty");
        return false;
    }

    // Copy input data
    inputBuffer.makeCopyOf(inputAudio);
    inputSampleRate = sampleRate;
    userProgressCallback = progressCallback;
    userCompletedCallback = completedCallback;

    // Reset state
    cancelRequested.store(false);
    progress.store(0.0f);
    status.store(SeparationStatus::Running);
    lastResult = SeparationResult();

    {
        juce::ScopedLock lock(statusLock);
        statusMessage = "Starting separation...";
    }

    // Start background thread
    startThread(juce::Thread::Priority::normal);

    notifyStarted();

    return true;
}

void StemSeparationManager::cancelSeparation()
{
    if (isRunning())
    {
        cancelRequested.store(true);

        {
            juce::ScopedLock lock(statusLock);
            statusMessage = "Cancelling...";
        }
    }
}

std::string StemSeparationManager::getStatusMessage() const
{
    juce::ScopedLock lock(statusLock);
    return statusMessage.toStdString();
}

void StemSeparationManager::addListener(Listener* listener)
{
    listeners.add(listener);
}

void StemSeparationManager::removeListener(Listener* listener)
{
    listeners.remove(listener);
}

void StemSeparationManager::run()
{
    DBG("Separation thread started");

    auto progressCb = [this](float p, const std::string& msg)
    {
        progress.store(p);

        {
            juce::ScopedLock lock(statusLock);
            statusMessage = msg;
        }

        // Notify on message thread
        notifyProgress(p, msg);

        // Also call user callback if provided
        if (userProgressCallback)
        {
            juce::MessageManager::callAsync([this, p, msg]()
            {
                if (userProgressCallback)
                    userProgressCallback(p, msg);
            });
        }
    };

    // Run separation
    auto result = separator->separate(
        inputBuffer,
        inputSampleRate,
        progressCb,
        &cancelRequested);

    // Store result
    lastResult = std::move(result);

    // Update status
    if (cancelRequested.load())
    {
        status.store(SeparationStatus::Cancelled);

        {
            juce::ScopedLock lock(statusLock);
            statusMessage = "Cancelled";
        }

        notifyCancelled();
    }
    else if (lastResult.success)
    {
        status.store(SeparationStatus::Completed);

        {
            juce::ScopedLock lock(statusLock);
            statusMessage = "Complete";
        }

        progress.store(1.0f);
        notifyCompleted(lastResult);

        // Call user callback
        if (userCompletedCallback)
        {
            auto resultCopy = lastResult;
            juce::MessageManager::callAsync([this, resultCopy]()
            {
                if (userCompletedCallback)
                    userCompletedCallback(resultCopy);
            });
        }
    }
    else
    {
        status.store(SeparationStatus::Failed);

        {
            juce::ScopedLock lock(statusLock);
            statusMessage = lastResult.errorMessage;
        }

        notifyFailed(lastResult.errorMessage);
    }

    DBG("Separation thread finished");
}

void StemSeparationManager::notifyStarted()
{
    juce::MessageManager::callAsync([this]()
    {
        listeners.call([](Listener& l) { l.separationStarted(); });
    });
}

void StemSeparationManager::notifyProgress(float p, const std::string& msg)
{
    juce::MessageManager::callAsync([this, p, msg]()
    {
        listeners.call([p, msg](Listener& l) { l.separationProgress(p, msg); });
    });
}

void StemSeparationManager::notifyCompleted(SeparationResult result)
{
    juce::MessageManager::callAsync([this, result = std::move(result)]()
    {
        listeners.call([&result](Listener& l) { l.separationCompleted(result); });
    });
}

void StemSeparationManager::notifyCancelled()
{
    juce::MessageManager::callAsync([this]()
    {
        listeners.call([](Listener& l) { l.separationCancelled(); });
    });
}

void StemSeparationManager::notifyFailed(const std::string& error)
{
    juce::MessageManager::callAsync([this, error]()
    {
        listeners.call([&error](Listener& l) { l.separationFailed(error); });
    });
}

} // namespace spectralz

#endif // SPECTRALZ_ENABLE_ML
