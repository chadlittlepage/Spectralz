#include "PluginEditor.h"
#include "PluginProcessor.h"

SpectralzAudioProcessorEditor::SpectralzAudioProcessorEditor(SpectralzAudioProcessor& p)
    : AudioProcessorEditor(&p)
    , processorRef(p)
    , fftProcessor(11) // 2048-point FFT
{
    // Apply platform-aware look and feel
    setLookAndFeel(&lookAndFeel);

    // Primary color from CLAUDE.md: #4a556c
    setSize(1200, 800);
    setResizable(true, true);
    setResizeLimits(800, 600, 2560, 1440);

    // Setup audio engine
    audioEngine.setupAudioDevice();

    // Add components
    addAndMakeVisible(spectrogramView);
    addAndMakeVisible(waveformView);
    addAndMakeVisible(vuMeter);
    addAndMakeVisible(transportControls);
    addChildComponent(settingsPanel); // Hidden by default

    // Phase 2: Add editing UI (hidden by default, toggle via View menu)
    addChildComponent(toolPanel);
    addChildComponent(layerPanel);

#if SPECTRALZ_ENABLE_ML
    // Phase 3: Add stem separation panel (hidden by default)
    addChildComponent(stemSeparationPanel);
    stemSeparationPanel.setSeparationManager(&stemSeparationManager);
#endif

    // Connect SpectrogramView to SpectralEditor
    spectrogramView.setSpectralEditor(&projectState.getEditor());
    layerPanel.setEditor(&projectState.getEditor());

    // Setup menu bar
    setupMenuBar();

    // Setup callbacks
    setupCallbacks();
    setupToolCallbacks();
    setupLayerCallbacks();
#if SPECTRALZ_ENABLE_ML
    setupStemSeparationCallbacks();
#endif

    // Initial state
    transportControls.setFileLoaded(false);

    // Apply initial tool settings to spectrogram view
    const auto& initialSettings = toolPanel.getSettings();
    spectrogramView.setBrushRadius(initialSettings.brushRadius);
    spectrogramView.setBrushStrength(initialSettings.brushStrength);
    spectrogramView.setBrushFalloff(initialSettings.brushFalloff);

    // Register key listener for shortcuts
    addKeyListener(this);
    setWantsKeyboardFocus(true);

    // Setup waveform divider for resizing (preview line during drag, resize on release)
    addAndMakeVisible(waveformDivider);
    waveformDivider.setCurrentHeight(waveformHeight);
    waveformDivider.onDragEnd = [this](int newHeight)
    {
        waveformHeight = newHeight;
        resized();
    };

    // Start timer for playhead updates (20Hz is smooth enough)
    startTimerHz(20);
}

SpectralzAudioProcessorEditor::~SpectralzAudioProcessorEditor()
{
    stopTimer();

#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(nullptr);
#endif

    setLookAndFeel(nullptr);
}

