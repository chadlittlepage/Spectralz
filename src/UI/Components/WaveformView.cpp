#include "WaveformView.h"
#include <cmath>

namespace spectralz
{

WaveformView::WaveformView()
{
    updateColorScheme();
}

WaveformView::~WaveformView()
{
    if (thumbnail)
        thumbnail->removeChangeListener(this);
}

void WaveformView::setThumbnail(juce::AudioThumbnail* newThumbnail)
{
    if (thumbnail)
        thumbnail->removeChangeListener(this);

    thumbnail = newThumbnail;
    audioBuffer = nullptr;

    if (thumbnail)
    {
        thumbnail->addChangeListener(this);
        totalDuration = thumbnail->getTotalLength();
        visibleStartTime = 0.0;
        visibleEndTime = totalDuration;
    }

    invalidateWaveformCache();
    repaint();
}

void WaveformView::setAudioBuffer(const juce::AudioBuffer<float>* buffer, double sampleRate)
{
    if (thumbnail)
        thumbnail->removeChangeListener(this);

    thumbnail = nullptr;
    audioBuffer = buffer;
    bufferSampleRate = sampleRate;

    if (audioBuffer)
    {
        totalDuration = static_cast<double>(audioBuffer->getNumSamples()) / sampleRate;
        visibleStartTime = 0.0;
        visibleEndTime = totalDuration;
    }

    invalidateWaveformCache();
    repaint();
}

void WaveformView::clear()
{
    if (thumbnail)
        thumbnail->removeChangeListener(this);

    thumbnail = nullptr;
    audioBuffer = nullptr;
    totalDuration = 0.0;
    visibleStartTime = 0.0;
    visibleEndTime = 10.0;
    selectionActive = false;
    playheadPosition = 0.0;
    invalidateWaveformCache();
    repaint();
}

void WaveformView::setWaveformStyle(WaveformStyle style)
{
    waveformStyle = style;
    invalidateWaveformCache();
    repaint();
}

void WaveformView::setColorScheme(WaveformColorScheme scheme)
{
    colorScheme = scheme;
    updateColorScheme();
    invalidateWaveformCache();
    repaint();
}

void WaveformView::updateColorScheme()
{
    switch (colorScheme)
    {
        case WaveformColorScheme::Blue:
            primaryColor = juce::Colour(0xff4a9eff);
            secondaryColor = juce::Colour(0xff7cb8ff);
            peakColor = juce::Colour(0xffadd8ff);
            rmsColor = primaryColor.withAlpha(0.7f);
            break;

        case WaveformColorScheme::Orange:
            primaryColor = juce::Colour(0xffff6b35);
            secondaryColor = juce::Colour(0xffff9f1c);
            peakColor = juce::Colour(0xffffd166);
            rmsColor = primaryColor.withAlpha(0.7f);
            break;

        case WaveformColorScheme::Green:
            primaryColor = juce::Colour(0xff06d6a0);
            secondaryColor = juce::Colour(0xff1b9aaa);
            peakColor = juce::Colour(0xff8ee3ef);
            rmsColor = primaryColor.withAlpha(0.7f);
            break;

        case WaveformColorScheme::Purple:
            primaryColor = juce::Colour(0xff9b5de5);
            secondaryColor = juce::Colour(0xfff15bb5);
            peakColor = juce::Colour(0xfffee440);
            rmsColor = primaryColor.withAlpha(0.7f);
            break;

        case WaveformColorScheme::Cyan:
            primaryColor = juce::Colour(0xff00f5d4);
            secondaryColor = juce::Colour(0xff00bbf9);
            peakColor = juce::Colour(0xff9b5de5);
            rmsColor = primaryColor.withAlpha(0.7f);
            break;

        case WaveformColorScheme::Fire:
            primaryColor = juce::Colour(0xffff0000);
            secondaryColor = juce::Colour(0xffff6600);
            peakColor = juce::Colour(0xffffcc00);
            rmsColor = primaryColor.withAlpha(0.7f);
            break;

        case WaveformColorScheme::Ice:
            primaryColor = juce::Colour(0xff0077b6);
            secondaryColor = juce::Colour(0xff00b4d8);
            peakColor = juce::Colour(0xffcaf0f8);
            rmsColor = primaryColor.withAlpha(0.7f);
            break;

        case WaveformColorScheme::Neon:
            primaryColor = juce::Colour(0xffff00ff);
            secondaryColor = juce::Colour(0xff00ffff);
            peakColor = juce::Colour(0xffffff00);
            rmsColor = primaryColor.withAlpha(0.7f);
            break;
    }

    selectionColor = primaryColor.withAlpha(0.3f);
    selectionBorderColor = primaryColor;
}

void WaveformView::setShowRMS(bool show)
{
    showRMS = show;
    invalidateWaveformCache();
    repaint();
}

void WaveformView::setShowPeaks(bool show)
{
    showPeaks = show;
    invalidateWaveformCache();
    repaint();
}

void WaveformView::setWaveformColor(juce::Colour color)
{
    primaryColor = color;
    invalidateWaveformCache();
    repaint();
}

void WaveformView::setBackgroundColor(juce::Colour color)
{
    backgroundColor = color;
    invalidateWaveformCache();
    repaint();
}

void WaveformView::setGridColor(juce::Colour color)
{
    gridColor = color;
    invalidateWaveformCache();
    repaint();
}

void WaveformView::setShowGrid(bool show)
{
    showGrid = show;
    invalidateWaveformCache();
    repaint();
}

void WaveformView::setShowTimeline(bool show)
{
    showTimeline = show;
    invalidateWaveformCache();
    repaint();
}

void WaveformView::setVisibleTimeRange(double startTime, double endTime)
{
    visibleStartTime = std::max(0.0, startTime);
    visibleEndTime = std::min(totalDuration, endTime);
    invalidateWaveformCache();
    repaint();

    if (onViewRangeChanged)
        onViewRangeChanged(visibleStartTime, visibleEndTime);
}

void WaveformView::setPlayheadPosition(double timeInSeconds)
{
    // Only repaint if playhead moved significantly
    if (std::abs(timeInSeconds - playheadPosition) < 0.01)
        return;

    double oldPos = playheadPosition;
    playheadPosition = timeInSeconds;

    // Partial repaint for playhead area only
    double range = visibleEndTime - visibleStartTime;
    if (range > 0.0)
    {
        int w = getWidth();
        int oldX = static_cast<int>((oldPos - visibleStartTime) / range * w);
        int newX = static_cast<int>((timeInSeconds - visibleStartTime) / range * w);
        int x1 = std::max(0, std::min(oldX, newX) - 3);
        int x2 = std::min(w, std::max(oldX, newX) + 3);
        repaint(x1, 0, x2 - x1, getHeight());
    }
}

void WaveformView::setSelection(double startTime, double endTime)
{
    selectionStartTime = startTime;
    selectionEndTime = endTime;
    selectionActive = true;
    repaint();
}

void WaveformView::clearSelection()
{
    selectionActive = false;
    repaint();
}

void WaveformView::getSelection(double& startTime, double& endTime) const
{
    startTime = selectionStartTime;
    endTime = selectionEndTime;
}

double WaveformView::xToTime(float x) const
{
    int waveformWidth = getWidth();
    return visibleStartTime + (x / waveformWidth) * (visibleEndTime - visibleStartTime);
}

float WaveformView::timeToX(double time) const
{
    return static_cast<float>((time - visibleStartTime) / (visibleEndTime - visibleStartTime) * getWidth());
}

juce::Colour WaveformView::getColorForAmplitude(float amplitude) const
{
    float t = std::clamp(std::abs(amplitude), 0.0f, 1.0f);

    // Non-linear mapping for more dynamic feel
    t = std::pow(t, 0.7f);

    return primaryColor.interpolatedWith(peakColor, t);
}

juce::Colour WaveformView::getLeftChannelColor() const
{
    return leftChannelColor;
}

juce::Colour WaveformView::getRightChannelColor() const
{
    return rightChannelColor;
}

void WaveformView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto waveformBounds = showTimeline ? bounds.withTrimmedBottom(timelineHeight) : bounds;
    auto timelineBounds = bounds.removeFromBottom(timelineHeight);

