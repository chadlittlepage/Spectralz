#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace spectralz
{

class TransportControls : public juce::Component,
                          public juce::Timer
{
public:
    TransportControls();
    ~TransportControls() override;

    // State updates
    void setPlaying(bool isPlaying);
    void setLooping(bool isLooping);
    void setPosition(double positionInSeconds);
    void setTotalDuration(double durationInSeconds);
    void setFileLoaded(bool loaded);

    // Callbacks
    std::function<void()> onPlay;
    std::function<void()> onPause;
    std::function<void()> onStop;
    std::function<void()> onSkipBackward;
    std::function<void()> onSkipForward;
    std::function<void(bool)> onLoopToggled;
    std::function<void(double)> onPositionChanged;
    std::function<void()> onOpenFile;
    std::function<void()> onSaveFile;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Timer override
    void timerCallback() override;

private:
    class TransportButton : public juce::Button
    {
    public:
        enum class ButtonType
        {
            Play,
            Pause,
            Stop,
            SkipBack,
            SkipForward,
            Loop,
            Open,
            Save
        };

        TransportButton(const juce::String& name, ButtonType type);
        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    private:
        ButtonType buttonType;
    };

    class TimeDisplay : public juce::Component
    {
    public:
        TimeDisplay();
        void setTime(double seconds, double total);
        void paint(juce::Graphics& g) override;

    private:
        double currentTime = 0.0;
        double totalTime = 0.0;
    };

    void updatePlayPauseButton();
    juce::String formatTime(double seconds) const;

    // Buttons
    std::unique_ptr<TransportButton> playPauseButton;
    std::unique_ptr<TransportButton> stopButton;
    std::unique_ptr<TransportButton> skipBackButton;
    std::unique_ptr<TransportButton> skipForwardButton;
    std::unique_ptr<TransportButton> loopButton;
    std::unique_ptr<TransportButton> openButton;
    std::unique_ptr<TransportButton> saveButton;

    // Position slider
    juce::Slider positionSlider;

    // Time display
    TimeDisplay timeDisplay;

    // State
    bool playing = false;
    bool looping = false;
    bool fileLoaded = false;
    double position = 0.0;
    double duration = 0.0;
    bool sliderBeingDragged = false;

    // Colors
    juce::Colour primaryColor{0xff4a4a4a};
    juce::Colour backgroundColor{0xff2d2d2d};
    juce::Colour textColor{0xffffffff};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportControls)
};

} // namespace spectralz
