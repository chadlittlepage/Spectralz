#include "VUMeter.h"
#include <cmath>

namespace spectralz
{

VUMeter::VUMeter()
{
    startTimerHz(30); // 30fps is sufficient for VU meters
}

void VUMeter::setLevels(float left, float right)
{
    // Input is peak amplitude 0-1 where 1.0 = 0dBFS
    // Convert to dB for proper metering
    float leftDb = (left > 0.0001f) ? 20.0f * std::log10(left) : -100.0f;
    float rightDb = (right > 0.0001f) ? 20.0f * std::log10(right) : -100.0f;

    // Normalize to 0-1 range for display (-60dB to 0dB)
    leftLevel = juce::jlimit(0.0f, 1.0f, (leftDb - minDb) / (maxDb - minDb));
    rightLevel = juce::jlimit(0.0f, 1.0f, (rightDb - minDb) / (maxDb - minDb));

    // Store actual dB for readout
    leftDbValue = leftDb;
    rightDbValue = rightDb;

    auto now = juce::Time::currentTimeMillis();

    // Update peaks (instant attack)
    if (leftLevel > leftPeak)
    {
        leftPeak = leftLevel;
        leftPeakDb = leftDb;
        leftPeakTime = now;
    }
    if (rightLevel > rightPeak)
    {
        rightPeak = rightLevel;
        rightPeakDb = rightDb;
        rightPeakTime = now;
    }

    // Fast attack for display level
    if (leftLevel > displayLeftLevel)
        displayLeftLevel = leftLevel;
    if (rightLevel > displayRightLevel)
        displayRightLevel = rightLevel;
}

void VUMeter::setLevelsDB(float leftDB, float rightDB)
{
    // Convert from dB to amplitude, then call setLevels
    float leftAmp = std::pow(10.0f, leftDB / 20.0f);
    float rightAmp = std::pow(10.0f, rightDB / 20.0f);
    setLevels(leftAmp, rightAmp);
}

void VUMeter::resetPeaks()
{
    leftPeak = 0.0f;
    rightPeak = 0.0f;
    leftPeakDb = minDb;
    rightPeakDb = minDb;
}

juce::Colour VUMeter::getColourForLevel(float normalizedLevel) const
{
    // Wider gradient - less green, more transition:
    // Green: 0.0 to 0.3
    // Green to Yellow: 0.3 to 0.6
    // Yellow to Red: 0.6 to 1.0

    if (normalizedLevel < 0.3f)
    {
        // Pure green
        return juce::Colour(0xff00cc44);
    }
    else if (normalizedLevel < 0.6f)
    {
        // Green to yellow
        float t = (normalizedLevel - 0.3f) / 0.3f;
        return juce::Colour(0xff00cc44).interpolatedWith(juce::Colour(0xffffcc00), t);
    }
    else
    {
        // Yellow to red
        float t = (normalizedLevel - 0.6f) / 0.4f;
        return juce::Colour(0xffffcc00).interpolatedWith(juce::Colour(0xffff2222), t);
    }
}

void VUMeter::timerCallback()
{
    auto now = juce::Time::currentTimeMillis();

    // Store old values to check if we need to repaint
    float oldDisplayLeft = displayLeftLevel;
    float oldDisplayRight = displayRightLevel;
    float oldLeftPeak = leftPeak;
    float oldRightPeak = rightPeak;

    // Fast decay for display levels (about 1.5 seconds from full to zero)
    const float decayAmount = 0.08f; // Per frame at 30fps (doubled from 60fps)
    displayLeftLevel = std::max(0.0f, displayLeftLevel - decayAmount);
    displayRightLevel = std::max(0.0f, displayRightLevel - decayAmount);

    // Reset peaks after hold time
    if (now - leftPeakTime > peakHoldMs)
    {
        leftPeak = std::max(0.0f, leftPeak - 0.04f);
        leftPeakDb = minDb + leftPeak * (maxDb - minDb);
    }
    if (now - rightPeakTime > peakHoldMs)
    {
        rightPeak = std::max(0.0f, rightPeak - 0.04f);
        rightPeakDb = minDb + rightPeak * (maxDb - minDb);
    }

    // Only repaint if values actually changed significantly
    bool needsRepaint = std::abs(displayLeftLevel - oldDisplayLeft) > 0.005f ||
                        std::abs(displayRightLevel - oldDisplayRight) > 0.005f ||
                        std::abs(leftPeak - oldLeftPeak) > 0.005f ||
                        std::abs(rightPeak - oldRightPeak) > 0.005f;

    if (needsRepaint)
        repaint();
}

void VUMeter::drawMeter(juce::Graphics& g, const juce::Rectangle<float>& bounds,
                        float level, float peak, bool isLeft)
{
    // Background
    g.setColour(meterBackground);
    g.fillRoundedRectangle(bounds, 2.0f);

    float meterWidth = bounds.getWidth();
    float filledWidth = meterWidth * level;

    if (filledWidth > 0.5f)
    {
        // Create smooth horizontal gradient for the filled portion
        auto filledBounds = juce::Rectangle<float>(
            bounds.getX() + 1,
            bounds.getY() + 1,
            filledWidth - 1,
            bounds.getHeight() - 2);

        // Build gradient with color stops
        juce::ColourGradient gradient(
            getColourForLevel(0.0f),
            filledBounds.getX(), filledBounds.getY(),
            getColourForLevel(level),
            filledBounds.getRight(), filledBounds.getY(),
            false);

        // Add intermediate color stops for smooth transition
        if (level > 0.3f)
            gradient.addColour(0.3f / level, getColourForLevel(0.3f));
        if (level > 0.6f)
            gradient.addColour(0.6f / level, getColourForLevel(0.6f));

        g.setGradientFill(gradient);
        g.fillRect(filledBounds);
    }

    // Draw peak indicator
    if (peak > 0.01f)
    {
        float peakX = bounds.getX() + meterWidth * peak;
        g.setColour(peakLineColor);
        g.fillRect(peakX - 1.5f, bounds.getY(), 3.0f, bounds.getHeight());
    }

    // Draw scale markers at key dB points
    g.setColour(juce::Colour(0x30ffffff));
    float dbMarkers[] = {-48.0f, -36.0f, -24.0f, -18.0f, -12.0f, -6.0f, -3.0f, 0.0f};
    for (float db : dbMarkers)
    {
        float x = bounds.getX() + meterWidth * ((db - minDb) / (maxDb - minDb));
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
    }

    // Channel label
    g.setColour(textColor.withAlpha(0.8f));
    g.setFont(11.0f);
    g.drawText(isLeft ? "L" : "R",
               static_cast<int>(bounds.getX() + 3),
               static_cast<int>(bounds.getY()),
               15, static_cast<int>(bounds.getHeight()),
               juce::Justification::centredLeft);
}

void VUMeter::drawDbReadout(juce::Graphics& g, const juce::Rectangle<int>& bounds,
                            float peakDb, bool isLeft)
{
    // Background
    g.setColour(meterBackground);
    g.fillRoundedRectangle(bounds.toFloat(), 3.0f);

    // Text color based on level
    if (peakDb >= -3.0f)
        g.setColour(juce::Colour(0xffff4444)); // Red for peak
    else if (peakDb >= -12.0f)
        g.setColour(juce::Colour(0xffffcc00)); // Yellow
    else
        g.setColour(juce::Colour(0xff00cc44)); // Green

    // Format dB value
    juce::String dbText;
    if (peakDb <= minDb + 1.0f)
        dbText = "-inf";
    else
        dbText = juce::String(peakDb, 1);

    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText(dbText, bounds, juce::Justification::centred);
}

void VUMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    g.fillAll(backgroundColor);