    // Check if we need to regenerate the waveform cache
    bool needsRegen = !waveformCacheValid ||
                      cachedWidth != waveformBounds.getWidth() ||
                      cachedHeight != waveformBounds.getHeight() ||
                      cachedStartTime != visibleStartTime ||
                      cachedEndTime != visibleEndTime;

    if (needsRegen)
    {
        renderWaveformToCache(waveformBounds);
    }

    // Draw cached waveform image (fast blit)
    if (waveformCache.isValid())
    {
        g.drawImageAt(waveformCache, waveformBounds.getX(), waveformBounds.getY());
    }

    if (showTimeline)
        drawTimeline(g, timelineBounds);

    // Draw selection (lightweight overlay)
    if (selectionActive)
    {
        float x1 = timeToX(selectionStartTime);
        float x2 = timeToX(selectionEndTime);

        g.setColour(selectionColor);
        g.fillRect(x1, 0.0f, x2 - x1, static_cast<float>(waveformBounds.getHeight()));

        g.setColour(selectionBorderColor);
        g.drawRect(x1, 0.0f, x2 - x1, static_cast<float>(waveformBounds.getHeight()), 2.0f);
    }

    // Draw playhead (simple line - no glow for performance)
    if (playheadPosition >= visibleStartTime && playheadPosition <= visibleEndTime)
    {
        float x = timeToX(playheadPosition);
        g.setColour(playheadColor);
        g.drawLine(x, 0.0f, x, static_cast<float>(waveformBounds.getHeight()), 2.0f);
    }
}

