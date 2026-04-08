#include "AudioEngine.h"

namespace spectralz
{

AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();
    thumbnail = std::make_unique<juce::AudioThumbnail>(512, formatManager, thumbnailCache);
    transportSource.addChangeListener(this);
}

AudioEngine::~AudioEngine()
{
    transportSource.removeChangeListener(this);
    transportSource.setSource(nullptr);
    audioSourcePlayer.setSource(nullptr);
    deviceManager.removeAudioCallback(&audioSourcePlayer);
}

void AudioEngine::setupAudioDevice()
{
    auto result = deviceManager.initialiseWithDefaultDevices(0, 2);
    if (result.isNotEmpty())
    {
        DBG("Audio device initialization failed: " + result);
    }

    deviceManager.addAudioCallback(&audioSourcePlayer);
    audioSourcePlayer.setSource(this);
}

bool AudioEngine::loadFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    auto* reader = formatManager.createReaderFor(file);
    if (reader == nullptr)
        return false;

    // Store metadata
    loadedSampleRate = reader->sampleRate;
    loadedNumChannels = static_cast<int>(reader->numChannels);

    // Load entire file into buffer for visualization
    audioBuffer.setSize(loadedNumChannels, static_cast<int>(reader->lengthInSamples));
    reader->read(&audioBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    // Setup thumbnail
    thumbnail->setSource(new juce::FileInputSource(file));

    // Create reader source for playback
    auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);

    transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate, static_cast<int>(reader->numChannels));
    readerSource = std::move(newSource);

    if (onFileLoaded)
        onFileLoaded();

    return true;
}

