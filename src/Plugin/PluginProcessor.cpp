#include "PluginProcessor.h"
#include "PluginEditor.h"

SpectralzAudioProcessor::SpectralzAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

SpectralzAudioProcessor::~SpectralzAudioProcessor() = default;

const juce::String SpectralzAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SpectralzAudioProcessor::acceptsMidi() const
{
    return false;
}

bool SpectralzAudioProcessor::producesMidi() const
{
    return false;
}

bool SpectralzAudioProcessor::isMidiEffect() const
{
    return false;
}

double SpectralzAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SpectralzAudioProcessor::getNumPrograms()
{
    return 1;
}

int SpectralzAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SpectralzAudioProcessor::setCurrentProgram(int /*index*/)
{
}

const juce::String SpectralzAudioProcessor::getProgramName(int /*index*/)
{
    return {};
}

void SpectralzAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/)
{
}

void SpectralzAudioProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
    // Initialize DSP here
}

void SpectralzAudioProcessor::releaseResources()
{
    // Release DSP resources
}

bool SpectralzAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void SpectralzAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Process audio here
}

bool SpectralzAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SpectralzAudioProcessor::createEditor()
{
    return new SpectralzAudioProcessorEditor(*this);
}

void SpectralzAudioProcessor::getStateInformation(juce::MemoryBlock& /*destData*/)
{
    // Save state
}

void SpectralzAudioProcessor::setStateInformation(const void* /*data*/, int /*sizeInBytes*/)
{
    // Load state
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectralzAudioProcessor();
}