void WaveformView::drawWaveform(juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    switch (waveformStyle)
    {
        case WaveformStyle::Classic:
            drawWaveformClassic(g, bounds);
            break;
        case WaveformStyle::RMSAndPeak:
            drawWaveformRMSAndPeak(g, bounds);
            break;
        case WaveformStyle::Gradient:
            drawWaveformGradient(g, bounds);
            break;
        case WaveformStyle::Stereo:
            drawWaveformStereo(g, bounds);
            break;
        case WaveformStyle::Rainbow:
            drawWaveformRainbow(g, bounds);
            break;
    }
}

void WaveformView::drawWaveformClassic(juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    if (thumbnail && thumbnail->getTotalLength() > 0.0)
    {
        g.setColour(primaryColor);
        thumbnail->drawChannels(g, bounds, visibleStartTime, visibleEndTime, 1.0f);
    }
    else if (audioBuffer && audioBuffer->getNumSamples() > 0)
    {
        g.setColour(primaryColor);

        const int numChannels = audioBuffer->getNumChannels();
        const float centerY = bounds.getCentreY();
        const float amplitude = bounds.getHeight() * 0.45f;

        const int startSample = static_cast<int>(visibleStartTime * bufferSampleRate);
        const int endSample = static_cast<int>(visibleEndTime * bufferSampleRate);
        const int samplesPerPixel = std::max(1, (endSample - startSample) / bounds.getWidth());

        juce::Path waveformPath;

        for (int x = 0; x < bounds.getWidth(); ++x)
        {
            int sampleIndex = startSample + x * samplesPerPixel;
            if (sampleIndex >= audioBuffer->getNumSamples())
                break;

            float minVal = 0.0f, maxVal = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* data = audioBuffer->getReadPointer(ch);
                for (int i = 0; i < samplesPerPixel && (sampleIndex + i) < audioBuffer->getNumSamples(); ++i)
                {
                    float sample = data[sampleIndex + i];
                    minVal = std::min(minVal, sample);
                    maxVal = std::max(maxVal, sample);
                }
            }

            float y1 = centerY - maxVal * amplitude;
            float y2 = centerY - minVal * amplitude;

            if (x == 0)
                waveformPath.startNewSubPath(static_cast<float>(x), y1);
            else
                waveformPath.lineTo(static_cast<float>(x), y1);
        }

        // Complete the path
        for (int x = bounds.getWidth() - 1; x >= 0; --x)
        {
            int sampleIndex = startSample + x * samplesPerPixel;
            if (sampleIndex >= audioBuffer->getNumSamples())
                continue;

            float minVal = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* data = audioBuffer->getReadPointer(ch);
                for (int i = 0; i < samplesPerPixel && (sampleIndex + i) < audioBuffer->getNumSamples(); ++i)
                {
                    float sample = data[sampleIndex + i];
                    minVal = std::min(minVal, sample);
                }
            }

            float y2 = centerY - minVal * amplitude;
            waveformPath.lineTo(static_cast<float>(x), y2);
        }

        waveformPath.closeSubPath();
        g.fillPath(waveformPath);
    }
}