bool AudioEngine::saveFile(const juce::File& file, juce::AudioFormat* format)
{
    if (!hasLoadedFile())
        return false;

    juce::AudioFormat* formatToUse = format;
    std::unique_ptr<juce::AudioFormat> ownedFormat;

    if (formatToUse == nullptr)
    {
        // Determine format from extension
        auto extension = file.getFileExtension().toLowerCase();
        if (extension == ".wav")
            ownedFormat = std::make_unique<juce::WavAudioFormat>();
        else if (extension == ".aiff" || extension == ".aif")
            ownedFormat = std::make_unique<juce::AiffAudioFormat>();
        else if (extension == ".flac")
            ownedFormat = std::make_unique<juce::FlacAudioFormat>();
        else
            return false;

        formatToUse = ownedFormat.get();
    }

    file.deleteFile();
    auto outputStream = file.createOutputStream();
    if (outputStream == nullptr)
        return false;

    auto* writer = formatToUse->createWriterFor(
        outputStream.release(),
        loadedSampleRate,
        static_cast<unsigned int>(loadedNumChannels),
        24,
        {},
        0);

    if (writer == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writerPtr(writer);
    return writerPtr->writeFromAudioSampleBuffer(audioBuffer, 0, audioBuffer.getNumSamples());
}

void AudioEngine::unloadFile()
{
    transportSource.stop();
    transportSource.setSource(nullptr);
    readerSource.reset();
    audioBuffer.setSize(0, 0);
    thumbnail->clear();
    loadedSampleRate = 0.0;
    loadedNumChannels = 0;
}

void AudioEngine::play()
{
    if (hasLoadedFile())
    {
        transportSource.start();
        if (onPlaybackStarted)
            onPlaybackStarted();
    }
}

void AudioEngine::pause()
{
    transportSource.stop();
}

void AudioEngine::stop()
{
    transportSource.stop();
    transportSource.setPosition(startPosition);
    if (onPlaybackStopped)
        onPlaybackStopped();
    if (onPositionChanged)
        onPositionChanged(startPosition);
}

void AudioEngine::setPosition(double positionInSeconds)
{
    transportSource.setPosition(positionInSeconds);
    if (onPositionChanged)
        onPositionChanged(positionInSeconds);
}

void AudioEngine::setStartPosition(double positionInSeconds)
{
    startPosition = positionInSeconds;
}

void AudioEngine::setLooping(bool shouldLoop)
{
    looping = shouldLoop;
    // Note: We handle looping manually in getNextAudioBlock for region support
    // Don't set readerSource looping - we control it ourselves
}

void AudioEngine::setLoopRegion(double startTime, double endTime)
{
    loopStartTime = startTime;
    loopEndTime = endTime;
    loopRegionSet = true;
}

void AudioEngine::clearLoopRegion()
{
    loopRegionSet = false;
    loopStartTime = 0.0;
    loopEndTime = 0.0;
}

bool AudioEngine::isPlaying() const
{
    return transportSource.isPlaying();
}

bool AudioEngine::isLooping() const
{
    return looping;
}

bool AudioEngine::hasLoadedFile() const
{
    return readerSource != nullptr;
}

double AudioEngine::getCurrentPosition() const
{
    return transportSource.getCurrentPosition();
}

double AudioEngine::getTotalLength() const
{
    return transportSource.getLengthInSeconds();
}

double AudioEngine::getSampleRate() const
{
    return loadedSampleRate;
}

int AudioEngine::getNumChannels() const
{
    return loadedNumChannels;
}

juce::int64 AudioEngine::getTotalSamples() const
{
    return audioBuffer.getNumSamples();
}

const juce::AudioBuffer<float>* AudioEngine::getAudioBuffer() const
{
    return hasLoadedFile() ? &audioBuffer : nullptr;
}

juce::AudioThumbnail* AudioEngine::getThumbnail()
{
    return thumbnail.get();
}

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
    editedTransportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void AudioEngine::releaseResources()
{
    transportSource.releaseResources();
    editedTransportSource.releaseResources();
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (!hasLoadedFile())
    {
        bufferToFill.clearActiveBufferRegion();
        leftLevel.store(0.0f);
        rightLevel.store(0.0f);
        return;
    }

    // Use edited audio source if available and selected
    if (playEditedAudio && editedBufferLoaded)
    {
        editedTransportSource.getNextAudioBlock(bufferToFill);
    }
    else
    {
        transportSource.getNextAudioBlock(bufferToFill);
    }

    // Calculate peak levels for metering
    auto* buffer = bufferToFill.buffer;
    int numSamples = bufferToFill.numSamples;
    int startSample = bufferToFill.startSample;

    if (buffer->getNumChannels() >= 1)
    {
        float peakL = buffer->getMagnitude(0, startSample, numSamples);
        leftLevel.store(peakL);

        if (buffer->getNumChannels() >= 2)
        {
            float peakR = buffer->getMagnitude(1, startSample, numSamples);
            rightLevel.store(peakR);
        }
        else
        {
            rightLevel.store(peakL); // Mono: same level for both
        }
    }

    // Handle looping
    if (looping)
    {
        double currentPos = transportSource.getCurrentPosition();

        if (loopRegionSet)
        {
            // Loop within selection region
            if (currentPos >= loopEndTime)
            {
                transportSource.setPosition(loopStartTime);
            }
        }
        else
        {
            // Loop entire file
            if (!transportSource.isPlaying() && transportSource.hasStreamFinished())
            {
                transportSource.setPosition(0.0);
                transportSource.start();
            }
        }
    }
}

void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &transportSource)
    {
        if (!transportSource.isPlaying() && onPlaybackStopped)
            onPlaybackStopped();
    }
}

void AudioEngine::setEditedBuffer(const juce::AudioBuffer<float>& buffer)
{
    // Copy the edited buffer
    editedBuffer.makeCopyOf(buffer);
    editedBufferLoaded = true;

    // Create a memory audio source from the edited buffer
    editedAudioSource = std::make_unique<juce::MemoryAudioSource>(editedBuffer, false, false);

    // Setup edited transport source
    editedTransportSource.setSource(editedAudioSource.get(), 0, nullptr, loadedSampleRate, loadedNumChannels);
}

void AudioEngine::clearEditedBuffer()
{
    editedTransportSource.setSource(nullptr);
    editedAudioSource.reset();
    editedBuffer.setSize(0, 0);
    editedBufferLoaded = false;
    playEditedAudio = false;
}

void AudioEngine::setPlayEditedAudio(bool playEdited)
{
    if (playEdited && !editedBufferLoaded)
        return;

    // Sync position before switching
    double currentPos = transportSource.getCurrentPosition();

    playEditedAudio = playEdited;

    if (playEdited)
    {
        editedTransportSource.setPosition(currentPos);
    }
    else
    {
        transportSource.setPosition(currentPos);
    }
}

} // namespace spectralz
