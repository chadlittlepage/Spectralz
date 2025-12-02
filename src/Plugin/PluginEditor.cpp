#include "PluginEditor.h"
#include "PluginProcessor.h"

SpectralzAudioProcessorEditor::SpectralzAudioProcessorEditor(SpectralzAudioProcessor& p)
    : AudioProcessorEditor(&p)
    , processorRef(p)
{
    // Primary color from CLAUDE.md: #4a556c
    setSize(1200, 800);
    setResizable(true, true);
    setResizeLimits(800, 600, 2560, 1440);
}

SpectralzAudioProcessorEditor::~SpectralzAudioProcessorEditor() = default;

void SpectralzAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Background with primary color
    g.fillAll(juce::Colour(0xff4a556c));

    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawFittedText("Spectralz", getLocalBounds(), juce::Justification::centred, 1);

    g.setFont(14.0f);
    g.drawFittedText("AI-Powered Spectral Audio Editor",
                     getLocalBounds().removeFromBottom(100),
                     juce::Justification::centred, 1);
}

void SpectralzAudioProcessorEditor::resized()
{
    // Layout components here
}
