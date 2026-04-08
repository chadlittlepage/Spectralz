#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_events/juce_events.h>
#include <functional>
#include <atomic>

namespace spectralz
{

class AudioEngine : public juce::AudioSource,
                    public juce::ChangeListener
{
public:
    AudioEngine();
    ~AudioEngine() override;

    // File I/O
    [[nodiscard]] bool loadFile(const juce::File& file);
    [[nodiscard]] bool saveFile(const juce::File& file, juce::AudioFormat* format = nullptr);
    void unloadFile();

    // Playback control
    void play();
    void pause();
    void stop();
    void setPosition(double positionInSeconds);
    void setStartPosition(double positionInSeconds);  // Sets the "return to" position
    [[nodiscard]] double getStartPosition() const { return startPosition; }
    void setLooping(bool shouldLoop);
    void setLoopRegion(double startTime, double endTime);  // Set loop in/out points
    void clearLoopRegion();  // Clear loop region (loop entire file)
    [[nodiscard]] bool hasLoopRegion() const { return loopRegionSet; }

    // State queries
    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] bool isLooping() const;
    [[nodiscard]] bool hasLoadedFile() const;
    [[nodiscard]] double getCurrentPosition() const;
    [[nodiscard]] double getTotalLength() const;
    [[nodiscard]] double getSampleRate() const;
    [[nodiscard]] int getNumChannels() const;
    [[nodiscard]] juce::int64 getTotalSamples() const;

    // Level metering (returns 0-1 normalized levels)
    [[nodiscard]] float getLeftLevel() const { return leftLevel.load(); }
    [[nodiscard]] float getRightLevel() const { return rightLevel.load(); }

    // Audio buffer access for visualization
    [[nodiscard]] const juce::AudioBuffer<float>* getAudioBuffer() const;
    [[nodiscard]] juce::AudioThumbnail* getThumbnail();

    // Edited audio playback
    void setEditedBuffer(const juce::AudioBuffer<float>& buffer);
    void clearEditedBuffer();
    void setPlayEditedAudio(bool playEdited);
    [[nodiscard]] bool isPlayingEditedAudio() const { return playEditedAudio; }
    [[nodiscard]] bool hasEditedBuffer() const { return editedBufferLoaded; }

    // Callbacks
    std::function<void()> onFileLoaded;
    std::function<void()> onPlaybackStarted;
    std::function<void()> onPlaybackStopped;
    std::function<void(double)> onPositionChanged;

    // AudioSource implementation
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    // ChangeListener implementation
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Device management
    void setupAudioDevice();
    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

private:
    juce::AudioFormatManager formatManager;
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer audioSourcePlayer;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    // Full audio buffer for visualization
    juce::AudioBuffer<float> audioBuffer;
    double loadedSampleRate = 0.0;
    int loadedNumChannels = 0;

    // Thumbnail for waveform display
    juce::AudioThumbnailCache thumbnailCache{5};
    std::unique_ptr<juce::AudioThumbnail> thumbnail;

    bool looping = false;
    double startPosition = 0.0;  // Position to return to on stop

    // Loop region
    double loopStartTime = 0.0;
    double loopEndTime = 0.0;
    bool loopRegionSet = false;

    // Edited audio buffer for preview
    juce::AudioBuffer<float> editedBuffer;
    bool editedBufferLoaded = false;
    bool playEditedAudio = false;
    std::unique_ptr<juce::MemoryAudioSource> editedAudioSource;
    juce::AudioTransportSource editedTransportSource;

    // Level metering (atomic for thread safety)
    std::atomic<float> leftLevel{0.0f};
    std::atomic<float> rightLevel{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};

} // namespace spectralz