void WaveformView::drawWaveformRMSAndPeak(juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    if (!audioBuffer || audioBuffer->getNumSamples() == 0)
    {
        // Draw center line
        g.setColour(gridColor);
        float centerY = bounds.getCentreY();
        g.drawLine(0.0f, centerY, static_cast<float>(bounds.getWidth()), centerY, 1.0f);
        return;
    }

    const int numChannels = audioBuffer->getNumChannels();
    const float centerY = bounds.getCentreY();
    const float amplitude = bounds.getHeight() * 0.45f;

    const int startSample = static_cast<int>(visibleStartTime * bufferSampleRate);
    const int endSample = static_cast<int>(visibleEndTime * bufferSampleRate);
    const int samplesPerPixel = std::max(1, (endSample - startSample) / bounds.getWidth());

    // Collect RMS and Peak values
    std::vector<float> peakMax(bounds.getWidth());
    std::vector<float> peakMin(bounds.getWidth());
    std::vector<float> rmsValues(bounds.getWidth());

    for (int x = 0; x < bounds.getWidth(); ++x)
    {
        int sampleIndex = startSample + x * samplesPerPixel;
        if (sampleIndex >= audioBuffer->getNumSamples())
            break;

        float minVal = 0.0f, maxVal = 0.0f;
        float sumSquares = 0.0f;
        int count = 0;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* data = audioBuffer->getReadPointer(ch);
            for (int i = 0; i < samplesPerPixel && (sampleIndex + i) < audioBuffer->getNumSamples(); ++i)
            {
                float sample = data[sampleIndex + i];
                minVal = std::min(minVal, sample);
                maxVal = std::max(maxVal, sample);
                sumSquares += sample * sample;
                count++;
            }
        }

        peakMax[x] = maxVal;
        peakMin[x] = minVal;
        rmsValues[x] = count > 0 ? std::sqrt(sumSquares / count) : 0.0f;
    }

    // Draw RMS (filled, semi-transparent)
    if (showRMS)
    {
        juce::Path rmsPath;
        bool pathStarted = false;

        for (int x = 0; x < bounds.getWidth(); ++x)
        {
            float rms = rmsValues[x];
            float y1 = centerY - rms * amplitude;

            if (!pathStarted)
            {
                rmsPath.startNewSubPath(static_cast<float>(x), y1);
                pathStarted = true;
            }
            else
            {
                rmsPath.lineTo(static_cast<float>(x), y1);
            }
        }

        for (int x = bounds.getWidth() - 1; x >= 0; --x)
        {
            float rms = rmsValues[x];
            float y2 = centerY + rms * amplitude;
            rmsPath.lineTo(static_cast<float>(x), y2);
        }

        rmsPath.closeSubPath();

        // Gradient fill for RMS
        juce::ColourGradient rmsGradient(
            rmsColor.brighter(0.3f), 0, centerY - amplitude,
            rmsColor.darker(0.2f), 0, centerY + amplitude,
            false);
        g.setGradientFill(rmsGradient);
        g.fillPath(rmsPath);
    }

    // Draw Peak (outline with gradient)
    if (showPeaks)
    {
        juce::Path peakPath;

        for (int x = 0; x < bounds.getWidth(); ++x)
        {
            float y1 = centerY - peakMax[x] * amplitude;
            if (x == 0)
                peakPath.startNewSubPath(static_cast<float>(x), y1);
            else
                peakPath.lineTo(static_cast<float>(x), y1);
        }

        for (int x = bounds.getWidth() - 1; x >= 0; --x)
        {
            float y2 = centerY - peakMin[x] * amplitude;
            peakPath.lineTo(static_cast<float>(x), y2);
        }

        peakPath.closeSubPath();

        // Draw peak outline with gradient stroke
        g.setColour(peakColor);
        g.strokePath(peakPath, juce::PathStrokeType(1.5f));
    }

    // Draw center line
    g.setColour(gridColor.brighter(0.3f));
    g.drawLine(0.0f, centerY, static_cast<float>(bounds.getWidth()), centerY, 0.5f);
}