void SpectralzAudioProcessorEditor::setupCallbacks()
{
    // Transport controls callbacks
    transportControls.onPlay = [this]()
    {
        audioEngine.play();
    };

    transportControls.onPause = [this]()
    {
        audioEngine.pause();
    };

    transportControls.onStop = [this]()
    {
        audioEngine.stop();
    };

    transportControls.onSkipBackward = [this]()
    {
        double newPos = std::max(0.0, audioEngine.getCurrentPosition() - 5.0);
        audioEngine.setPosition(newPos);
    };

    transportControls.onSkipForward = [this]()
    {
        double newPos = std::min(audioEngine.getTotalLength(),
                                 audioEngine.getCurrentPosition() + 5.0);
        audioEngine.setPosition(newPos);
    };

    transportControls.onLoopToggled = [this](bool shouldLoop)
    {
        audioEngine.setLooping(shouldLoop);
    };

    transportControls.onPositionChanged = [this](double position)
    {
        audioEngine.setStartPosition(position);  // Remember this position for stop
        audioEngine.setPosition(position);
    };

    transportControls.onOpenFile = [this]()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select an audio file...",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg");

        auto chooserFlags = juce::FileBrowserComponent::openMode |
                           juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                loadAudioFile(file);
            }
        });
    };

    transportControls.onSaveFile = [this]()
    {
        saveAudioFile();
    };

    // Audio engine callbacks
    audioEngine.onFileLoaded = [this]()
    {
        transportControls.setFileLoaded(true);
        transportControls.setTotalDuration(audioEngine.getTotalLength());
        // Use audio buffer for RMS+Peak display instead of thumbnail
        waveformView.setAudioBuffer(audioEngine.getAudioBuffer(), audioEngine.getSampleRate());
        spectrogramNeedsUpdate = true;
        // Grab keyboard focus so spacebar works immediately
        grabKeyboardFocus();
    };

    audioEngine.onPlaybackStarted = [this]()
    {
        transportControls.setPlaying(true);
    };

    audioEngine.onPlaybackStopped = [this]()
    {
        transportControls.setPlaying(false);
    };

    // Spectrogram view callbacks
    spectrogramView.onTimeClicked = [this](double time)
    {
        audioEngine.setStartPosition(time);  // Remember this position for stop
        audioEngine.setPosition(time);
    };

    spectrogramView.onSelectionChanged = [this](double startTime, double endTime, float, float)
    {
        waveformView.setSelection(startTime, endTime);
        // Set loop region for selection-based looping
        if (startTime != endTime)
        {
            double loopStart = std::min(startTime, endTime);
            double loopEnd = std::max(startTime, endTime);
            audioEngine.setLoopRegion(loopStart, loopEnd);
            // Auto-zoom spectrogram to selection (waveform stays global)
            spectrogramView.setVisibleTimeRange(loopStart, loopEnd);
        }
        else
        {
            audioEngine.clearLoopRegion();
        }
    };

    // Waveform view callbacks
    waveformView.onTimeClicked = [this](double time)
    {
        audioEngine.setStartPosition(time);  // Remember this position for stop
        audioEngine.setPosition(time);
    };

    waveformView.onViewRangeChanged = [this](double startTime, double endTime)
    {
        spectrogramView.setVisibleTimeRange(startTime, endTime);
    };

    waveformView.onSelectionChanged = [this](double startTime, double endTime)
    {
        // Set loop region for selection-based looping
        if (startTime != endTime)
        {
            double loopStart = std::min(startTime, endTime);
            double loopEnd = std::max(startTime, endTime);
            audioEngine.setLoopRegion(loopStart, loopEnd);
            // Auto-zoom spectrogram to selection (waveform stays global for navigation)
            spectrogramView.setVisibleTimeRange(loopStart, loopEnd);
        }
        else
        {
            audioEngine.clearLoopRegion();
        }
    };

    // Settings panel callbacks
    settingsPanel.onSpectrogramColorMapChanged = [this](spectralz::SpectrogramColorMap colorMap)
    {
        spectrogramView.setColorMap(colorMap);
    };

    settingsPanel.onSpectrogramBrightnessChanged = [this](float brightness)
    {
        spectrogramView.setBrightness(brightness);
    };

    settingsPanel.onSpectrogramContrastChanged = [this](float contrast)
    {
        spectrogramView.setContrast(contrast);
    };

    settingsPanel.onWaveformStyleChanged = [this](spectralz::WaveformStyle style)
    {
        waveformView.setWaveformStyle(style);
    };

    settingsPanel.onWaveformColorSchemeChanged = [this](spectralz::WaveformColorScheme scheme)
    {
        waveformView.setColorScheme(scheme);
    };

    // Brush stroke callback for spectrogram editing
    spectrogramView.onBrushStroke = [this](const std::vector<spectralz::BrushPoint>& points,
                                            spectralz::InteractionMode mode)
    {
        handleBrushStroke(points, mode);
    };
}

void SpectralzAudioProcessorEditor::setupToolCallbacks()
{
    // Tool selection callback
    toolPanel.onToolChanged = [this](spectralz::ToolType tool)
    {
        // Map tool types to interaction modes
        spectralz::InteractionMode mode = spectralz::InteractionMode::Selection;

        switch (tool)
        {
            case spectralz::ToolType::Select:
                mode = spectralz::InteractionMode::Selection;
                break;
            case spectralz::ToolType::FrequencySelect:
                mode = spectralz::InteractionMode::FrequencySelect;
                break;
            case spectralz::ToolType::TimeSelect:
                mode = spectralz::InteractionMode::TimeSelect;
                break;
            case spectralz::ToolType::BrushBoost:
                mode = spectralz::InteractionMode::BrushBoost;
                break;
            case spectralz::ToolType::BrushAttenuate:
                mode = spectralz::InteractionMode::BrushAttenuate;
                break;
            case spectralz::ToolType::Eraser:
                mode = spectralz::InteractionMode::Eraser;
                break;
        }

        spectrogramView.setInteractionMode(mode);
    };

    // Tool settings callback - updates brush settings when changed in panel
    toolPanel.onSettingsChanged = [this](const spectralz::ToolSettings& settings)
    {
        spectrogramView.setBrushRadius(settings.brushRadius);
        spectrogramView.setBrushStrength(settings.brushStrength);
        spectrogramView.setBrushFalloff(settings.brushFalloff);
    };
}

void SpectralzAudioProcessorEditor::setupLayerCallbacks()
{
    // Layer selection
    layerPanel.onLayerSelected = [this](int index)
    {
        projectState.getEditor().setActiveLayer(index);
    };

    // Layer add
    layerPanel.onAddLayer = [this]()
    {
        projectState.getEditor().addLayer("Layer " +
            std::to_string(projectState.getEditor().getNumLayers() + 1));
        layerPanel.refresh();
    };

    // Layer remove
    layerPanel.onRemoveLayer = [this](int index)
    {
        projectState.getEditor().removeLayer(index);
        layerPanel.refresh();
    };

    // Visibility/mute/solo changes
    layerPanel.onLayerVisibilityChanged = [this](int, bool)
    {
        spectrogramView.refreshEditOverlay();
    };

    layerPanel.onLayerMuteChanged = [this](int, bool)
    {
        spectrogramView.refreshEditOverlay();
    };

    layerPanel.onLayerSoloChanged = [this](int, bool)
    {
        spectrogramView.refreshEditOverlay();
    };
}

