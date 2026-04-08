#if SPECTRALZ_ENABLE_ML

#include "StemSeparationPanel.h"

namespace spectralz
{

StemSeparationPanel::StemSeparationPanel()
    : progressBar(progressValue)
{
    // Title
    titleLabel.setText("Stem Separation", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(kTextColor));
    addAndMakeVisible(titleLabel);

    // Engine selector
    engineLabel.setText("Engine:", juce::dontSendNotification);
    engineLabel.setColour(juce::Label::textColourId, juce::Colour(kTextColor));
    addAndMakeVisible(engineLabel);

    engineSelector.addItem("Demucs (ONNX)", 1);
    engineSelector.addItem("Spleeter (ONNX)", 2);
    engineSelector.addItem("Python Demucs (Terminal)", 3);
    engineSelector.addItem("Python Spleeter (Terminal)", 4);
    engineSelector.setSelectedId(1);
    engineSelector.onChange = [this]() { updateEngineSelection(); };
    addAndMakeVisible(engineSelector);

    // Model selector
    modelLabel.setText("Model:", juce::dontSendNotification);
    modelLabel.setColour(juce::Label::textColourId, juce::Colour(kTextColor));
    addAndMakeVisible(modelLabel);

    modelSelector.onChange = [this]()
    {
        // Reset progress bar when model changes
        progressValue = 0.0;
        progressBar.repaint();

        if (separationManager && modelSelector.getSelectedId() > 0)
        {
            try
            {
                // Check if we're using Python Demucs engine (special handling - no ONNX models)
                if (separationManager->getCurrentEngine() == SeparationEngine::PythonDemucs)
                {
                    // For Python Demucs, map selection ID to model name directly
                    std::string pythonModelId;
                    int selectedId = modelSelector.getSelectedId();
                    switch (selectedId)
                    {
                        case 1: pythonModelId = "htdemucs"; break;
                        case 2: pythonModelId = "htdemucs_6s"; break;
                        case 3: pythonModelId = "htdemucs_ft"; break;
                        default: pythonModelId = "htdemucs"; break;
                    }

                    // Update stems label based on model
                    if (selectedId == 2)
                        stemsLabel.setText("Stems: drums, bass, other, vocals, guitar, piano (terminal)", juce::dontSendNotification);
                    else
                        stemsLabel.setText("Stems: drums, bass, other, vocals (terminal)", juce::dontSendNotification);

                    DBG("Attempting to select Python Demucs model: " << pythonModelId);
                    if (!separationManager->selectModel(pythonModelId))
                    {
                        statusLabel.setText("Failed to load model", juce::dontSendNotification);
                        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff6060));
                    }
                    else
                    {
                        statusLabel.setText("Model ready (terminal)", juce::dontSendNotification);
                        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa0a0a0));
                        updateUIState();  // Enable Separate button after model loads
                    }
                    return;
                }

