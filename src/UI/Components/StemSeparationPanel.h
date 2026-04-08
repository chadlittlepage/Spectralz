#pragma once

#if SPECTRALZ_ENABLE_ML

#include "../../ML/StemSeparationManager.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace spectralz
{

class StemSeparationPanel : public juce::Component,
                            public StemSeparationManager::Listener,
                            private juce::Timer
{
public:
    StemSeparationPanel();
    ~StemSeparationPanel() override;

    // Set the separation manager to use
    void setSeparationManager(StemSeparationManager* manager);

    // Callbacks
    std::function<void()> onSeparateClicked;
    std::function<void(const SeparationResult&)> onSeparationComplete;
    std::function<void(bool)> onCreateLayersToggled;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // StemSeparationManager::Listener overrides
    void separationStarted() override;
    void separationProgress(float progress, const std::string& message) override;
    void separationCompleted(const SeparationResult& result) override;
    void separationCancelled() override;
    void separationFailed(const std::string& error) override;

    // Timer for UI updates
    void timerCallback() override;

    // Get current settings
    [[nodiscard]] bool shouldCreateLayers() const { return createLayersToggle.getToggleState(); }
    [[nodiscard]] std::string getSelectedModelId() const;

private:
    void updateModelList();
    void updateEngineSelection();
    void updateUIState();

    StemSeparationManager* separationManager = nullptr;

    // UI Components
    juce::Label titleLabel;
    juce::Label engineLabel;
    juce::ComboBox engineSelector;
    juce::Label modelLabel;
    juce::ComboBox modelSelector;
    juce::TextButton separateButton;
    juce::TextButton cancelButton;
    juce::ProgressBar progressBar;
    juce::Label statusLabel;
    juce::ToggleButton createLayersToggle;
    juce::Label stemsLabel;

    // Progress value for the progress bar
    double progressValue = 0.0;

    // Color scheme (matching app theme)
    static constexpr juce::uint32 kPrimaryColor = 0xff4a556c;
    static constexpr juce::uint32 kAccentColor = 0xff4a9eff;
    static constexpr juce::uint32 kBackgroundColor = 0xff2d2d2d;
    static constexpr juce::uint32 kTextColor = 0xffe0e0e0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemSeparationPanel)
};

} // namespace spectralz

#endif // SPECTRALZ_ENABLE_ML