void SpectralzAudioProcessorEditor::handleBrushStroke(
    const std::vector<spectralz::BrushPoint>& points,
    spectralz::InteractionMode mode)
{
    if (points.empty())
        return;

    auto& editor = projectState.getEditor();
    const auto& toolSettings = toolPanel.getSettings();

    // Validate mode is a brush mode
    if (mode != spectralz::InteractionMode::BrushBoost &&
        mode != spectralz::InteractionMode::BrushAttenuate &&
        mode != spectralz::InteractionMode::Eraser)
    {
        return;  // Not a brush mode
    }

    // Get actual FFT parameters from the source data
    if (!editor.hasSourceData())
        return;

    const auto* sourceData = editor.getSourceData();

    // Validate source data has valid dimensions
    if (sourceData->numFrames <= 0 || sourceData->fftSize <= 0 || sourceData->hopSize <= 0)
        return;

    double sampleRate = sourceData->sampleRate;
    int hopSize = sourceData->hopSize;
    int fftSize = static_cast<int>(sourceData->fftSize);
    int maxFrame = sourceData->numFrames - 1;
    int maxBin = fftSize / 2;

    spectralz::BrushStroke stroke;
    stroke.radius = toolSettings.brushRadius;
    stroke.strength = toolSettings.brushStrength;
    stroke.falloff = toolSettings.brushFalloff;
    stroke.points.reserve(points.size());

    for (const auto& pt : points)
    {
        // Convert time to frame
        int frame = static_cast<int>(pt.time * sampleRate / hopSize);

        // Convert frequency to bin (linear mapping)
        int bin = static_cast<int>(pt.frequency * fftSize / sampleRate);

        // Clamp to valid range (ensure bounds are valid before clamping)
        frame = std::max(0, std::min(frame, maxFrame));
        bin = std::max(0, std::min(bin, maxBin));

        stroke.points.emplace_back(frame, bin);
    }

    // Apply the edit using the appropriate SpectralEditor method
    if (mode == spectralz::InteractionMode::BrushBoost)
    {
        editor.applyBrushGain(stroke, toolSettings.gainAmount);
    }
    else if (mode == spectralz::InteractionMode::BrushAttenuate)
    {
        editor.applyBrushGain(stroke, -toolSettings.gainAmount);
    }
    else if (mode == spectralz::InteractionMode::Eraser)
    {
        editor.brushErase(stroke, toolSettings.eraseFloor);
    }

    // Mark that we need to resynthesize (defer until playback)
    spectrogramNeedsUpdate = true;

    // Refresh the view
    spectrogramView.refreshEditOverlay();
}

void SpectralzAudioProcessorEditor::loadAudioFile(const juce::File& file)
{
    if (audioEngine.loadFile(file))
    {
        currentFile = file;
        updateSpectrogram();

        // Update window title
        if (auto* window = getTopLevelComponent())
        {
            window->setName("Spectralz - " + file.getFileName());
        }
    }
}

void SpectralzAudioProcessorEditor::saveAudioFile()
{
    if (!audioEngine.hasLoadedFile())
        return;

    fileChooser = std::make_unique<juce::FileChooser>(
        "Save audio file...",
        currentFile.existsAsFile() ? currentFile.getParentDirectory()
                                   : juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.wav;*.aiff;*.flac");

    auto chooserFlags = juce::FileBrowserComponent::saveMode |
                       juce::FileBrowserComponent::canSelectFiles |
                       juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File{})
        {
            audioEngine.saveFile(file);
        }
    });
}

void SpectralzAudioProcessorEditor::updateSpectrogram()
{
    if (!audioEngine.hasLoadedFile())
        return;

    auto* buffer = audioEngine.getAudioBuffer();
    if (buffer == nullptr)
        return;

    // Compute spectrogram in background thread
    juce::Thread::launch([this, buffer, sampleRate = audioEngine.getSampleRate()]()
    {
        auto spectrogramData = fftProcessor.computeSpectrogram(*buffer, 0, sampleRate);

        juce::MessageManager::callAsync([this, data = std::move(spectrogramData)]()
        {
            spectrogramView.setSpectrogramData(data);

            // Set source data on the SpectralEditor for editing
            // Note: The data is now owned by spectrogramView, we need a copy
            // For now, we'll get it from the view
            projectState.getEditor().setSourceSpectrogram(data);

            // Enable the edit overlay so edits are visible
            spectrogramView.setShowEditOverlay(true);
        });
    });
}

void SpectralzAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff1a1a1a));

