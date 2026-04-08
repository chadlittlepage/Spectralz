#pragma once

#include "PluginProcessor.h"
#include "../Core/AudioEngine.h"
#include "../Core/ProjectState.h"
#include "../DSP/FFTProcessor.h"
#include "../DSP/SpectralEditor.h"
#include "../UI/Components/SpectrogramView.h"
#include "../UI/Components/WaveformView.h"
#include "../UI/Components/TransportControls.h"
#include "../UI/Components/SettingsPanel.h"
#include "../UI/Components/VUMeter.h"
#include "../UI/Components/ToolPanel.h"
#include "../UI/Components/LayerPanel.h"
#include "../UI/SpectralzLookAndFeel.h"
#if SPECTRALZ_ENABLE_ML
#include "../UI/Components/StemSeparationPanel.h"
#include "../ML/StemSeparationManager.h"
#endif
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class SpectralzAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::Timer,
                                       public juce::FileDragAndDropTarget,
                                       public juce::MenuBarModel,
                                       public juce::KeyListener
{
public:
    explicit SpectralzAudioProcessorEditor(SpectralzAudioProcessor&);
    ~SpectralzAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Timer for playhead updates
    void timerCallback() override;

    // Drag and drop support
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // MenuBarModel overrides
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    // KeyListener overrides
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

    // Component override for focus
    void parentHierarchyChanged() override;


    // Audio settings callback (set by standalone app)
    std::function<void()> onShowAudioSettings;

private:
    // Inner class for resize divider between spectrogram and waveform
    class ResizeDivider : public juce::Component
    {
    public:
        std::function<void(int)> onDragEnd;  // Called with final height on release

        void setCurrentHeight(int h) { currentHeight = h; }

        ResizeDivider() { setMouseCursor(juce::MouseCursor::UpDownResizeCursor); }

        void paint(juce::Graphics& g) override
        {
            // Always draw the actual divider line
            g.setColour(juce::Colour(0x40ffffff));
            g.fillRect(0, getHeight() / 2 - 1, getWidth(), 2);

            // During drag, draw preview line at new position
            if (isDragging && previewOffset != 0)
            {
                g.setColour(juce::Colour(0xff4a9eff));
                g.fillRect(0, getHeight() / 2 - 1 - previewOffset, getWidth(), 3);
            }
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            isDragging = true;
            dragStartY = e.y;
            startHeight = currentHeight;
            previewOffset = 0;
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            int delta = dragStartY - e.y;
            int newHeight = juce::jlimit(40, 300, startHeight + delta);
            previewOffset = newHeight - startHeight;
            repaint();  // Just repaint the small divider, not the whole UI
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            isDragging = false;
            int finalHeight = juce::jlimit(40, 300, startHeight + previewOffset);
            previewOffset = 0;
            currentHeight = finalHeight;
            if (onDragEnd) onDragEnd(finalHeight);
            repaint();
        }

    private:
        int dragStartY = 0;
        int startHeight = 80;
        int currentHeight = 80;
        int previewOffset = 0;
        bool isDragging = false;
    };

    void loadAudioFile(const juce::File& file);
    void saveAudioFile();
    void updateSpectrogram();
    void setupCallbacks();
    void setupMenuBar();
    void setupToolCallbacks();
    void setupLayerCallbacks();
    void showAudioSettings();
    void handleBrushStroke(const std::vector<spectralz::BrushPoint>& points,
                           spectralz::InteractionMode mode);

    SpectralzAudioProcessor& processorRef;

    // Platform-aware look and feel
    spectralz::SpectralzLookAndFeel lookAndFeel;

    // Audio engine for file I/O and playback
    spectralz::AudioEngine audioEngine;

    // DSP processor
    spectralz::FFTProcessor fftProcessor;

    // UI Components
    spectralz::SpectrogramView spectrogramView;
    spectralz::WaveformView waveformView;
    spectralz::VUMeter vuMeter;
    spectralz::TransportControls transportControls;
    spectralz::SettingsPanel settingsPanel;

    // Phase 2: Editing UI
    spectralz::ToolPanel toolPanel;
    spectralz::LayerPanel layerPanel;

    // Project state (owns SpectralEditor)
    spectralz::ProjectState projectState;

    // Menu bar (for macOS native menu or in-window on Windows)
    std::unique_ptr<juce::MenuBarComponent> menuBar;

    // File chooser for async file dialogs
    std::unique_ptr<juce::FileChooser> fileChooser;

    // State
    bool spectrogramNeedsUpdate = false;
    juce::File currentFile;

    // Resizable waveform divider
    ResizeDivider waveformDivider;
    int waveformHeight = 80;

    // Panel visibility state (start hidden, user can show via View menu)
    bool toolPanelVisible = false;
    bool layerPanelVisible = false;

#if SPECTRALZ_ENABLE_ML
    // Phase 3: ML Stem Separation
    spectralz::StemSeparationManager stemSeparationManager;
    spectralz::StemSeparationPanel stemSeparationPanel;
    bool stemSeparationPanelVisible = false;
    juce::File stemOutputDirectory;  // Where to save separated stems

    void setupStemSeparationCallbacks();
    void handleStemSeparationComplete(const spectralz::SeparationResult& result);
    void saveStemToFile(const std::string& stemName, const juce::AudioBuffer<float>& buffer, double sampleRate);
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralzAudioProcessorEditor)
};