void WaveformView::drawWaveformGradient(juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    if (!audioBuffer || audioBuffer->getNumSamples() == 0)
        return;

    const int numChannels = audioBuffer->getNumChannels();
    const float centerY = bounds.getCentreY();
    const float amplitude = bounds.getHeight() * 0.45f;

    const int startSample = static_cast<int>(visibleStartTime * bufferSampleRate);
    const int endSample = static_cast<int>(visibleEndTime * bufferSampleRate);
    const int samplesPerPixel = std::max(1, (endSample - startSample) / bounds.getWidth());

    for (int x = 0; x < bounds.getWidth(); ++x)
    {
        int sampleIndex = startSample + x * samplesPerPixel;
        if (sampleIndex >= audioBuffer->getNumSamples())
            break;

        float minVal = 0.0f, maxVal = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* data = audioBuffer->getReadPointer(ch);
            for (int i = 0; i < samplesPerPixel && (sampleIndex + i) < audioBuffer->getNumSamples(); ++i)
            {
                float sample = data[sampleIndex + i];
                minVal = std::min(minVal, sample);
                maxVal = std::max(maxVal, sample);
            }
        }

        float y1 = centerY - maxVal * amplitude;
        float y2 = centerY - minVal * amplitude;

        // Color based on amplitude
        float maxAmp = std::max(std::abs(maxVal), std::abs(minVal));
        juce::Colour lineColor = getColorForAmplitude(maxAmp);

        // Draw vertical line with gradient
        juce::ColourGradient lineGradient(
            lineColor.brighter(0.5f), static_cast<float>(x), y1,
            lineColor.darker(0.3f), static_cast<float>(x), y2,
            false);
        g.setGradientFill(lineGradient);
        g.drawLine(static_cast<float>(x), y1, static_cast<float>(x), y2, 1.0f);
    }
}

