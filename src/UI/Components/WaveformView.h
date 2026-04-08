#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>

namespace spectralz
{

enum class WaveformStyle
{
    Classic,        // Simple filled waveform
    RMSAndPeak,     // RMS filled with peak outline
    Gradient,       // Gradient based on amplitude
    Stereo,         // Separate L/R channels with different colors
    Rainbow         // Rainbow gradient based on amplitude
};

enum class WaveformColorScheme
{
    Blue,           // Classic blue
    Orange,         // iZotope RX style orange
    Green,          // Green spectrum
    Purple,         // Purple/magenta
    Cyan,           // Cyan/teal
    Fire,           // Red/orange/yellow gradient
    Ice,            // Blue/cyan/white gradient
    Neon            // Bright neon colors
};

class WaveformView : public juce::Component,
                     public juce::ChangeListener
{
public:
    WaveformView();
    ~WaveformView() override;

    // Set the audio thumbnail to display
    void setThumbnail(juce::AudioThumbnail* thumbnail);

    // Set the audio buffer directly
    void setAudioBuffer(const juce::AudioBuffer<float>* buffer, double sampleRate);

    // Clear the display
    void clear();

    // Display style settings
    void setWaveformStyle(WaveformStyle style);
    void setColorScheme(WaveformColorScheme scheme);
    void setShowRMS(bool show);
    void setShowPeaks(bool show);

    // Legacy color settings
    void setWaveformColor(juce::Colour color);
    void setBackgroundColor(juce::Colour color);
    void setGridColor(juce::Colour color);
    void setShowGrid(bool show);
    void setShowTimeline(bool show);

    // Viewport control
    void setVisibleTimeRange(double startTime, double endTime);
    [[nodiscard]] double getVisibleStartTime() const { return visibleStartTime; }
    [[nodiscard]] double getVisibleEndTime() const { return visibleEndTime; }
    [[nodiscard]] double getTotalDuration() const { return totalDuration; }

    // Playhead position
    void setPlayheadPosition(double timeInSeconds);

    // Selection
    void setSelection(double startTime, double endTime);
    void clearSelection();
    [[nodiscard]] bool hasSelection() const { return selectionActive; }
    void getSelection(double& startTime, double& endTime) const;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // ChangeListener override
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Callbacks
    std::function<void(double)> onTimeClicked;
    std::function<void(double, double)> onSelectionChanged;
    std::function<void(double, double)> onViewRangeChanged;

private:
    [[nodiscard]] double xToTime(float x) const;
    [[nodiscard]] float timeToX(double time) const;
    void drawWaveform(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawWaveformClassic(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawWaveformRMSAndPeak(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawWaveformGradient(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawWaveformStereo(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawWaveformRainbow(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawGrid(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawTimeline(juce::Graphics& g, const juce::Rectangle<int>& bounds);

    [[nodiscard]] juce::Colour getColorForAmplitude(float amplitude) const;
    [[nodiscard]] juce::Colour getLeftChannelColor() const;
    [[nodiscard]] juce::Colour getRightChannelColor() const;
    void updateColorScheme();

    juce::AudioThumbnail* thumbnail = nullptr;
    const juce::AudioBuffer<float>* audioBuffer = nullptr;
    double bufferSampleRate = 44100.0;

    // Style settings
    WaveformStyle waveformStyle = WaveformStyle::RMSAndPeak;
    WaveformColorScheme colorScheme = WaveformColorScheme::Orange;
    bool showRMS = true;
    bool showPeaks = true;

    // Colors
    juce::Colour primaryColor{0xffff6b35};      // Main waveform color
    juce::Colour secondaryColor{0xffff9f1c};    // Secondary/gradient color
    juce::Colour peakColor{0xffffd166};         // Peak outline color
    juce::Colour rmsColor{0xccff6b35};          // RMS fill color
    juce::Colour leftChannelColor{0xff4ecdc4};  // Left channel (stereo mode)
    juce::Colour rightChannelColor{0xffff6b6b}; // Right channel (stereo mode)
    juce::Colour backgroundColor{0xff1a1a1a};   // Darker background
    juce::Colour gridColor{0x30ffffff};
    juce::Colour playheadColor{0xffffffff};
    juce::Colour selectionColor{0x40ff6b35};
    juce::Colour selectionBorderColor{0xffff6b35};

    // Display options
    bool showGrid = true;
    bool showTimeline = true;
    int timelineHeight = 20;

    // Time range
    double visibleStartTime = 0.0;
    double visibleEndTime = 10.0;
    double totalDuration = 0.0;

    // Playhead
    double playheadPosition = 0.0;

    // Selection
    bool selectionActive = false;
    bool isDragging = false;
    double selectionStartTime = 0.0;
    double selectionEndTime = 0.0;
    juce::Point<float> dragStart;

    // Pending selection (to restore on click without drag)
    double pendingSelectionStart = 0.0;
    double pendingSelectionEnd = 0.0;
    bool pendingSelectionActive = false;

    // Cached waveform image for performance
    juce::Image waveformCache;
    bool waveformCacheValid = false;
    double cachedStartTime = 0.0;
    double cachedEndTime = 0.0;
    int cachedWidth = 0;
    int cachedHeight = 0;

    void invalidateWaveformCache();
    void renderWaveformToCache(const juce::Rectangle<int>& bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformView)
};

} // namespace spectralz