                // Check if we're using Python Spleeter engine (special handling - no ONNX models)
                if (separationManager->getCurrentEngine() == SeparationEngine::PythonSpleeter)
                {
                    // For Python Spleeter, map selection ID to model preset directly
                    std::string pythonModelId;
                    int selectedId = modelSelector.getSelectedId();
                    switch (selectedId)
                    {
                        case 1: pythonModelId = "spleeter:2stems"; break;
                        case 2: pythonModelId = "spleeter:4stems"; break;
                        case 3: pythonModelId = "spleeter:5stems"; break;
                        default: pythonModelId = "spleeter:2stems"; break;
                    }

                    // Update stems label based on model
                    if (selectedId == 3)
                        stemsLabel.setText("Stems: vocals, drums, bass, piano, other (terminal)", juce::dontSendNotification);
                    else if (selectedId == 2)
                        stemsLabel.setText("Stems: vocals, drums, bass, other (terminal)", juce::dontSendNotification);
                    else
                        stemsLabel.setText("Stems: vocals, accompaniment (terminal)", juce::dontSendNotification);

                    DBG("Attempting to select Python Spleeter model: " << pythonModelId);
                    if (!separationManager->selectModel(pythonModelId))
                    {
                        statusLabel.setText("Failed to load model", juce::dontSendNotification);
                        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff6060));
                    }
                    else
                    {
                        statusLabel.setText("Model ready (terminal)", juce::dontSendNotification);
                        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa0a0a0));
                        updateUIState();  // Enable Separate button after model loads
                    }
                    return;
                }

                // For ONNX-based engines, look up in ModelManager
                auto modelId = modelSelector.getItemText(modelSelector.getSelectedItemIndex()).toStdString();
                // Extract ID from display text (format: "DisplayName (stems)")
                auto models = separationManager->getModelManager().getAvailableModels();
                for (const auto& model : models)
                {
                    if (modelSelector.getText().startsWith(model.displayName))
                    {
                        // Always update stems label when model is found in manifest
                        juce::String stemsText = "Stems: ";
                        for (size_t i = 0; i < model.stems.size(); ++i)
                        {
                            if (i > 0) stemsText += ", ";
                            stemsText += juce::String(model.stems[i]);
                        }
                        stemsLabel.setText(stemsText, juce::dontSendNotification);

                        DBG("Attempting to select model: " << model.id);
                        if (!separationManager->selectModel(model.id))
                        {
                            statusLabel.setText("Failed to load model", juce::dontSendNotification);
                            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff6060));
                        }
                        else
                        {
                            statusLabel.setText("Model loaded", juce::dontSendNotification);
                            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa0a0a0));
                            updateUIState();  // Enable Separate button after model loads
                        }
                        break;
                    }
                }
            }
            catch (const std::exception& e)
            {
                DBG("Exception selecting model: " << e.what());
                statusLabel.setText(juce::String("Error: ") + e.what(), juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff6060));
            }
            catch (...)
            {
                DBG("Unknown exception selecting model");
                statusLabel.setText("Unknown error loading model", juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff6060));
            }
        }
    };
    addAndMakeVisible(modelSelector);

    // Stems info label
    stemsLabel.setText("", juce::dontSendNotification);
    stemsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa0a0a0));
    stemsLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(stemsLabel);

    // Create layers toggle
    createLayersToggle.setButtonText("Create layers from stems");
    createLayersToggle.setToggleState(true, juce::dontSendNotification);
    createLayersToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(kTextColor));
    createLayersToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(kAccentColor));
    createLayersToggle.onClick = [this]()
    {
        if (onCreateLayersToggled)
            onCreateLayersToggled(createLayersToggle.getToggleState());
    };
    addAndMakeVisible(createLayersToggle);

    // Separate button
    separateButton.setButtonText("Separate");
    separateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(kAccentColor));
    separateButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    separateButton.onClick = [this]()
    {
        if (onSeparateClicked)
            onSeparateClicked();
    };
    addAndMakeVisible(separateButton);

    // Cancel button
    cancelButton.setButtonText("Cancel");
    cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff804040));
    cancelButton.onClick = [this]()
    {
        if (separationManager)
            separationManager->cancelSeparation();
    };
    cancelButton.setEnabled(false);
    addAndMakeVisible(cancelButton);

    // Progress bar
    progressBar.setColour(juce::ProgressBar::foregroundColourId, juce::Colour(kAccentColor));
    progressBar.setColour(juce::ProgressBar::backgroundColourId, juce::Colour(0xff404040));
    addAndMakeVisible(progressBar);

    // Status label
    statusLabel.setText("Ready", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa0a0a0));
    statusLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(statusLabel);
}

StemSeparationPanel::~StemSeparationPanel()
{
    stopTimer();

    if (separationManager)
        separationManager->removeListener(this);
}

void StemSeparationPanel::setSeparationManager(StemSeparationManager* manager)
{
    if (separationManager)
        separationManager->removeListener(this);

    separationManager = manager;

    if (separationManager)
    {
        separationManager->addListener(this);
        updateModelList();
        updateUIState();
    }
}

void StemSeparationPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(kBackgroundColor));

    // Border
    g.setColour(juce::Colour(0xff404040));
    g.drawRect(getLocalBounds(), 1);

    // Header background
    auto headerBounds = getLocalBounds().removeFromTop(30);
    g.setColour(juce::Colour(kPrimaryColor).withAlpha(0.3f));
    g.fillRect(headerBounds);
}

void StemSeparationPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Title
    titleLabel.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(10);

    // Engine row
    auto engineRow = bounds.removeFromTop(24);
    engineLabel.setBounds(engineRow.removeFromLeft(60));
    engineSelector.setBounds(engineRow.reduced(2, 0));
    bounds.removeFromTop(8);

    // Model row
    auto modelRow = bounds.removeFromTop(24);
    modelLabel.setBounds(modelRow.removeFromLeft(60));
    modelSelector.setBounds(modelRow.reduced(2, 0));
    bounds.removeFromTop(4);

    // Stems info
    stemsLabel.setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(8);

    // Create layers toggle
    createLayersToggle.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(12);

    // Buttons row
    auto buttonRow = bounds.removeFromTop(30);
    int buttonWidth = (buttonRow.getWidth() - 10) / 2;
    separateButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(10);
    cancelButton.setBounds(buttonRow);
    bounds.removeFromTop(10);

    // Progress bar
    progressBar.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(6);

    // Status
    statusLabel.setBounds(bounds.removeFromTop(18));
}

void StemSeparationPanel::updateModelList()
{
    modelSelector.clear();

    if (!separationManager)
    {
        // Add placeholder items if no manager
        modelSelector.addItem("No models available", 1);
        return;
    }

    auto currentEngine = separationManager->getCurrentEngine();
    auto models = separationManager->getModelManager().getModelsForEngine(currentEngine);

    // If no models from manager, get all available and filter
    if (models.empty())
    {
        models = separationManager->getModelManager().getAvailableModels();
        // Filter by current engine
        models.erase(std::remove_if(models.begin(), models.end(),
            [currentEngine](const ModelInfo& m) { return m.engine != currentEngine; }),
            models.end());
    }

    int id = 1;
    for (const auto& model : models)
    {
        juce::String displayText = model.displayName;
        if (!model.isValid)
            displayText += " (not installed)";

        modelSelector.addItem(displayText, id++);
    }

    // If still empty, add placeholder based on engine
    if (modelSelector.getNumItems() == 0)
    {
        if (currentEngine == SeparationEngine::Demucs)
        {
            modelSelector.addItem("Demucs HT (4 stems) - not installed", 1);
            modelSelector.addItem("Demucs HT 6-stem - not installed", 2);
        }
        else if (currentEngine == SeparationEngine::PythonDemucs)
        {
            // Python Demucs doesn't need ONNX models - it uses the terminal command
            modelSelector.addItem("htdemucs (4 stems)", 1);
            modelSelector.addItem("htdemucs_6s (6 stems)", 2);
            modelSelector.addItem("htdemucs_ft (4 stems, fine-tuned)", 3);
        }
        else if (currentEngine == SeparationEngine::PythonSpleeter)
        {
            // Python Spleeter doesn't need ONNX models - it uses the terminal command
            modelSelector.addItem("spleeter:2stems (vocals, accompaniment)", 1);
            modelSelector.addItem("spleeter:4stems (vocals, drums, bass, other)", 2);
            modelSelector.addItem("spleeter:5stems (vocals, drums, bass, piano, other)", 3);
        }
        else
        {
            modelSelector.addItem("Spleeter (2 stems) - not installed", 1);
            modelSelector.addItem("Spleeter (5 stems) - not installed", 2);
        }
    }

    if (modelSelector.getNumItems() > 0)
    {
        // Use sendNotificationAsync to trigger onChange callback for model selection
        modelSelector.setSelectedItemIndex(0, juce::sendNotificationAsync);

        // Update stems label
        if (!models.empty())
        {
            juce::String stemsText = "Stems: ";
            for (size_t i = 0; i < models[0].stems.size(); ++i)
            {
                if (i > 0) stemsText += ", ";
                stemsText += juce::String(models[0].stems[i]);
            }
            stemsLabel.setText(stemsText, juce::dontSendNotification);
        }
        else
        {
            // Fallback stems based on engine
            if (currentEngine == SeparationEngine::Demucs)
                stemsLabel.setText("Stems: drums, bass, other, vocals", juce::dontSendNotification);
            else if (currentEngine == SeparationEngine::PythonDemucs)
                stemsLabel.setText("Stems: drums, bass, other, vocals (terminal)", juce::dontSendNotification);
            else if (currentEngine == SeparationEngine::PythonSpleeter)
                stemsLabel.setText("Stems: vocals, accompaniment (terminal)", juce::dontSendNotification);
            else
                stemsLabel.setText("Stems: vocals, accompaniment", juce::dontSendNotification);
        }
    }
}