void WaveformView::drawWaveformStereo(juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    if (!audioBuffer || audioBuffer->getNumSamples() == 0 || audioBuffer->getNumChannels() < 2)
    {
        drawWaveformRMSAndPeak(g, bounds);
        return;
    }

    const float centerY = bounds.getCentreY();
    const float amplitude = bounds.getHeight() * 0.45f;

    const int startSample = static_cast<int>(visibleStartTime * bufferSampleRate);
    const int endSample = static_cast<int>(visibleEndTime * bufferSampleRate);
    const int samplesPerPixel = std::max(1, (endSample - startSample) / bounds.getWidth());

    // Draw left channel (top half, going up)
    juce::Path leftPath;
    // Draw right channel (bottom half, going down)
    juce::Path rightPath;

    const float* leftData = audioBuffer->getReadPointer(0);
    const float* rightData = audioBuffer->getReadPointer(1);

    for (int x = 0; x < bounds.getWidth(); ++x)
    {
        int sampleIndex = startSample + x * samplesPerPixel;
        if (sampleIndex >= audioBuffer->getNumSamples())
            break;

        float leftMax = 0.0f, rightMax = 0.0f;

        for (int i = 0; i < samplesPerPixel && (sampleIndex + i) < audioBuffer->getNumSamples(); ++i)
        {
            leftMax = std::max(leftMax, std::abs(leftData[sampleIndex + i]));
            rightMax = std::max(rightMax, std::abs(rightData[sampleIndex + i]));
        }

        float leftY = centerY - leftMax * amplitude;
        float rightY = centerY + rightMax * amplitude;

        if (x == 0)
        {
            leftPath.startNewSubPath(static_cast<float>(x), centerY);
            rightPath.startNewSubPath(static_cast<float>(x), centerY);
        }

        leftPath.lineTo(static_cast<float>(x), leftY);
        rightPath.lineTo(static_cast<float>(x), rightY);
    }

    // Complete paths back to center
    for (int x = bounds.getWidth() - 1; x >= 0; --x)
    {
        leftPath.lineTo(static_cast<float>(x), centerY);
        rightPath.lineTo(static_cast<float>(x), centerY);
    }

    leftPath.closeSubPath();
    rightPath.closeSubPath();

    // Draw with gradients
    juce::ColourGradient leftGradient(
        leftChannelColor.withAlpha(0.8f), 0, centerY,
        leftChannelColor.withAlpha(0.3f), 0, centerY - amplitude,
        false);
    g.setGradientFill(leftGradient);
    g.fillPath(leftPath);

    juce::ColourGradient rightGradient(
        rightChannelColor.withAlpha(0.8f), 0, centerY,
        rightChannelColor.withAlpha(0.3f), 0, centerY + amplitude,
        false);
    g.setGradientFill(rightGradient);
    g.fillPath(rightPath);

    // Draw outlines
    g.setColour(leftChannelColor);
    g.strokePath(leftPath, juce::PathStrokeType(1.0f));

    g.setColour(rightChannelColor);
    g.strokePath(rightPath, juce::PathStrokeType(1.0f));

    // Center line
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawLine(0.0f, centerY, static_cast<float>(bounds.getWidth()), centerY, 1.0f);
}

void WaveformView::drawWaveformRainbow(juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    if (!audioBuffer || audioBuffer->getNumSamples() == 0)
        return;

    const int numChannels = audioBuffer->getNumChannels();
    const float centerY = bounds.getCentreY();
    const float amplitude = bounds.getHeight() * 0.45f;

    const int startSample = static_cast<int>(visibleStartTime * bufferSampleRate);
    const int endSample = static_cast<int>(visibleEndTime * bufferSampleRate);
    const int samplesPerPixel = std::max(1, (endSample - startSample) / bounds.getWidth());

    for (int x = 0; x < bounds.getWidth(); ++x)
    {
        int sampleIndex = startSample + x * samplesPerPixel;
        if (sampleIndex >= audioBuffer->getNumSamples())
            break;

        float minVal = 0.0f, maxVal = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* data = audioBuffer->getReadPointer(ch);
            for (int i = 0; i < samplesPerPixel && (sampleIndex + i) < audioBuffer->getNumSamples(); ++i)
            {
                float sample = data[sampleIndex + i];
                minVal = std::min(minVal, sample);
                maxVal = std::max(maxVal, sample);
            }
        }

        float y1 = centerY - maxVal * amplitude;
        float y2 = centerY - minVal * amplitude;

        // Rainbow hue based on x position
        float hue = static_cast<float>(x) / bounds.getWidth();
        float maxAmp = std::max(std::abs(maxVal), std::abs(minVal));
        float saturation = 0.7f + 0.3f * maxAmp;
        float brightness = 0.5f + 0.5f * maxAmp;

        juce::Colour lineColor = juce::Colour::fromHSV(hue, saturation, brightness, 1.0f);

        g.setColour(lineColor);
        g.drawLine(static_cast<float>(x), y1, static_cast<float>(x), y2, 1.5f);
    }
}

