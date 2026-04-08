#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SpectrogramView.h"
#include "WaveformView.h"
#include <functional>

namespace spectralz
{

class SettingsPanel : public juce::Component
{
public:
    SettingsPanel();
    ~SettingsPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Toggle visibility
    void toggleVisibility();
    [[nodiscard]] bool isPanelVisible() const { return isVisible(); }

    // Callbacks
    std::function<void(SpectrogramColorMap)> onSpectrogramColorMapChanged;
    std::function<void(float)> onSpectrogramBrightnessChanged;
    std::function<void(float)> onSpectrogramContrastChanged;
    std::function<void(WaveformStyle)> onWaveformStyleChanged;
    std::function<void(WaveformColorScheme)> onWaveformColorSchemeChanged;

private:
    void setupSpectrogramSection();
    void setupWaveformSection();

    // Section labels
    juce::Label spectrogramLabel{"spectrogramLabel", "Spectrogram"};
    juce::Label waveformLabel{"waveformLabel", "Waveform"};

    // Spectrogram controls
    juce::Label colorMapLabel{"colorMapLabel", "Color Map:"};
    juce::ComboBox colorMapSelector;
    juce::Label brightnessLabel{"brightnessLabel", "Brightness:"};
    juce::Slider brightnessSlider;
    juce::Label contrastLabel{"contrastLabel", "Contrast:"};
    juce::Slider contrastSlider;

    // Waveform controls
    juce::Label styleLabel{"styleLabel", "Style:"};
    juce::ComboBox styleSelector;
    juce::Label colorSchemeLabel{"colorSchemeLabel", "Color Scheme:"};
    juce::ComboBox colorSchemeSelector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};

} // namespace spectralz
