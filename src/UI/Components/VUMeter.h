#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>

namespace spectralz
{

/**
 * Stereo VU Meter with horizontal bars and dB readouts
 * Gradient goes from green -> yellow -> red at peak
 */
class VUMeter : public juce::Component,
                public juce::Timer
{
public:
    VUMeter();
    ~VUMeter() override = default;

    // Set levels (0.0 to 1.0 linear, or use dB version)
    void setLevels(float leftLevel, float rightLevel);
    void setLevelsDB(float leftDB, float rightDB);

    // Peak hold
    void setPeakHoldTime(int milliseconds) { peakHoldMs = milliseconds; }
    void resetPeaks();

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Timer for decay
    void timerCallback() override;

private:
    [[nodiscard]] float dbToLinear(float db) const;
    [[nodiscard]] float linearToDb(float linear) const;
    [[nodiscard]] juce::Colour getColourForLevel(float normalizedLevel) const;
    void drawMeter(juce::Graphics& g, const juce::Rectangle<float>& bounds,
                   float level, float peak, bool isLeft);
    void drawDbReadout(juce::Graphics& g, const juce::Rectangle<int>& bounds,
                       float db, bool isLeft);

    // Current levels (0-1 normalized)
    float leftLevel = 0.0f;
    float rightLevel = 0.0f;

    // Peak levels
    float leftPeak = 0.0f;
    float rightPeak = 0.0f;
    float leftPeakDb = -100.0f;
    float rightPeakDb = -100.0f;
    juce::int64 leftPeakTime = 0;
    juce::int64 rightPeakTime = 0;
    int peakHoldMs = 2000;

    // Display levels (smoothed)
    float displayLeftLevel = 0.0f;
    float displayRightLevel = 0.0f;

    // Actual dB values for readout
    float leftDbValue = -100.0f;
    float rightDbValue = -100.0f;

    // dB range
    float minDb = -60.0f;
    float maxDb = 0.0f;

    // Colors
    juce::Colour backgroundColor{0xff1a1a1a};
    juce::Colour meterBackground{0xff0d0d0d};
    juce::Colour textColor{0xffffffff};
    juce::Colour peakLineColor{0xffffffff};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUMeter)
};

} // namespace spectralz
