#include "SpectrogramView.h"
#include <cmath>

namespace spectralz
{

SpectrogramView::SpectrogramView()
{
    openGLContext.setRenderer(this);
    openGLContext.attachTo(*this);
    openGLContext.setContinuousRepainting(false);

    createColorMap();
}

SpectrogramView::~SpectrogramView()
{
    openGLContext.detach();
}

void SpectrogramView::setSpectrogramData(const FFTProcessor::SpectrogramData& data)
{
    spectrogramData = data;
    dataLoaded = true;

    totalDuration = static_cast<double>(data.numFrames * data.hopSize) / data.sampleRate;
    viewStartTime = 0.0;
    viewEndTime = totalDuration;

    textureNeedsUpdate = true;
    overviewNeedsUpdate = true;
    repaint();
}

void SpectrogramView::updateOverview()
{
    if (!dataLoaded || spectrogramData.numFrames == 0 || spectrogramData.numBins == 0)
        return;

    // Create a low-res overview of the entire spectrogram (fast to scale)
    int overviewWidth = std::min(2048, spectrogramData.numFrames);
    int overviewHeight = 512;

    overviewImage = juce::Image(juce::Image::ARGB, overviewWidth, overviewHeight, true);

    float logMin = std::log10(minFrequency);
    float logMax = std::log10(maxFrequency);

    for (int x = 0; x < overviewWidth; ++x)
    {
        int frame = static_cast<int>(static_cast<float>(x) * spectrogramData.numFrames / overviewWidth);
        frame = std::clamp(frame, 0, spectrogramData.numFrames - 1);

        for (int y = 0; y < overviewHeight; ++y)
        {
            float normalizedY = 1.0f - static_cast<float>(y) / static_cast<float>(overviewHeight);
            float frequency = logarithmicFrequency
                ? std::pow(10.0f, logMin + normalizedY * (logMax - logMin))
                : minFrequency + normalizedY * (maxFrequency - minFrequency);

            int bin = static_cast<int>(frequency * spectrogramData.fftSize / spectrogramData.sampleRate);
            bin = std::clamp(bin, 0, spectrogramData.numBins - 1);

            float magnitude = spectrogramData.magnitudes[static_cast<size_t>(frame)][static_cast<size_t>(bin)];
            float magnitudeDB = (magnitude > 1e-10f) ? 20.0f * std::log10(magnitude) : minDB;

            overviewImage.setPixelAt(x, y, magnitudeToColor(magnitudeDB));
        }
    }

    overviewNeedsUpdate = false;
}

void SpectrogramView::clear()
{
    spectrogramData = FFTProcessor::SpectrogramData();
    dataLoaded = false;
    totalDuration = 0.0;
    viewStartTime = 0.0;
    viewEndTime = 10.0;
    selectionActive = false;
    textureNeedsUpdate = true;
    repaint();
}

void SpectrogramView::setColorMap(SpectrogramColorMap colorMap)
{
    currentColorMap = colorMap;
    createColorMap();
    textureNeedsUpdate = true;
    repaint();
}

void SpectrogramView::createColorMap()
{
    colorMapData.colors.resize(512);  // Higher resolution for smoother gradients

    auto interpolateColor = [](const juce::Colour& c1, const juce::Colour& c2, float t) -> juce::Colour
    {
        return juce::Colour(
            static_cast<juce::uint8>(c1.getRed() + t * (c2.getRed() - c1.getRed())),
            static_cast<juce::uint8>(c1.getGreen() + t * (c2.getGreen() - c1.getGreen())),
            static_cast<juce::uint8>(c1.getBlue() + t * (c2.getBlue() - c1.getBlue())),
            static_cast<juce::uint8>(255));
    };

    auto createGradient = [&](const std::vector<std::pair<float, juce::Colour>>& stops)
    {
        for (int i = 0; i < 512; ++i)
        {
            float t = static_cast<float>(i) / 511.0f;

            // Find the two stops we're between
            size_t stopIdx = 0;
            for (size_t j = 1; j < stops.size(); ++j)
            {
                if (t <= stops[j].first)
                {
                    stopIdx = j - 1;
                    break;
                }
                stopIdx = j - 1;
            }

            if (stopIdx >= stops.size() - 1)
                stopIdx = stops.size() - 2;

            float localT = (t - stops[stopIdx].first) / (stops[stopIdx + 1].first - stops[stopIdx].first);
            localT = std::clamp(localT, 0.0f, 1.0f);

            colorMapData.colors[i] = interpolateColor(stops[stopIdx].second, stops[stopIdx + 1].second, localT);
        }
    };

    switch (currentColorMap)
    {
        case SpectrogramColorMap::SpectralLayers:
        {
            // SpectraLayers style: deep blue -> blue -> orange -> yellow -> white
            std::vector<std::pair<float, juce::Colour>> stops = {
                {0.00f, juce::Colour(0xff000814)},  // Very dark blue (almost black)
                {0.15f, juce::Colour(0xff001d3d)},  // Deep navy blue
                {0.30f, juce::Colour(0xff003566)},  // Navy blue
                {0.45f, juce::Colour(0xff0066aa)},  // Medium blue
                {0.55f, juce::Colour(0xff1089c4)},  // Blue
                {0.65f, juce::Colour(0xffff6b35)},  // Orange
                {0.75f, juce::Colour(0xffff9500)},  // Bright orange
                {0.85f, juce::Colour(0xffffd000)},  // Yellow-orange
                {0.92f, juce::Colour(0xffffea00)},  // Yellow
                {1.00f, juce::Colour(0xffffffff)}   // White
            };
            createGradient(stops);
            break;
        }

        case SpectrogramColorMap::IzotopeRX:
        {
            // iZotope RX style: dark -> deep blue -> blue -> orange -> yellow
            std::vector<std::pair<float, juce::Colour>> stops = {
                {0.00f, juce::Colour(0xff0a0a14)},  // Nearly black
                {0.12f, juce::Colour(0xff0d1b2a)},  // Very dark blue
                {0.25f, juce::Colour(0xff1b263b)},  // Dark blue
                {0.38f, juce::Colour(0xff415a77)},  // Steel blue
                {0.50f, juce::Colour(0xff778da9)},  // Light steel blue
                {0.60f, juce::Colour(0xffcc5803)},  // Dark orange
                {0.72f, juce::Colour(0xffe85d04)},  // Orange
                {0.82f, juce::Colour(0xfff48c06)},  // Light orange
                {0.90f, juce::Colour(0xfffaa307)},  // Yellow-orange
                {0.95f, juce::Colour(0xffffba08)},  // Yellow
                {1.00f, juce::Colour(0xffffffff)}   // White
            };
            createGradient(stops);
            break;
        }

        case SpectrogramColorMap::Viridis:
        {
            std::vector<std::pair<float, juce::Colour>> stops = {
                {0.00f, juce::Colour(0xff440154)},
                {0.25f, juce::Colour(0xff3b528b)},
                {0.50f, juce::Colour(0xff21918c)},
                {0.75f, juce::Colour(0xff5ec962)},
                {1.00f, juce::Colour(0xfffde725)}
            };
            createGradient(stops);
            break;
        }

        case SpectrogramColorMap::Magma:
        {
            std::vector<std::pair<float, juce::Colour>> stops = {
                {0.00f, juce::Colour(0xff000004)},
                {0.25f, juce::Colour(0xff3b0f70)},
                {0.50f, juce::Colour(0xffb63679)},
                {0.75f, juce::Colour(0xfffc8961)},
                {1.00f, juce::Colour(0xfffcfdbf)}
            };
            createGradient(stops);
            break;
        }

        case SpectrogramColorMap::Inferno:
        {
            std::vector<std::pair<float, juce::Colour>> stops = {
                {0.00f, juce::Colour(0xff000004)},
                {0.25f, juce::Colour(0xff420a68)},
                {0.50f, juce::Colour(0xffbc3754)},
                {0.75f, juce::Colour(0xfff98e09)},
                {1.00f, juce::Colour(0xffffffc8)}
            };
            createGradient(stops);
            break;
        }

        case SpectrogramColorMap::Plasma:
        {
            std::vector<std::pair<float, juce::Colour>> stops = {
                {0.00f, juce::Colour(0xff0d0887)},
                {0.25f, juce::Colour(0xff7e03a8)},
                {0.50f, juce::Colour(0xffcc4778)},
                {0.75f, juce::Colour(0xfff89540)},
                {1.00f, juce::Colour(0xfff0f921)}
            };
            createGradient(stops);
            break;
        }

        case SpectrogramColorMap::Grayscale:
        {
            for (int i = 0; i < 512; ++i)
            {
                auto val = static_cast<juce::uint8>(i * 255 / 511);
                colorMapData.colors[i] = juce::Colour(val, val, val);
            }
            break;
        }

        case SpectrogramColorMap::Thermal:
        {
            std::vector<std::pair<float, juce::Colour>> stops = {
                {0.00f, juce::Colour(0xff000000)},  // Black
                {0.25f, juce::Colour(0xff8b0000)},  // Dark red
                {0.50f, juce::Colour(0xffff4500)},  // Orange red
                {0.75f, juce::Colour(0xffffa500)},  // Orange
                {0.90f, juce::Colour(0xffffff00)},  // Yellow
                {1.00f, juce::Colour(0xffffffff)}   // White
            };
            createGradient(stops);
            break;
        }

        case SpectrogramColorMap::Ocean:
        {
            std::vector<std::pair<float, juce::Colour>> stops = {
                {0.00f, juce::Colour(0xff000428)},  // Deep ocean
                {0.30f, juce::Colour(0xff004e92)},  // Ocean blue
                {0.50f, juce::Colour(0xff0077b6)},  // Blue
                {0.70f, juce::Colour(0xff00b4d8)},  // Light blue
                {0.85f, juce::Colour(0xff90e0ef)},  // Cyan
                {1.00f, juce::Colour(0xffffffff)}   // White
            };
            createGradient(stops);
            break;
        }

        case SpectrogramColorMap::Sunset:
        {
            std::vector<std::pair<float, juce::Colour>> stops = {
                {0.00f, juce::Colour(0xff10002b)},  // Deep purple
                {0.20f, juce::Colour(0xff3c096c)},  // Purple
                {0.40f, juce::Colour(0xff7b2cbf)},  // Violet
                {0.55f, juce::Colour(0xffe0aaff)},  // Light purple
                {0.70f, juce::Colour(0xffff6d00)},  // Orange
                {0.85f, juce::Colour(0xffffba08)},  // Yellow-orange
                {1.00f, juce::Colour(0xffffffff)}   // White
            };
            createGradient(stops);
            break;
        }
    }
}

juce::Colour SpectrogramView::magnitudeToColor(float magnitudeDB) const
{
    // Normalize to 0-1 range with contrast and brightness adjustments
    float normalized = (magnitudeDB - minDB) / (maxDB - minDB);

    // Apply contrast (centered around 0.5)
    normalized = 0.5f + (normalized - 0.5f) * contrast;

    // Apply brightness
    normalized *= brightness;

    // Apply a slight gamma curve for better visual perception
    normalized = std::pow(std::clamp(normalized, 0.0f, 1.0f), 0.85f);

    int index = static_cast<int>(normalized * 511.0f);
    index = std::clamp(index, 0, 511);
    return colorMapData.colors[index];
}

void SpectrogramView::setFrequencyRange(float minHz, float maxHz)
{
    minFrequency = minHz;
    maxFrequency = maxHz;
    textureNeedsUpdate = true;
    repaint();
}

void SpectrogramView::setTimeRange(double startTime, double endTime)
{
    viewStartTime = startTime;
    viewEndTime = endTime;
    repaint();
}

void SpectrogramView::setDBRange(float min, float max)
{
    minDB = min;
    maxDB = max;
    textureNeedsUpdate = true;
    repaint();
}

void SpectrogramView::setLogarithmicFrequency(bool useLog)
{
    logarithmicFrequency = useLog;
    textureNeedsUpdate = true;
    repaint();
}

void SpectrogramView::setBrightness(float b)
{
    brightness = std::clamp(b, 0.0f, 2.0f);
    textureNeedsUpdate = true;
    repaint();
}

void SpectrogramView::setContrast(float c)
{
    contrast = std::clamp(c, 0.0f, 2.0f);
    textureNeedsUpdate = true;
    repaint();
}

void SpectrogramView::setVisibleTimeRange(double startTime, double endTime)
{
    viewStartTime = std::max(0.0, startTime);
    viewEndTime = std::min(totalDuration, endTime);
    repaint();
}

void SpectrogramView::zoomToSelection(double startTime, double endTime, float startFreq, float endFreq)
{
    viewStartTime = startTime;
    viewEndTime = endTime;
    minFrequency = startFreq;
    maxFrequency = endFreq;
    textureNeedsUpdate = true;
    repaint();
}

void SpectrogramView::resetZoom()
{
    viewStartTime = 0.0;
    viewEndTime = totalDuration;
    minFrequency = 20.0f;
    maxFrequency = 20000.0f;
    textureNeedsUpdate = true;
    repaint();
}

void SpectrogramView::zoomIn()
{
    double center = (viewStartTime + viewEndTime) * 0.5;
    double currentDuration = viewEndTime - viewStartTime;
    double newDuration = currentDuration * 0.7;  // Zoom in by 30%

    // Limit minimum zoom (0.1 seconds)
    newDuration = std::max(0.1, newDuration);

    viewStartTime = center - newDuration * 0.5;
    viewEndTime = center + newDuration * 0.5;

    // Clamp to bounds
    if (viewStartTime < 0.0)
    {
        viewEndTime = std::min(newDuration, totalDuration);
        viewStartTime = 0.0;
    }
    if (viewEndTime > totalDuration)
    {
        viewStartTime = std::max(0.0, totalDuration - newDuration);
        viewEndTime = totalDuration;
    }

    repaint();
}

void SpectrogramView::zoomOut()
{
    double center = (viewStartTime + viewEndTime) * 0.5;
    double currentDuration = viewEndTime - viewStartTime;
    double newDuration = currentDuration * 1.4;  // Zoom out by 40%

    // Limit maximum zoom (full duration)
    newDuration = std::min(newDuration, totalDuration);

    viewStartTime = center - newDuration * 0.5;
    viewEndTime = center + newDuration * 0.5;

    // Clamp to bounds
    if (viewStartTime < 0.0)
    {
        viewEndTime = std::min(newDuration, totalDuration);
        viewStartTime = 0.0;
    }
    if (viewEndTime > totalDuration)
    {
        viewStartTime = std::max(0.0, totalDuration - newDuration);
        viewEndTime = totalDuration;
    }

    repaint();
}

void SpectrogramView::setPlayheadPosition(double timeInSeconds)
{
    // Only repaint if playhead moved more than 1 pixel
    double oldX = timeToX(playheadPosition);
    double newX = timeToX(timeInSeconds);

    playheadPosition = timeInSeconds;

    if (std::abs(newX - oldX) >= 1.0)
    {
        // Repaint only the playhead area (old and new positions)
        int x1 = static_cast<int>(std::min(oldX, newX)) - 5;
        int x2 = static_cast<int>(std::max(oldX, newX)) + 5;
        repaint(x1, 0, x2 - x1, getHeight());
    }
}

void SpectrogramView::setSelectionEnabled(bool enabled)
{
    selectionEnabled = enabled;
}

void SpectrogramView::getSelection(double& startTime, double& endTime, float& startFreq, float& endFreq) const
{
    startTime = selectionStartTime;
    endTime = selectionEndTime;
    startFreq = selectionStartFreq;
    endFreq = selectionEndFreq;
}

void SpectrogramView::clearSelection()
{
    selectionActive = false;
    repaint();
}

void SpectrogramView::setInteractionMode(InteractionMode mode)
{
    interactionMode = mode;

    // Update cursor based on mode
    switch (mode)
    {
        case InteractionMode::Selection:
        case InteractionMode::FrequencySelect:
        case InteractionMode::TimeSelect:
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
            showBrushCursor = false;
            break;

        case InteractionMode::BrushBoost:
        case InteractionMode::BrushAttenuate:
        case InteractionMode::Eraser:
            setMouseCursor(juce::MouseCursor::NoCursor);
            showBrushCursor = true;
            break;
    }

    repaint();
}

void SpectrogramView::setBrushRadius(float radiusInPixels)
{
    brushRadius = std::clamp(radiusInPixels, 5.0f, 200.0f);
    repaint();
}

void SpectrogramView::setBrushStrength(float strength)
{
    brushStrength = std::clamp(strength, 0.0f, 1.0f);
}

void SpectrogramView::setBrushFalloff(float falloff)
{
    brushFalloff = std::clamp(falloff, 0.0f, 1.0f);
}

void SpectrogramView::setSpectralEditor(SpectralEditor* editor)
{
    spectralEditor = editor;
}

void SpectrogramView::setShowEditOverlay(bool show)
{
    showEditOverlay = show;
    repaint();
}

void SpectrogramView::refreshEditOverlay()
{
    repaint();
}

void SpectrogramView::drawBrushCursor(juce::Graphics& g)
{
    if (!showBrushCursor)
        return;

    float x = currentMousePosition.x;
    float y = currentMousePosition.y;

    // Don't draw if cursor is off-screen
    if (x < -100.0f || y < -100.0f || x > getWidth() + 100.0f || y > getHeight() + 100.0f)
        return;

    // Draw brush circle based on mode
    juce::Colour brushColor;
    switch (interactionMode)
    {
        case InteractionMode::BrushBoost:
            brushColor = juce::Colour(0x8800ff88);  // Green for boost
            break;
        case InteractionMode::BrushAttenuate:
            brushColor = juce::Colour(0x88ff8800);  // Orange for attenuate
            break;
        case InteractionMode::Eraser:
            brushColor = juce::Colour(0x88ff4444);  // Red for erase
            break;
        default:
            return;
    }

    // Draw outer circle (brush extent)
    g.setColour(brushColor);
    g.drawEllipse(x - brushRadius, y - brushRadius,
                  brushRadius * 2.0f, brushRadius * 2.0f, 2.0f);

    // Draw inner circle showing falloff
    float innerRadius = brushRadius * (1.0f - brushFalloff);
    if (innerRadius > 2.0f)
    {
        g.setColour(brushColor.withAlpha(0.5f));
        g.drawEllipse(x - innerRadius, y - innerRadius,
                      innerRadius * 2.0f, innerRadius * 2.0f, 1.0f);
    }

    // Draw center crosshair
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.drawLine(x - 5.0f, y, x + 5.0f, y, 1.0f);
    g.drawLine(x, y - 5.0f, x, y + 5.0f, 1.0f);
}

void SpectrogramView::drawEditOverlay(juce::Graphics& g)
{
    if (!showEditOverlay || spectralEditor == nullptr || !spectralEditor->hasSourceData())
        return;

    const auto* sourceData = spectralEditor->getSourceData();
    if (sourceData == nullptr || sourceData->numFrames <= 0)
        return;

    // Pre-calculate constants once
    const double timePerFrame = static_cast<double>(sourceData->hopSize) / sourceData->sampleRate;
    const float binFreqStep = static_cast<float>(sourceData->sampleRate) / static_cast<float>(sourceData->fftSize);
    const int maxBin = static_cast<int>(sourceData->fftSize / 2 + 1);
    const float height = static_cast<float>(getHeight());

    // Iterate through all layers and draw edit regions
    const int numLayers = spectralEditor->getNumLayers();
    for (int layerIdx = 0; layerIdx < numLayers; ++layerIdx)
    {
        const auto* layer = spectralEditor->getLayer(layerIdx);
        if (layer == nullptr || !layer->isVisible() || layer->isMuted())
            continue;

        const float layerOpacity = layer->getOpacity();
        const size_t numEdits = layer->getNumEdits();

        // Draw each edit in this layer
        for (size_t editIdx = 0; editIdx < numEdits; ++editIdx)
        {
            const auto* edit = layer->getEdit(editIdx);
            if (edit == nullptr)
                continue;

            SpectralRegion region = edit->getAffectedRegion();

            // Quick bounds check and clamp
            if (region.endFrame <= 0 || region.startFrame >= sourceData->numFrames)
                continue;

            region.startFrame = std::max(0, region.startFrame);
            region.endFrame = std::min(sourceData->numFrames, region.endFrame);
            region.startBin = std::max(0, region.startBin);
            region.endBin = std::min(maxBin, region.endBin);

            if (region.startFrame >= region.endFrame || region.startBin >= region.endBin)
                continue;

            // Convert frame/bin to time/freq
            double startTime = region.startFrame * timePerFrame;
            double endTime = region.endFrame * timePerFrame;

            // Skip if outside visible range
            if (endTime < viewStartTime || startTime > viewEndTime)
                continue;

            float startFreq = region.startBin * binFreqStep;
            float endFreq = region.endBin * binFreqStep;

            float x1 = static_cast<float>(timeToX(startTime));
            float x2 = static_cast<float>(timeToX(endTime));
            float y1 = frequencyToY(endFreq) * height;
            float y2 = frequencyToY(startFreq) * height;

            if (y1 > y2) std::swap(y1, y2);

            // Choose color based on edit type (simplified)
            juce::Colour overlayColor;
            auto editType = edit->getType();
            if (editType == SpectralOpType::GainMultiply || editType == SpectralOpType::GainAdd)
                overlayColor = (edit->getGainDeltaDB() > 0 || edit->getGainFactor() > 1.0f)
                    ? juce::Colour(0x4000ff00) : juce::Colour(0x40ff8800);
            else if (editType == SpectralOpType::Erase)
                overlayColor = juce::Colour(0x40ff0000);
            else
                overlayColor = juce::Colour(0x30ffffff);

            overlayColor = overlayColor.withMultipliedAlpha(layerOpacity);

            // Draw filled region only (skip border for performance)
            g.setColour(overlayColor);
            g.fillRect(x1, y1, x2 - x1, y2 - y1);
        }
    }
}

void SpectrogramView::processBrushPoint(const juce::MouseEvent& event)
{
    BrushPoint point;
    point.time = xToTime(event.position.x);
    point.frequency = yToFrequency(event.position.y);
    point.pressure = event.isPressureValid() ? event.pressure : 1.0f;

    currentBrushStroke.push_back(point);
    lastBrushPosition = event.position;
}

void SpectrogramView::finalizeBrushStroke()
{
    if (!currentBrushStroke.empty() && onBrushStroke)
    {
        onBrushStroke(currentBrushStroke, interactionMode);
    }

    currentBrushStroke.clear();
    brushActive = false;
}

float SpectrogramView::frequencyToY(float frequency) const
{
    if (logarithmicFrequency)
    {
        float logMin = std::log10(minFrequency);
        float logMax = std::log10(maxFrequency);
        float logFreq = std::log10(std::clamp(frequency, minFrequency, maxFrequency));
        return 1.0f - (logFreq - logMin) / (logMax - logMin);
    }
    return 1.0f - (frequency - minFrequency) / (maxFrequency - minFrequency);
}

float SpectrogramView::yToFrequency(float y) const
{
    float normalizedY = 1.0f - y / static_cast<float>(getHeight());
    if (logarithmicFrequency)
    {
        float logMin = std::log10(minFrequency);
        float logMax = std::log10(maxFrequency);
        return std::pow(10.0f, logMin + normalizedY * (logMax - logMin));
    }
    return minFrequency + normalizedY * (maxFrequency - minFrequency);
}

double SpectrogramView::timeToX(double time) const
{
    return (time - viewStartTime) / (viewEndTime - viewStartTime) * getWidth();
}

double SpectrogramView::xToTime(float x) const
{
    return viewStartTime + (x / getWidth()) * (viewEndTime - viewStartTime);
}

void SpectrogramView::updateTexture()
{
    if (!dataLoaded || spectrogramData.numFrames == 0 || spectrogramData.numBins == 0)
        return;

    // Render at screen resolution for the visible portion only
    int imageWidth = getWidth();
    int imageHeight = getHeight();

    if (imageWidth <= 0 || imageHeight <= 0)
        return;

    // Calculate visible frame range
    int startFrame = static_cast<int>(viewStartTime / totalDuration * spectrogramData.numFrames);
    int endFrame = static_cast<int>(viewEndTime / totalDuration * spectrogramData.numFrames);
    startFrame = std::clamp(startFrame, 0, spectrogramData.numFrames - 1);
    endFrame = std::clamp(endFrame, 1, spectrogramData.numFrames);

    int visibleFrames = endFrame - startFrame;
    if (visibleFrames <= 0)
        return;

    spectrogramImage = juce::Image(juce::Image::ARGB, imageWidth, imageHeight, true);

    // Pre-calculate log frequency values for performance
    float logMin = std::log10(minFrequency);
    float logMax = std::log10(maxFrequency);

    for (int x = 0; x < imageWidth; ++x)
    {
        // Map screen X to frame index within visible range
        float t = static_cast<float>(x) / static_cast<float>(imageWidth);
        int frame = startFrame + static_cast<int>(t * visibleFrames);
        frame = std::clamp(frame, startFrame, endFrame - 1);

        for (int y = 0; y < imageHeight; ++y)
        {
            float normalizedY = 1.0f - static_cast<float>(y) / static_cast<float>(imageHeight);
            float frequency;

            if (logarithmicFrequency)
            {
                frequency = std::pow(10.0f, logMin + normalizedY * (logMax - logMin));
            }
            else
            {
                frequency = minFrequency + normalizedY * (maxFrequency - minFrequency);
            }

            int bin = static_cast<int>(frequency * spectrogramData.fftSize / spectrogramData.sampleRate);
            bin = std::clamp(bin, 0, spectrogramData.numBins - 1);

            float magnitude = spectrogramData.magnitudes[static_cast<size_t>(frame)][static_cast<size_t>(bin)];

            // Convert to dB with floor
            float magnitudeDB;
            if (magnitude > 1e-10f)
            {
                magnitudeDB = 20.0f * std::log10(magnitude);
            }
            else
            {
                magnitudeDB = minDB;
            }

            spectrogramImage.setPixelAt(x, y, magnitudeToColor(magnitudeDB));
        }
    }

    // Update cache tracking
    cachedViewStart = viewStartTime;
    cachedViewEnd = viewEndTime;
    cachedWidth = imageWidth;
    cachedHeight = imageHeight;
    textureNeedsUpdate = false;
}

void SpectrogramView::paint(juce::Graphics& g)
{
    // Dark background
    g.fillAll(juce::Colour(0xff0a0a14));

    if (!dataLoaded)
        return;

    // Generate overview if needed (one-time, fast after that)
    if (overviewNeedsUpdate)
        updateOverview();

    // Check if high-res cache is valid
    bool viewChanged = std::abs(cachedViewStart - viewStartTime) > 0.0001 ||
                       std::abs(cachedViewEnd - viewEndTime) > 0.0001;
    bool sizeChanged = cachedWidth != getWidth() || cachedHeight != getHeight();

    if (textureNeedsUpdate || viewChanged || sizeChanged)
    {
        if (textureNeedsUpdate)
        {
            // First load - generate high-res immediately
            updateTexture();
            useOverviewMode = false;
        }
        else
        {
            // View changed - use overview mode and defer high-res
            useOverviewMode = true;
            regenerationPending = true;
            stopTimer();
            startTimer(100); // Regenerate 100ms after last zoom
        }
    }

    // Draw spectrogram
    if (useOverviewMode && overviewImage.isValid())
    {
        // Fast path: scale overview image to current view (instant)
        float srcX = static_cast<float>(viewStartTime / totalDuration * overviewImage.getWidth());
        float srcW = static_cast<float>((viewEndTime - viewStartTime) / totalDuration * overviewImage.getWidth());

        srcX = std::max(0.0f, srcX);
        srcW = std::max(1.0f, std::min(srcW, static_cast<float>(overviewImage.getWidth()) - srcX));

        g.drawImage(overviewImage,
                    0, 0, getWidth(), getHeight(),
                    static_cast<int>(srcX), 0,
                    static_cast<int>(srcW), overviewImage.getHeight());
    }
    else if (spectrogramImage.isValid())
    {
        // High-res path: direct blit (image matches current view)
        g.drawImageAt(spectrogramImage, 0, 0);
    }

    // Draw selection rectangle
    if (selectionActive)
    {
        float x1 = static_cast<float>(timeToX(selectionStartTime));
        float x2 = static_cast<float>(timeToX(selectionEndTime));
        float y1 = frequencyToY(selectionEndFreq) * getHeight();
        float y2 = frequencyToY(selectionStartFreq) * getHeight();

        // Selection fill with glow
        g.setColour(juce::Colour(0x30ffffff));
        g.fillRect(x1, y1, x2 - x1, y2 - y1);

        // Selection border
        g.setColour(juce::Colour(0xccffffff));
        g.drawRect(x1, y1, x2 - x1, y2 - y1, 1.5f);
    }

    // Draw playhead with glow
    if (playheadPosition >= viewStartTime && playheadPosition <= viewEndTime)
    {
        float x = static_cast<float>(timeToX(playheadPosition));

        // Glow effect
        for (int i = 3; i >= 0; --i)
        {
            float alpha = 0.1f * (4 - i);
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.drawLine(x, 0.0f, x, static_cast<float>(getHeight()), 1.0f + i * 2.0f);
        }

        g.setColour(juce::Colours::white);
        g.drawLine(x, 0.0f, x, static_cast<float>(getHeight()), 2.0f);
    }

    // Draw frequency axis labels
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.setFont(10.0f);

    std::vector<float> freqLabels = {50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    for (float freq : freqLabels)
    {
        if (freq >= minFrequency && freq <= maxFrequency)
        {
            float y = frequencyToY(freq) * static_cast<float>(getHeight());
            juce::String label = (freq >= 1000) ? juce::String(freq / 1000.0f, 1) + "k" : juce::String(static_cast<int>(freq));

            // Draw subtle grid line
            g.setColour(juce::Colours::white.withAlpha(0.1f));
            g.drawLine(0.0f, y, static_cast<float>(getWidth()), y, 0.5f);

            // Clamp label position to stay within bounds with top padding
            int labelY = static_cast<int>(y) - 7;
            labelY = std::max(4, std::min(labelY, getHeight() - 14));

            // Draw label with background
            g.setColour(juce::Colour(0x80000000));
            g.fillRect(2, labelY, 30, 14);
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawText(label, 4, labelY + 1, 28, 12, juce::Justification::left);
        }
    }

    // Draw edit overlay (shows edited regions)
    drawEditOverlay(g);

    // Draw brush cursor (must be drawn last to be on top)
    drawBrushCursor(g);
}

void SpectrogramView::resized()
{
    textureNeedsUpdate = true;
}

void SpectrogramView::mouseDown(const juce::MouseEvent& event)
{
    // Handle brush/eraser modes
    if (interactionMode == InteractionMode::BrushBoost ||
        interactionMode == InteractionMode::BrushAttenuate ||
        interactionMode == InteractionMode::Eraser)
    {
        brushActive = true;
        currentBrushStroke.clear();
        processBrushPoint(event);
        return;
    }

    // Handle selection modes
    if (!selectionEnabled)
    {
        if (onTimeClicked)
            onTimeClicked(xToTime(static_cast<float>(event.x)));
        return;
    }

    isDragging = true;
    dragStart = event.position;

    // Handle different selection modes
    switch (interactionMode)
    {
        case InteractionMode::Selection:
            selectionStartTime = xToTime(event.position.x);
            selectionStartFreq = yToFrequency(event.position.y);
            selectionEndTime = selectionStartTime;
            selectionEndFreq = selectionStartFreq;
            break;

        case InteractionMode::FrequencySelect:
            // Full frequency range at time position
            selectionStartTime = xToTime(event.position.x);
            selectionStartFreq = minFrequency;
            selectionEndTime = selectionStartTime;
            selectionEndFreq = maxFrequency;
            break;

        case InteractionMode::TimeSelect:
            // Full time range at frequency
            selectionStartTime = viewStartTime;
            selectionStartFreq = yToFrequency(event.position.y);
            selectionEndTime = viewEndTime;
            selectionEndFreq = selectionStartFreq;
            break;

        default:
            break;
    }

    selectionActive = true;
}

void SpectrogramView::mouseDrag(const juce::MouseEvent& event)
{
    currentMousePosition = event.position;

    // Handle brush modes
    if (brushActive)
    {
        processBrushPoint(event);
        repaint();
        return;
    }

    if (!isDragging)
        return;

    // Handle different selection modes during drag
    switch (interactionMode)
    {
        case InteractionMode::Selection:
            selectionEndTime = xToTime(event.position.x);
            selectionEndFreq = yToFrequency(event.position.y);
            break;

        case InteractionMode::FrequencySelect:
            selectionEndTime = xToTime(event.position.x);
            // Keep full frequency range
            break;

        case InteractionMode::TimeSelect:
            selectionEndFreq = yToFrequency(event.position.y);
            // Keep full time range
            break;

        default:
            break;
    }

    // Normalize selection bounds
    double startT = selectionStartTime, endT = selectionEndTime;
    float startF = selectionStartFreq, endF = selectionEndFreq;

    if (endT < startT)
        std::swap(startT, endT);
    if (endF < startF)
        std::swap(startF, endF);

    selectionStartTime = startT;
    selectionEndTime = endT;
    selectionStartFreq = startF;
    selectionEndFreq = endF;

    repaint();
}

void SpectrogramView::mouseUp(const juce::MouseEvent& event)
{
    // Handle brush mode
    if (brushActive)
    {
        finalizeBrushStroke();
        repaint();
        return;
    }

    if (isDragging)
    {
        isDragging = false;

        // Check if selection is too small (just a click)
        if (std::abs(event.position.x - dragStart.x) < 5 &&
            std::abs(event.position.y - dragStart.y) < 5)
        {
            selectionActive = false;
            if (onTimeClicked)
                onTimeClicked(xToTime(event.position.x));
        }
        else if (onSelectionChanged)
        {
            onSelectionChanged(selectionStartTime, selectionEndTime, selectionStartFreq, selectionEndFreq);
        }

        repaint();
    }
}

void SpectrogramView::mouseMove(const juce::MouseEvent& event)
{
    currentMousePosition = event.position;

    // Only need to repaint if showing brush cursor
    if (showBrushCursor)
    {
        repaint();
    }
}

void SpectrogramView::mouseEnter(const juce::MouseEvent& event)
{
    currentMousePosition = event.position;

    // Repaint to show brush cursor if in brush mode
    if (interactionMode == InteractionMode::BrushBoost ||
        interactionMode == InteractionMode::BrushAttenuate ||
        interactionMode == InteractionMode::Eraser)
    {
        repaint();
    }
}

void SpectrogramView::mouseExit(const juce::MouseEvent& event)
{
    (void)event;

    // Need to repaint to hide brush cursor when mouse leaves component
    if (interactionMode == InteractionMode::BrushBoost ||
        interactionMode == InteractionMode::BrushAttenuate ||
        interactionMode == InteractionMode::Eraser)
    {
        // Temporarily disable cursor for paint, but keep mode
        currentMousePosition = juce::Point<float>(-1000.0f, -1000.0f);  // Off-screen
        repaint();
    }
}

void SpectrogramView::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    // Smooth zoom with mouse wheel, centered on cursor
    // Use a gentler zoom factor for smoother experience
    double zoomAmount = wheel.deltaY * 0.15;  // Smoother zoom steps
    double zoomFactor = std::exp(-zoomAmount);  // Exponential for consistent feel
    zoomFactor = std::clamp(zoomFactor, 0.8, 1.25);  // Limit per-step zoom

    double mouseTime = xToTime(static_cast<float>(event.x));
    double currentDuration = viewEndTime - viewStartTime;
    double newDuration = currentDuration * zoomFactor;

    // Limit zoom range (min 0.1 seconds, max full duration)
    newDuration = std::clamp(newDuration, 0.1, totalDuration);

    // Keep cursor position at same screen location (not centered)
    double mouseFraction = (mouseTime - viewStartTime) / currentDuration;
    viewStartTime = mouseTime - mouseFraction * newDuration;
    viewEndTime = viewStartTime + newDuration;

    // Clamp to bounds while preserving zoom level
    if (viewStartTime < 0.0)
    {
        viewEndTime = std::min(newDuration, totalDuration);
        viewStartTime = 0.0;
    }
    if (viewEndTime > totalDuration)
    {
        viewStartTime = std::max(0.0, totalDuration - newDuration);
        viewEndTime = totalDuration;
    }

    repaint();
}

void SpectrogramView::newOpenGLContextCreated()
{
    // OpenGL initialization (for future GPU-accelerated rendering)
}

void SpectrogramView::renderOpenGL()
{
    // Future: GPU-accelerated rendering
}

void SpectrogramView::openGLContextClosing()
{
    // Cleanup
}

void SpectrogramView::timerCallback()
{
    // Timer fired - zooming has stopped, regenerate at full quality
    stopTimer();
    regenerationPending = false;
    useOverviewMode = false;
    updateTexture();
    repaint();
}

} // namespace spectralz
