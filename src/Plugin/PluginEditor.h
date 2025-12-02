#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class SpectralzAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SpectralzAudioProcessorEditor(SpectralzAudioProcessor&);
    ~SpectralzAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SpectralzAudioProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralzAudioProcessorEditor)
};