void StemSeparationPanel::updateEngineSelection()
{
    if (!separationManager)
        return;

    // Reset progress bar when engine changes
    progressValue = 0.0;
    progressBar.repaint();
    statusLabel.setText("Ready", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa0a0a0));

    int selectedId = engineSelector.getSelectedId();
    SeparationEngine engine;
    switch (selectedId)
    {
        case 2:  engine = SeparationEngine::Spleeter; break;
        case 3:  engine = SeparationEngine::PythonDemucs; break;
        case 4:  engine = SeparationEngine::PythonSpleeter; break;
        default: engine = SeparationEngine::Demucs; break;
    }

    separationManager->setEngine(engine);
    updateModelList();
}

void StemSeparationPanel::updateUIState()
{
    if (!separationManager)
        return;

    bool isRunning = separationManager->isRunning();

    engineSelector.setEnabled(!isRunning);
    modelSelector.setEnabled(!isRunning);
    separateButton.setEnabled(!isRunning && separationManager->isModelLoaded());
    cancelButton.setEnabled(isRunning);
    createLayersToggle.setEnabled(!isRunning);
}

std::string StemSeparationPanel::getSelectedModelId() const
{
    if (!separationManager)
        return "";

    auto currentModel = separationManager->getCurrentModel();
    return currentModel.has_value() ? currentModel->id : "";
}

void StemSeparationPanel::separationStarted()
{
    progressValue = 0.0;
    statusLabel.setText("Starting...", juce::dontSendNotification);
    updateUIState();
    startTimerHz(30);  // Update progress at 30fps
}

void StemSeparationPanel::separationProgress(float progress, const std::string& message)
{
    progressValue = progress;
    statusLabel.setText(message, juce::dontSendNotification);
}

void StemSeparationPanel::separationCompleted(const SeparationResult& result)
{
    stopTimer();
    progressValue = 1.0;
    statusLabel.setText("Complete - " + std::to_string(result.getStemCount()) + " stems extracted",
                       juce::dontSendNotification);
    updateUIState();

    if (onSeparationComplete)
        onSeparationComplete(result);
}

void StemSeparationPanel::separationCancelled()
{
    stopTimer();
    progressValue = 0.0;
    statusLabel.setText("Cancelled", juce::dontSendNotification);
    updateUIState();
}

void StemSeparationPanel::separationFailed(const std::string& error)
{
    stopTimer();
    progressValue = 0.0;
    statusLabel.setText("Error: " + error, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff6060));
    updateUIState();

    // Reset color after a delay
    juce::Timer::callAfterDelay(3000, [this]()
    {
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa0a0a0));
    });
}

void StemSeparationPanel::timerCallback()
{
    // Force progress bar repaint
    progressBar.repaint();
}

} // namespace spectralz

#endif // SPECTRALZ_ENABLE_ML