void WaveformView::drawGrid(juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    g.setColour(gridColor);

    // Horizontal center line
    float centerY = bounds.getCentreY();
    g.drawLine(0.0f, centerY, static_cast<float>(bounds.getWidth()), centerY, 0.5f);

    // dB lines at -6dB and -12dB
    float db6 = bounds.getHeight() * 0.25f;
    g.setColour(gridColor.withAlpha(0.3f));
    g.drawLine(0.0f, centerY - db6, static_cast<float>(bounds.getWidth()), centerY - db6, 0.5f);
    g.drawLine(0.0f, centerY + db6, static_cast<float>(bounds.getWidth()), centerY + db6, 0.5f);

    // Time grid lines
    double duration = visibleEndTime - visibleStartTime;
    double gridInterval = 1.0;

    if (duration > 60.0) gridInterval = 10.0;
    else if (duration > 30.0) gridInterval = 5.0;
    else if (duration > 10.0) gridInterval = 2.0;
    else if (duration > 5.0) gridInterval = 1.0;
    else if (duration > 2.0) gridInterval = 0.5;
    else if (duration > 1.0) gridInterval = 0.25;
    else gridInterval = 0.1;

    g.setColour(gridColor);
    double startGrid = std::ceil(visibleStartTime / gridInterval) * gridInterval;
    for (double t = startGrid; t < visibleEndTime; t += gridInterval)
    {
        float x = timeToX(t);
        g.drawLine(x, 0.0f, x, static_cast<float>(bounds.getHeight()), 0.5f);
    }
}

void WaveformView::drawTimeline(juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    g.setColour(backgroundColor.brighter(0.15f));
    g.fillRect(bounds);

    g.setColour(gridColor.brighter(0.5f));
    g.drawLine(0.0f, static_cast<float>(bounds.getY()),
               static_cast<float>(bounds.getWidth()), static_cast<float>(bounds.getY()), 1.0f);

    // Time labels
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(10.0f);

    double duration = visibleEndTime - visibleStartTime;
    double labelInterval = 1.0;

    if (duration > 120.0) labelInterval = 30.0;
    else if (duration > 60.0) labelInterval = 10.0;
    else if (duration > 30.0) labelInterval = 5.0;
    else if (duration > 10.0) labelInterval = 2.0;
    else if (duration > 5.0) labelInterval = 1.0;
    else labelInterval = 0.5;

    double startLabel = std::ceil(visibleStartTime / labelInterval) * labelInterval;
    for (double t = startLabel; t < visibleEndTime; t += labelInterval)
    {
        float x = timeToX(t);

        // Format time string
        int minutes = static_cast<int>(t) / 60;
        int seconds = static_cast<int>(t) % 60;
        int millis = static_cast<int>((t - std::floor(t)) * 1000);

        juce::String timeStr;
        if (minutes > 0)
            timeStr = juce::String::formatted("%d:%02d", minutes, seconds);
        else if (labelInterval >= 1.0)
            timeStr = juce::String::formatted("%d", seconds);
        else
            timeStr = juce::String::formatted("%d.%03d", seconds, millis);

        g.drawText(timeStr, static_cast<int>(x) - 20, bounds.getY() + 2, 40, bounds.getHeight() - 4,
                   juce::Justification::centred);
    }
}

void WaveformView::resized()
{
    invalidateWaveformCache();
    repaint();
}

void WaveformView::mouseDown(const juce::MouseEvent& event)
{
    isDragging = true;
    dragStart = event.position;
    // Store the existing selection to restore if it's just a click
    pendingSelectionStart = selectionStartTime;
    pendingSelectionEnd = selectionEndTime;
    pendingSelectionActive = selectionActive;

    double clickTime = xToTime(event.position.x);

    if (event.mods.isShiftDown() && selectionActive)
    {
        // Shift-click extends selection
        if (clickTime < (selectionStartTime + selectionEndTime) / 2)
            selectionStartTime = clickTime;
        else
            selectionEndTime = clickTime;
    }
    // Don't modify selection on simple click - wait for drag or mouseUp
}

