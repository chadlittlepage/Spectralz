#include "SettingsPanel.h"

namespace spectralz
{

SettingsPanel::SettingsPanel()
{
    setSize(250, 400);

    setupSpectrogramSection();
    setupWaveformSection();
}

void SettingsPanel::setupSpectrogramSection()
{
    // Section header
    spectrogramLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    spectrogramLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(spectrogramLabel);

    // Color map selector
    colorMapLabel.setColour(juce::Label::textColourId, juce::Colour(0xaaffffff));
    addAndMakeVisible(colorMapLabel);

    colorMapSelector.addItem("Orange+Blue", 1);
    colorMapSelector.addItem("Orange", 2);
    colorMapSelector.addItem("Viridis", 3);
    colorMapSelector.addItem("Magma", 4);
    colorMapSelector.addItem("Inferno", 5);
    colorMapSelector.addItem("Plasma", 6);
    colorMapSelector.addItem("Grayscale", 7);
    colorMapSelector.addItem("Thermal", 8);
    colorMapSelector.addItem("Ocean", 9);
    colorMapSelector.addItem("Sunset", 10);
    colorMapSelector.setSelectedId(1, juce::dontSendNotification);
    colorMapSelector.onChange = [this]()
    {
        if (onSpectrogramColorMapChanged)
        {
            auto map = static_cast<SpectrogramColorMap>(colorMapSelector.getSelectedId() - 1);
            onSpectrogramColorMapChanged(map);
        }
    };
    addAndMakeVisible(colorMapSelector);

    // Brightness slider
    brightnessLabel.setColour(juce::Label::textColourId, juce::Colour(0xaaffffff));
    addAndMakeVisible(brightnessLabel);

    brightnessSlider.setRange(0.2, 3.0, 0.05);
    brightnessSlider.setValue(0.5, juce::dontSendNotification);
    brightnessSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    brightnessSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    brightnessSlider.onValueChange = [this]()
    {
        if (onSpectrogramBrightnessChanged)
            onSpectrogramBrightnessChanged(static_cast<float>(brightnessSlider.getValue()));
    };
    addAndMakeVisible(brightnessSlider);

    // Contrast slider
    contrastLabel.setColour(juce::Label::textColourId, juce::Colour(0xaaffffff));
    addAndMakeVisible(contrastLabel);

    contrastSlider.setRange(0.5, 3.0, 0.05);
    contrastSlider.setValue(1.2, juce::dontSendNotification);
    contrastSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    contrastSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    contrastSlider.onValueChange = [this]()
    {
        if (onSpectrogramContrastChanged)
            onSpectrogramContrastChanged(static_cast<float>(contrastSlider.getValue()));
    };
    addAndMakeVisible(contrastSlider);
}

void SettingsPanel::setupWaveformSection()
{
    // Section header
    waveformLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    waveformLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(waveformLabel);

    // Style selector
    styleLabel.setColour(juce::Label::textColourId, juce::Colour(0xaaffffff));
    addAndMakeVisible(styleLabel);

    styleSelector.addItem("Classic", 1);
    styleSelector.addItem("RMS + Peak", 2);
    styleSelector.addItem("Gradient", 3);
    styleSelector.addItem("Stereo", 4);
    styleSelector.addItem("Rainbow", 5);
    styleSelector.setSelectedId(2, juce::dontSendNotification);
    styleSelector.onChange = [this]()
    {
        if (onWaveformStyleChanged)
        {
            auto style = static_cast<WaveformStyle>(styleSelector.getSelectedId() - 1);
            onWaveformStyleChanged(style);
        }
    };
    addAndMakeVisible(styleSelector);

    // Color scheme selector
    colorSchemeLabel.setColour(juce::Label::textColourId, juce::Colour(0xaaffffff));
    addAndMakeVisible(colorSchemeLabel);

    colorSchemeSelector.addItem("Blue", 1);
    colorSchemeSelector.addItem("Orange", 2);
    colorSchemeSelector.addItem("Green", 3);
    colorSchemeSelector.addItem("Purple", 4);
    colorSchemeSelector.addItem("Cyan", 5);
    colorSchemeSelector.addItem("Fire", 6);
    colorSchemeSelector.addItem("Ice", 7);
    colorSchemeSelector.addItem("Neon", 8);
    colorSchemeSelector.setSelectedId(2, juce::dontSendNotification);
    colorSchemeSelector.onChange = [this]()
    {
        if (onWaveformColorSchemeChanged)
        {
            auto scheme = static_cast<WaveformColorScheme>(colorSchemeSelector.getSelectedId() - 1);
            onWaveformColorSchemeChanged(scheme);
        }
    };
    addAndMakeVisible(colorSchemeSelector);
}

void SettingsPanel::toggleVisibility()
{
    setVisible(!isVisible());
}

void SettingsPanel::paint(juce::Graphics& g)
{
    // Panel background
    g.fillAll(juce::Colour(0xee1a1a1a));

    // Border
    g.setColour(juce::Colour(0xff4a4a4a));
    g.drawRect(getLocalBounds(), 2);

    // Separator line between sections
    int separatorY = 180;
    g.setColour(juce::Colour(0x40ffffff));
    g.drawHorizontalLine(separatorY, 10.0f, static_cast<float>(getWidth() - 10));
}

void SettingsPanel::resized()
{
    auto bounds = getLocalBounds().reduced(15);
    int rowHeight = 28;
    int labelWidth = 90;
    int spacing = 8;

    // Spectrogram section
    spectrogramLabel.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(spacing);

    auto colorMapRow = bounds.removeFromTop(rowHeight);
    colorMapLabel.setBounds(colorMapRow.removeFromLeft(labelWidth));
    colorMapSelector.setBounds(colorMapRow);
    bounds.removeFromTop(spacing);

    auto brightnessRow = bounds.removeFromTop(rowHeight);
    brightnessLabel.setBounds(brightnessRow.removeFromLeft(labelWidth));
    brightnessSlider.setBounds(brightnessRow);
    bounds.removeFromTop(spacing);

    auto contrastRow = bounds.removeFromTop(rowHeight);
    contrastLabel.setBounds(contrastRow.removeFromLeft(labelWidth));
    contrastSlider.setBounds(contrastRow);
    bounds.removeFromTop(spacing * 3);

    // Waveform section
    waveformLabel.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(spacing);

    auto styleRow = bounds.removeFromTop(rowHeight);
    styleLabel.setBounds(styleRow.removeFromLeft(labelWidth));
    styleSelector.setBounds(styleRow);
    bounds.removeFromTop(spacing);

    auto schemeRow = bounds.removeFromTop(rowHeight);
    colorSchemeLabel.setBounds(schemeRow.removeFromLeft(labelWidth));
    colorSchemeSelector.setBounds(schemeRow);
}

} // namespace spectralz