#if !JUCE_MAC
    // On Windows/Linux, draw an in-window title bar below the menu
    auto titleBounds = getLocalBounds();
    if (menuBar != nullptr)
        titleBounds.removeFromTop(24); // Menu bar height
    titleBounds = titleBounds.removeFromTop(30);

    g.setColour(juce::Colour(0xff4a4a4a));
    g.fillRect(titleBounds);

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("Spectralz", titleBounds, juce::Justification::centred, true);
#endif

    // Draw drop zone hint if no file loaded
    if (!audioEngine.hasLoadedFile())
    {
        auto dropZone = getLocalBounds().reduced(100);
        g.setColour(juce::Colour(0x40ffffff));
        g.drawRoundedRectangle(dropZone.toFloat(), 10.0f, 2.0f);

        g.setColour(juce::Colour(0x80ffffff));
        g.setFont(24.0f);
        g.drawText("Drop audio file here or use File > Open",
                   dropZone, juce::Justification::centred, true);
    }
}

void SpectralzAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

#if JUCE_MAC
    // macOS: Use native menu bar (no in-window menu)
    // The window already has native title bar with traffic lights
#else
    // Windows/Linux: In-window menu bar
    if (menuBar != nullptr)
    {
        menuBar->setBounds(bounds.removeFromTop(24));
    }
    // Title bar below menu
    bounds.removeFromTop(30);
#endif

    // Transport controls at bottom
    transportControls.setBounds(bounds.removeFromBottom(50));

    // VU meters above transport
    vuMeter.setBounds(bounds.removeFromBottom(36));

    // Waveform overview at bottom (above VU meters) - resizable
    waveformView.setBounds(bounds.removeFromBottom(waveformHeight));

    // Position the resize divider at the top edge of waveform
    auto waveformBounds = waveformView.getBounds();
    waveformDivider.setBounds(waveformBounds.getX(), waveformBounds.getY() - 4,
                               waveformBounds.getWidth(), 8);

    // Tool panel on the left side (if visible)
    if (toolPanelVisible)
    {
        toolPanel.setVisible(true);
        toolPanel.setBounds(bounds.removeFromLeft(180));
    }
    else
    {
        toolPanel.setVisible(false);
    }

    // Right side: settings panel takes priority, then stem separation, then layers
    if (settingsPanel.isVisible())
    {
        // Settings panel takes the right side when visible
        settingsPanel.setBounds(bounds.removeFromRight(260));
    }
#if SPECTRALZ_ENABLE_ML
    else if (stemSeparationPanelVisible)
    {
        // Stem separation panel on the right
        stemSeparationPanel.setVisible(true);
        stemSeparationPanel.setBounds(bounds.removeFromRight(280));
    }
#endif
    else if (layerPanelVisible)
    {
        // Layer panel on the right side when settings is hidden and layers enabled
        layerPanel.setVisible(true);
        layerPanel.setBounds(bounds.removeFromRight(180));
    }
    else
    {
        layerPanel.setVisible(false);
#if SPECTRALZ_ENABLE_ML
        stemSeparationPanel.setVisible(false);
#endif
    }

    // Spectrogram takes remaining space (center area)
    spectrogramView.setBounds(bounds);
}

void SpectralzAudioProcessorEditor::timerCallback()
{
    if (audioEngine.hasLoadedFile())
    {
        double pos = audioEngine.getCurrentPosition();
        transportControls.setPosition(pos);
        spectrogramView.setPlayheadPosition(pos);
        waveformView.setPlayheadPosition(pos);

        // Update VU meters with current levels
        vuMeter.setLevels(audioEngine.getLeftLevel(), audioEngine.getRightLevel());
    }

    if (spectrogramNeedsUpdate)
    {
        spectrogramNeedsUpdate = false;
        updateSpectrogram();
    }
}

bool SpectralzAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& file : files)
    {
        if (file.endsWithIgnoreCase(".wav") ||
            file.endsWithIgnoreCase(".aiff") ||
            file.endsWithIgnoreCase(".aif") ||
            file.endsWithIgnoreCase(".mp3") ||
            file.endsWithIgnoreCase(".flac") ||
            file.endsWithIgnoreCase(".ogg"))
        {
            return true;
        }
    }
    return false;
}

void SpectralzAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    for (const auto& filePath : files)
    {
        juce::File file(filePath);
        if (file.existsAsFile())
        {
            loadAudioFile(file);
            break; // Only load the first file
        }
    }
}

// Menu bar implementation
void SpectralzAudioProcessorEditor::setupMenuBar()
{
#if JUCE_MAC
    // On macOS, set this as the model for the native menu bar
    // Add extra items to the app menu (Spectralz menu)
    juce::PopupMenu extraAppleMenuItems;
    extraAppleMenuItems.addItem(400, "Audio/MIDI Settings...");
    extraAppleMenuItems.addSeparator();

    juce::MenuBarModel::setMacMainMenu(this, &extraAppleMenuItems);
#else
    // On Windows/Linux, create an in-window menu bar
    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(menuBar.get());
#endif
}

juce::StringArray SpectralzAudioProcessorEditor::getMenuBarNames()
{
#if JUCE_MAC
    return {"File", "Edit", "View"};  // Help goes in app menu on macOS
#else
    return {"File", "Edit", "View", "Help"};
#endif
}