void WaveformView::mouseDrag(const juce::MouseEvent& event)
{
    if (!isDragging)
        return;

    // Only start a new selection if actually dragging (not just clicking)
    if (std::abs(event.position.x - dragStart.x) >= 3)
    {
        double oldStart = selectionStartTime;
        double oldEnd = selectionEndTime;
        double startTime = xToTime(dragStart.x);
        double dragTime = xToTime(event.position.x);

        // New selection from drag start to current position
        selectionStartTime = std::min(startTime, dragTime);
        selectionEndTime = std::max(startTime, dragTime);
        selectionActive = true;

        // Partial repaint - only affected selection area
        int x1 = static_cast<int>(timeToX(std::min({oldStart, oldEnd, selectionStartTime, selectionEndTime, pendingSelectionStart, pendingSelectionEnd}))) - 5;
        int x2 = static_cast<int>(timeToX(std::max({oldStart, oldEnd, selectionStartTime, selectionEndTime, pendingSelectionStart, pendingSelectionEnd}))) + 5;
        repaint(std::max(0, x1), 0, x2 - x1, getHeight());
    }
}

void WaveformView::mouseUp(const juce::MouseEvent& event)
{
    if (isDragging)
    {
        isDragging = false;

        if (std::abs(event.position.x - dragStart.x) < 3)
        {
            // Click without drag - set playhead position but KEEP existing selection
            // Restore the selection that was saved in mouseDown
            selectionStartTime = pendingSelectionStart;
            selectionEndTime = pendingSelectionEnd;
            selectionActive = pendingSelectionActive;

            if (onTimeClicked)
                onTimeClicked(xToTime(event.position.x));

            repaint();
        }
        else if (onSelectionChanged && selectionActive)
        {
            // Drag completed - notify about new selection
            onSelectionChanged(selectionStartTime, selectionEndTime);
        }
    }
}

void WaveformView::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    double zoomFactor = 1.0 - wheel.deltaY * 0.1;
    double mouseTime = xToTime(static_cast<float>(event.x));

    double newDuration = (visibleEndTime - visibleStartTime) * zoomFactor;
    double mouseFraction = (mouseTime - visibleStartTime) / (visibleEndTime - visibleStartTime);

    visibleStartTime = mouseTime - mouseFraction * newDuration;
    visibleEndTime = visibleStartTime + newDuration;

    if (visibleStartTime < 0.0)
    {
        visibleEndTime -= visibleStartTime;
        visibleStartTime = 0.0;
    }
    if (visibleEndTime > totalDuration)
    {
        visibleStartTime -= (visibleEndTime - totalDuration);
        visibleEndTime = totalDuration;
    }

    invalidateWaveformCache();
    repaint();

    if (onViewRangeChanged)
        onViewRangeChanged(visibleStartTime, visibleEndTime);
}

void WaveformView::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == thumbnail)
    {
        totalDuration = thumbnail->getTotalLength();
        invalidateWaveformCache();
        repaint();
    }
}

void WaveformView::invalidateWaveformCache()
{
    waveformCacheValid = false;
}

void WaveformView::renderWaveformToCache(const juce::Rectangle<int>& bounds)
{
    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
        return;

    // Create or resize the cache image
    if (!waveformCache.isValid() ||
        waveformCache.getWidth() != bounds.getWidth() ||
        waveformCache.getHeight() != bounds.getHeight())
    {
        waveformCache = juce::Image(juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    }

    // Clear and draw to cache
    juce::Graphics g(waveformCache);

    // Draw gradient background
    juce::ColourGradient bgGradient(
        backgroundColor.brighter(0.05f), 0, 0,
        backgroundColor.darker(0.1f), 0, static_cast<float>(bounds.getHeight()),
        false);
    g.setGradientFill(bgGradient);
    g.fillRect(0, 0, bounds.getWidth(), bounds.getHeight());

    // Draw grid onto cache
    if (showGrid)
    {
        juce::Rectangle<int> cacheBounds(0, 0, bounds.getWidth(), bounds.getHeight());
        drawGrid(g, cacheBounds);
    }

    // Draw waveform onto cache
    juce::Rectangle<int> cacheBounds(0, 0, bounds.getWidth(), bounds.getHeight());
    drawWaveform(g, cacheBounds);

    // Update cache metadata
    waveformCacheValid = true;
    cachedStartTime = visibleStartTime;
    cachedEndTime = visibleEndTime;
    cachedWidth = bounds.getWidth();
    cachedHeight = bounds.getHeight();
}

} // namespace spectralz