    // Calculate layout - ensure both meters are exactly the same size
    int dbReadoutWidth = 50;
    int padding = 4;
    int spacing = 2;

    auto meterArea = bounds.reduced(padding, 2);
    int totalHeight = meterArea.getHeight();
    int meterHeight = (totalHeight - spacing) / 2;

    // Calculate meter width (same for both)
    int meterWidth = meterArea.getWidth() - dbReadoutWidth - 4; // 4 = spacing before dB readout

    // Left meter bounds
    auto leftMeterBounds = juce::Rectangle<float>(
        static_cast<float>(meterArea.getX()),
        static_cast<float>(meterArea.getY()),
        static_cast<float>(meterWidth),
        static_cast<float>(meterHeight));

    // Right meter bounds (same size as left)
    auto rightMeterBounds = juce::Rectangle<float>(
        static_cast<float>(meterArea.getX()),
        static_cast<float>(meterArea.getY() + meterHeight + spacing),
        static_cast<float>(meterWidth),
        static_cast<float>(meterHeight));

    // dB readout bounds (aligned to the right)
    auto leftDbBounds = juce::Rectangle<int>(
        meterArea.getRight() - dbReadoutWidth,
        meterArea.getY(),
        dbReadoutWidth,
        meterHeight);

    auto rightDbBounds = juce::Rectangle<int>(
        meterArea.getRight() - dbReadoutWidth,
        meterArea.getY() + meterHeight + spacing,
        dbReadoutWidth,
        meterHeight);

    // Draw meters
    drawMeter(g, leftMeterBounds, displayLeftLevel, leftPeak, true);
    drawMeter(g, rightMeterBounds, displayRightLevel, rightPeak, false);

    // Draw dB readouts (show peak dB value)
    drawDbReadout(g, leftDbBounds, leftPeakDb, true);
    drawDbReadout(g, rightDbBounds, rightPeakDb, false);
}

void VUMeter::resized()
{
    // Layout is handled in paint()
}

// Helper functions
float VUMeter::dbToLinear(float db) const
{
    if (db <= minDb)
        return 0.0f;
    if (db >= maxDb)
        return 1.0f;
    return (db - minDb) / (maxDb - minDb);
}

float VUMeter::linearToDb(float linear) const
{
    if (linear <= 0.0f)
        return minDb;
    return minDb + linear * (maxDb - minDb);
}

} // namespace spectralz