juce::PopupMenu SpectralzAudioProcessorEditor::getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName)
{
    juce::PopupMenu menu;

    if (menuName == "File")
    {
        menu.addItem(1, "Open...", true, false);
        menu.addItem(2, "Save As...", audioEngine.hasLoadedFile(), false);
        menu.addSeparator();
#if !JUCE_MAC
        menu.addItem(10, "Exit");
#endif
    }
    else if (menuName == "Edit")
    {
        auto& editor = projectState.getEditor();
        menu.addItem(50, "Undo " + juce::String(editor.getUndoDescription()),
                     editor.canUndo(), false);
        menu.addItem(51, "Redo " + juce::String(editor.getRedoDescription()),
                     editor.canRedo(), false);
        menu.addSeparator();
        menu.addItem(52, "Cut", spectrogramView.hasSelection(), false);
        menu.addItem(53, "Copy", spectrogramView.hasSelection(), false);
        menu.addItem(54, "Paste", editor.hasClipboardData(), false);
        menu.addSeparator();
        menu.addItem(55, "Delete Selection", spectrogramView.hasSelection(), false);
        menu.addItem(56, "Select All", audioEngine.hasLoadedFile(), false);
    }
    else if (menuName == "View")
    {
        menu.addItem(20, "Options", true, settingsPanel.isVisible());
        menu.addItem(21, "Tools", true, toolPanelVisible);
        menu.addItem(22, "Layers", true, layerPanelVisible);
#if SPECTRALZ_ENABLE_ML
        menu.addItem(26, "Stem Separation", true, stemSeparationPanelVisible);
#endif
        menu.addSeparator();

        // Zoom controls
        menu.addItem(23, "Zoom In\t\tCmd+=", audioEngine.hasLoadedFile());
        menu.addItem(24, "Zoom Out\t\tCmd+-", audioEngine.hasLoadedFile());
        menu.addItem(25, "Zoom to Fit\t\tCmd+0", audioEngine.hasLoadedFile());
        menu.addSeparator();

        // Spectrogram color maps submenu
        juce::PopupMenu colorMapMenu;
        colorMapMenu.addItem(100, "Orange+Blue");
        colorMapMenu.addItem(101, "Orange");
        colorMapMenu.addItem(102, "Viridis");
        colorMapMenu.addItem(103, "Magma");
        colorMapMenu.addItem(104, "Inferno");
        colorMapMenu.addItem(105, "Plasma");
        colorMapMenu.addItem(106, "Grayscale");
        colorMapMenu.addItem(107, "Thermal");
        colorMapMenu.addItem(108, "Ocean");
        colorMapMenu.addItem(109, "Sunset");
        menu.addSubMenu("Spectrogram Color", colorMapMenu);

        // Waveform style submenu
        juce::PopupMenu waveformMenu;
        waveformMenu.addItem(200, "Classic");
        waveformMenu.addItem(201, "RMS + Peak");
        waveformMenu.addItem(202, "Gradient");
        waveformMenu.addItem(203, "Stereo");
        waveformMenu.addItem(204, "Rainbow");
        menu.addSubMenu("Waveform Style", waveformMenu);
    }
    else if (menuName == "Help")
    {
        menu.addItem(300, "About Spectralz");
    }

    return menu;
}

