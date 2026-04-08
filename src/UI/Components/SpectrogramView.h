#pragma once

#include <juce_opengl/juce_opengl.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../DSP/FFTProcessor.h"
#include "../../DSP/SpectralEditor.h"
#include <vector>

namespace spectralz
{

// Interaction modes for the spectrogram view
enum class InteractionMode
{
    Selection,          // Standard rectangular selection
    FrequencySelect,    // Select full frequency range at time position
    TimeSelect,         // Select full time range at frequency
    BrushBoost,         // Brush to boost/amplify spectral content
    BrushAttenuate,     // Brush to attenuate spectral content
    Eraser              // Erase spectral content
};

// Brush stroke point for continuous editing
struct BrushPoint
{
    double time;
    float frequency;
    float pressure;  // For pressure-sensitive tablets (0.0 to 1.0)
};

enum class SpectrogramColorMap
{
    SpectralLayers,     // Dark blue -> blue -> orange -> yellow -> white (like SpectraLayers)
    IzotopeRX,          // Similar to RX - deep blue to orange/yellow
    Viridis,            // Scientific color map
    Magma,              // Dark purple to yellow
    Inferno,            // Black to yellow through red
    Plasma,             // Purple to yellow
    Grayscale,          // Black to white
    Thermal,            // Black -> red -> orange -> yellow -> white
    Ocean,              // Deep blue to cyan to white
    Sunset              // Deep purple -> magenta -> orange -> yellow
};

class SpectrogramView : public juce::Component,
                        public juce::OpenGLRenderer,
                        private juce::Timer
{
public:
    SpectrogramView();
    ~SpectrogramView() override;

    // Set spectrogram data
    void setSpectrogramData(const FFTProcessor::SpectrogramData& data);
    void clear();

    // Display settings
    void setColorMap(SpectrogramColorMap colorMap);
    void setFrequencyRange(float minHz, float maxHz);
    void setTimeRange(double startTime, double endTime);
    void setDBRange(float minDB, float maxDB);
    void setLogarithmicFrequency(bool useLog);
    void setBrightness(float brightness);  // 0.0 to 2.0, default 1.0
    void setContrast(float contrast);      // 0.0 to 2.0, default 1.0

    // Viewport control
    void setVisibleTimeRange(double startTime, double endTime);
    void zoomToSelection(double startTime, double endTime, float startFreq, float endFreq);
    void resetZoom();
    void zoomIn();
    void zoomOut();

    // Playhead position
    void setPlayheadPosition(double timeInSeconds);

    // Selection
    void setSelectionEnabled(bool enabled);
    [[nodiscard]] bool hasSelection() const { return selectionActive; }
    void getSelection(double& startTime, double& endTime, float& startFreq, float& endFreq) const;
    void clearSelection();

    // Interaction mode
    void setInteractionMode(InteractionMode mode);
    [[nodiscard]] InteractionMode getInteractionMode() const { return interactionMode; }

    // Brush settings
    void setBrushRadius(float radiusInPixels);
    void setBrushStrength(float strength);  // 0.0 to 1.0
    void setBrushFalloff(float falloff);    // 0.0 = hard edge, 1.0 = soft gradient
    [[nodiscard]] float getBrushRadius() const { return brushRadius; }
    [[nodiscard]] float getBrushStrength() const { return brushStrength; }
    [[nodiscard]] float getBrushFalloff() const { return brushFalloff; }

    // Editor integration
    void setSpectralEditor(SpectralEditor* editor);

    // Edit overlay (shows edited regions)
    void setShowEditOverlay(bool show);
    void refreshEditOverlay();

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // OpenGL overrides
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    // Callbacks
    std::function<void(double, double, float, float)> onSelectionChanged;
    std::function<void(double)> onTimeClicked;
    std::function<void(const std::vector<BrushPoint>&, InteractionMode)> onBrushStroke;

private:
    void drawBrushCursor(juce::Graphics& g);
    void drawEditOverlay(juce::Graphics& g);
    void processBrushPoint(const juce::MouseEvent& event);
    void finalizeBrushStroke();
    struct ColorMapData
    {
        std::vector<juce::Colour> colors;
    };

    void createColorMap();
    [[nodiscard]] juce::Colour magnitudeToColor(float magnitudeDB) const;
    void updateTexture();
    [[nodiscard]] float frequencyToY(float frequency) const;
    [[nodiscard]] float yToFrequency(float y) const;
    [[nodiscard]] double timeToX(double time) const;
    [[nodiscard]] double xToTime(float x) const;

    // OpenGL context
    juce::OpenGLContext openGLContext;

    // Spectrogram data
    FFTProcessor::SpectrogramData spectrogramData;
    bool dataLoaded = false;

    // Display settings
    SpectrogramColorMap currentColorMap = SpectrogramColorMap::SpectralLayers;
    ColorMapData colorMapData;
    float minFrequency = 20.0f;
    float maxFrequency = 20000.0f;
    float minDB = -90.0f;
    float maxDB = 0.0f;
    float brightness = 0.5f;
    float contrast = 1.2f;
    bool logarithmicFrequency = true;

    // Viewport
    double viewStartTime = 0.0;
    double viewEndTime = 10.0;
    double totalDuration = 0.0;

    // Playhead
    double playheadPosition = 0.0;

    // Selection
    bool selectionEnabled = true;
    bool selectionActive = false;
    bool isDragging = false;
    double selectionStartTime = 0.0;
    double selectionEndTime = 0.0;
    float selectionStartFreq = 0.0f;
    float selectionEndFreq = 0.0f;
    juce::Point<float> dragStart;

    // High-res texture for current view (rendered at screen resolution)
    juce::Image spectrogramImage;
    bool textureNeedsUpdate = true;

    // Low-res overview for fast zooming (covers entire file)
    juce::Image overviewImage;
    bool overviewNeedsUpdate = true;

    // Cache tracking for view-dependent rendering
    double cachedViewStart = -1.0;
    double cachedViewEnd = -1.0;
    int cachedWidth = 0;
    int cachedHeight = 0;

    // Deferred regeneration for smooth zooming
    bool regenerationPending = false;
    bool useOverviewMode = false;
    void timerCallback() override;
    void updateOverview();

    // Interaction mode
    InteractionMode interactionMode = InteractionMode::Selection;

    // Brush settings
    float brushRadius = 20.0f;      // Radius in pixels
    float brushStrength = 0.5f;     // Strength 0.0-1.0
    float brushFalloff = 0.5f;      // Falloff 0.0=hard, 1.0=soft
    bool brushActive = false;
    std::vector<BrushPoint> currentBrushStroke;
    juce::Point<float> lastBrushPosition;
    juce::Point<float> currentMousePosition;
    bool showBrushCursor = false;

    // Editor integration
    SpectralEditor* spectralEditor = nullptr;
    bool showEditOverlay = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramView)
};

} // namespace spectralz