void SpectralzAudioProcessorEditor::menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/)
{
    switch (menuItemID)
    {
        case 1: // Open
            transportControls.onOpenFile();
            break;

        case 2: // Save As
            saveAudioFile();
            break;

        case 10: // Exit (Windows only)
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            break;

        case 20: // Toggle Options Panel
            settingsPanel.toggleVisibility();
            resized();
            menuItemsChanged();
            break;

        case 21: // Toggle Tools Panel
            toolPanelVisible = !toolPanelVisible;
            toolPanel.setVisible(toolPanelVisible);
            resized();
            menuItemsChanged();
            break;

        case 22: // Toggle Layers Panel
            layerPanelVisible = !layerPanelVisible;
            layerPanel.setVisible(layerPanelVisible);
            resized();
            menuItemsChanged();
            break;

#if SPECTRALZ_ENABLE_ML
        case 26: // Toggle Stem Separation Panel
            stemSeparationPanelVisible = !stemSeparationPanelVisible;
            stemSeparationPanel.setVisible(stemSeparationPanelVisible);
            resized();
            menuItemsChanged();
            break;
#endif

        case 23: // Zoom In
            spectrogramView.zoomIn();
            break;

        case 24: // Zoom Out
            spectrogramView.zoomOut();
            break;

        case 25: // Zoom to Fit (100%)
            spectrogramView.resetZoom();
            break;

        case 400: // Audio/MIDI Settings
            showAudioSettings();
            break;

        // Spectrogram color maps (100-109)
        case 100: case 101: case 102: case 103: case 104:
        case 105: case 106: case 107: case 108: case 109:
        {
            auto colorMap = static_cast<spectralz::SpectrogramColorMap>(menuItemID - 100);
            spectrogramView.setColorMap(colorMap);
            break;
        }

        // Waveform styles (200-204)
        case 200: case 201: case 202: case 203: case 204:
        {
            auto style = static_cast<spectralz::WaveformStyle>(menuItemID - 200);
            waveformView.setWaveformStyle(style);
            break;
        }

        case 300: // About
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "About Spectralz",
                "Spectralz - AI-Powered Spectral Audio Editor\n\n"
                "Version 0.1.0\n\n"
                "A professional spectral editing application inspired by\n"
                "SpectraLayers and iZotope RX.",
                "OK");
            break;
        }

        // Edit menu
        case 50: // Undo
            projectState.getEditor().undo();
            spectrogramView.refreshEditOverlay();
            break;

        case 51: // Redo
            projectState.getEditor().redo();
            spectrogramView.refreshEditOverlay();
            break;

        case 52: // Cut
        {
            auto& editor = projectState.getEditor();
            if (spectrogramView.hasSelection() && editor.hasSourceData())
            {
                double startTime, endTime;
                float startFreq, endFreq;
                spectrogramView.getSelection(startTime, endTime, startFreq, endFreq);

                // Convert to frame/bin coordinates
                const auto* sourceData = editor.getSourceData();
                double sampleRate = sourceData->sampleRate;
                int hopSize = sourceData->hopSize;
                int fftSize = static_cast<int>(sourceData->fftSize);

                spectralz::SpectralRegion region;
                region.startFrame = static_cast<int>(startTime * sampleRate / hopSize);
                region.endFrame = static_cast<int>(endTime * sampleRate / hopSize);
                region.startBin = static_cast<int>(startFreq * fftSize / sampleRate);
                region.endBin = static_cast<int>(endFreq * fftSize / sampleRate);

                // Validate source data dimensions
                if (sourceData->numFrames <= 0 || fftSize <= 0)
                    break;

                // Clamp to valid range
                region.startFrame = std::clamp(region.startFrame, 0, sourceData->numFrames - 1);
                region.endFrame = std::clamp(region.endFrame, 1, sourceData->numFrames);
                region.startBin = std::clamp(region.startBin, 0, fftSize / 2);
                region.endBin = std::clamp(region.endBin, 1, fftSize / 2 + 1);

                // Cut = copy + erase
                editor.cutRegion(region);
                spectrogramView.refreshEditOverlay();
            }
            break;
        }

        case 53: // Copy
        {
            auto& editor = projectState.getEditor();
            if (spectrogramView.hasSelection() && editor.hasSourceData())
            {
                double startTime, endTime;
                float startFreq, endFreq;
                spectrogramView.getSelection(startTime, endTime, startFreq, endFreq);

                // Convert to frame/bin coordinates
                const auto* sourceData = editor.getSourceData();
                double sampleRate = sourceData->sampleRate;
                int hopSize = sourceData->hopSize;
                int fftSize = static_cast<int>(sourceData->fftSize);

                // Validate source data dimensions
                if (sourceData->numFrames <= 0 || fftSize <= 0)
                    break;

                spectralz::SpectralRegion region;
                region.startFrame = static_cast<int>(startTime * sampleRate / hopSize);
                region.endFrame = static_cast<int>(endTime * sampleRate / hopSize);
                region.startBin = static_cast<int>(startFreq * fftSize / sampleRate);
                region.endBin = static_cast<int>(endFreq * fftSize / sampleRate);

                // Clamp to valid range
                region.startFrame = std::clamp(region.startFrame, 0, sourceData->numFrames - 1);
                region.endFrame = std::clamp(region.endFrame, 1, sourceData->numFrames);
                region.startBin = std::clamp(region.startBin, 0, fftSize / 2);
                region.endBin = std::clamp(region.endBin, 1, fftSize / 2 + 1);

                editor.copyRegion(region);
            }
            break;
        }

        case 54: // Paste
        {
            auto& editor = projectState.getEditor();
            if (editor.hasClipboardData() && editor.hasSourceData())
            {
                // Paste at selection start or playhead position
                int destFrame = 0;
                int destBin = 0;

                if (spectrogramView.hasSelection())
                {
                    double startTime, endTime;
                    float startFreq, endFreq;
                    spectrogramView.getSelection(startTime, endTime, startFreq, endFreq);

                    const auto* sourceData = editor.getSourceData();
                    double sampleRate = sourceData->sampleRate;
                    int hopSize = sourceData->hopSize;
                    int fftSize = static_cast<int>(sourceData->fftSize);

                    destFrame = static_cast<int>(startTime * sampleRate / hopSize);
                    destBin = static_cast<int>(startFreq * fftSize / sampleRate);
                }
                else
                {
                    // Use playhead position
                    const auto* sourceData = editor.getSourceData();
                    double sampleRate = sourceData->sampleRate;
                    int hopSize = sourceData->hopSize;
                    destFrame = static_cast<int>(audioEngine.getCurrentPosition() * sampleRate / hopSize);
                    destBin = 0;
                }

                editor.paste(destFrame, destBin);
                spectrogramView.refreshEditOverlay();
            }
            break;
        }

        case 55: // Delete Selection
        {
            auto& editor = projectState.getEditor();
            if (spectrogramView.hasSelection() && editor.hasSourceData())
            {
                double startTime, endTime;
                float startFreq, endFreq;
                spectrogramView.getSelection(startTime, endTime, startFreq, endFreq);

                // Convert to frame/bin coordinates
                const auto* sourceData = editor.getSourceData();
                double sampleRate = sourceData->sampleRate;
                int hopSize = sourceData->hopSize;
                int fftSize = static_cast<int>(sourceData->fftSize);

                // Validate source data dimensions
                if (sourceData->numFrames <= 0 || fftSize <= 0)
                    break;

                spectralz::SpectralRegion region;
                region.startFrame = static_cast<int>(startTime * sampleRate / hopSize);
                region.endFrame = static_cast<int>(endTime * sampleRate / hopSize);
                region.startBin = static_cast<int>(startFreq * fftSize / sampleRate);
                region.endBin = static_cast<int>(endFreq * fftSize / sampleRate);

                // Clamp to valid range
                region.startFrame = std::clamp(region.startFrame, 0, sourceData->numFrames - 1);
                region.endFrame = std::clamp(region.endFrame, 1, sourceData->numFrames);
                region.startBin = std::clamp(region.startBin, 0, fftSize / 2);
                region.endBin = std::clamp(region.endBin, 1, fftSize / 2 + 1);

                // Erase the region
                editor.erase(region, toolPanel.getSettings().eraseFloor);
                spectrogramView.refreshEditOverlay();
            }
            break;
        }

        case 56: // Select All
        {
            auto& editor = projectState.getEditor();
            if (editor.hasSourceData())
            {
                // Select entire spectrogram - handled by SpectrogramView
                // Would need to add setSelection method to SpectrogramView
            }
            break;
        }

        default:
            break;
    }
}

void SpectralzAudioProcessorEditor::parentHierarchyChanged()
{
    // Grab keyboard focus when added to hierarchy (e.g., when window opens)
    if (getParentComponent() != nullptr)
    {
        // Use a timer to grab focus after a short delay to ensure window is ready
        juce::Timer::callAfterDelay(100, [this]() {
            if (isShowing())
                grabKeyboardFocus();
        });
    }
}

bool SpectralzAudioProcessorEditor::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    // Tool shortcuts
    if (key == juce::KeyPress('v') || key == juce::KeyPress('V'))
    {
        toolPanel.setCurrentTool(spectralz::ToolType::Select);
        return true;
    }
    if (key == juce::KeyPress('f') || key == juce::KeyPress('F'))
    {
        toolPanel.setCurrentTool(spectralz::ToolType::FrequencySelect);
        return true;
    }
    if (key == juce::KeyPress('t') || key == juce::KeyPress('T'))
    {
        toolPanel.setCurrentTool(spectralz::ToolType::TimeSelect);
        return true;
    }
    if (key == juce::KeyPress('b') || key == juce::KeyPress('B'))
    {
        toolPanel.setCurrentTool(spectralz::ToolType::BrushBoost);
        return true;
    }
    if (key == juce::KeyPress('n') || key == juce::KeyPress('N'))
    {
        toolPanel.setCurrentTool(spectralz::ToolType::BrushAttenuate);
        return true;
    }
    if (key == juce::KeyPress('e') || key == juce::KeyPress('E'))
    {
        toolPanel.setCurrentTool(spectralz::ToolType::Eraser);
        return true;
    }

    // Undo/Redo
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0))
    {
        if (projectState.getEditor().canUndo())
        {
            projectState.getEditor().undo();
            spectrogramView.refreshEditOverlay();
        }
        return true;
    }
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        if (projectState.getEditor().canRedo())
        {
            projectState.getEditor().redo();
            spectrogramView.refreshEditOverlay();
        }
        return true;
    }

    // Delete selection
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (spectrogramView.hasSelection())
        {
            menuItemSelected(55, 0);  // Delete Selection
        }
        return true;
    }

    // Brush size shortcuts
    if (key == juce::KeyPress('['))
    {
        float newRadius = std::max(5.0f, spectrogramView.getBrushRadius() - 5.0f);
        spectrogramView.setBrushRadius(newRadius);
        return true;
    }
    if (key == juce::KeyPress(']'))
    {
        float newRadius = std::min(100.0f, spectrogramView.getBrushRadius() + 5.0f);
        spectrogramView.setBrushRadius(newRadius);
        return true;
    }

    // Space for play/stop
    if (key == juce::KeyPress::spaceKey)
    {
        if (audioEngine.isPlaying())
            audioEngine.stop();
        else
            audioEngine.play();
        return true;
    }

    // Zoom shortcuts (Cmd + =, Cmd + -, Cmd + 0)
    if (key == juce::KeyPress('=', juce::ModifierKeys::commandModifier, 0) ||
        key == juce::KeyPress('+', juce::ModifierKeys::commandModifier, 0))
    {
        if (audioEngine.hasLoadedFile())
            spectrogramView.zoomIn();
        return true;
    }
    if (key == juce::KeyPress('-', juce::ModifierKeys::commandModifier, 0))
    {
        if (audioEngine.hasLoadedFile())
            spectrogramView.zoomOut();
        return true;
    }
    if (key == juce::KeyPress('0', juce::ModifierKeys::commandModifier, 0))
    {
        if (audioEngine.hasLoadedFile())
            spectrogramView.resetZoom();
        return true;
    }

    return false;
}

void SpectralzAudioProcessorEditor::showAudioSettings()
{
    if (onShowAudioSettings)
    {
        onShowAudioSettings();
    }
}

#if SPECTRALZ_ENABLE_ML
void SpectralzAudioProcessorEditor::setupStemSeparationCallbacks()
{
    // Separate button clicked
    stemSeparationPanel.onSeparateClicked = [this]()
    {
        if (!audioEngine.hasLoadedFile())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "No Audio Loaded",
                "Please load an audio file first.");
            return;
        }

        auto* buffer = audioEngine.getAudioBuffer();
        if (buffer == nullptr)
            return;

        // Show folder chooser for stem output
        auto defaultDir = currentFile.getParentDirectory();
        if (!defaultDir.exists())
            defaultDir = juce::File::getSpecialLocation(juce::File::userMusicDirectory);

        fileChooser = std::make_unique<juce::FileChooser>(
            "Choose folder to save stems",
            defaultDir,
            "",
            true);

        auto chooserFlags = juce::FileBrowserComponent::openMode |
                           juce::FileBrowserComponent::canSelectDirectories;

        fileChooser->launchAsync(chooserFlags, [this, buffer](const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (!result.exists())
                return;  // User cancelled

            stemOutputDirectory = result;
            DBG("Stem output directory: " << stemOutputDirectory.getFullPathName());

            // Start separation in background
            stemSeparationManager.startSeparation(
                *buffer,
                audioEngine.getSampleRate(),
                nullptr,  // Progress callback handled by listener
                [this](const spectralz::SeparationResult& result)
                {
                    handleStemSeparationComplete(result);
                });
        });
    };

    // Separation complete callback (already handled via callback above)
    stemSeparationPanel.onSeparationComplete = [this](const spectralz::SeparationResult& result)
    {
        // This is called from the panel's listener
        // The main handling is done in handleStemSeparationComplete
    };
}

void SpectralzAudioProcessorEditor::handleStemSeparationComplete(const spectralz::SeparationResult& result)
{
    if (!result.success)
    {
        DBG("Stem separation failed: " << result.errorMessage);
        return;
    }

    // Save stems to files
    int savedCount = 0;
    juce::String savedFilesList;

    for (const auto& [stemName, stemBuffer] : result.stems)
    {
        saveStemToFile(stemName, stemBuffer, result.sampleRate);
        savedCount++;
        if (!savedFilesList.isEmpty())
            savedFilesList += ", ";
        savedFilesList += juce::String(stemName);
    }

    // Create layers from stems if enabled
    if (stemSeparationPanel.shouldCreateLayers())
    {
        auto& editor = projectState.getEditor();

        for (const auto& [stemName, stemBuffer] : result.stems)
        {
            // Add a new layer for this stem
            editor.addLayer(stemName);

            DBG("Created layer for stem: " << stemName
                << " (" << stemBuffer.getNumSamples() << " samples)");
        }

        // Refresh the layer panel
        layerPanel.refresh();

        // Show the layers panel if hidden
        if (!layerPanelVisible)
        {
            layerPanelVisible = true;
            layerPanel.setVisible(true);
            resized();
        }
    }

    // Show success message
    juce::String message = "Successfully extracted " + juce::String(result.getStemCount()) + " stems.\n\n";
    if (stemOutputDirectory.exists())
    {
        message += "Saved to: " + stemOutputDirectory.getFullPathName() + "\n";
        message += "Files: " + savedFilesList;
    }

    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::InfoIcon,
        "Separation Complete",
        message);
}

void SpectralzAudioProcessorEditor::saveStemToFile(
    const std::string& stemName,
    const juce::AudioBuffer<float>& buffer,
    double sampleRate)
{
    if (!stemOutputDirectory.exists())
    {
        DBG("Stem output directory does not exist, skipping save");
        return;
    }

    // Create filename: originalname_stemname.wav
    juce::String baseName = currentFile.getFileNameWithoutExtension();
    if (baseName.isEmpty())
        baseName = "audio";

    juce::String filename = baseName + "_" + juce::String(stemName) + ".wav";
    juce::File outputFile = stemOutputDirectory.getChildFile(filename);

    DBG("Saving stem '" << stemName << "' to: " << outputFile.getFullPathName());

    // Create WAV writer
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outputStream(outputFile.createOutputStream());

    if (outputStream == nullptr)
    {
        DBG("Failed to create output stream for: " << outputFile.getFullPathName());
        return;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(
            outputStream.release(),  // Writer takes ownership
            sampleRate,
            static_cast<unsigned int>(buffer.getNumChannels()),
            24,  // bits per sample
            {},  // metadata
            0)); // quality

    if (writer == nullptr)
    {
        DBG("Failed to create WAV writer for: " << outputFile.getFullPathName());
        return;
    }

    // Write the audio buffer
    writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());

    DBG("Saved " << stemName << " (" << buffer.getNumSamples() << " samples @ "
        << sampleRate << "Hz) to " << outputFile.getFileName());
}
#endif
