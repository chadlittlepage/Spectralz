/*
    Custom Standalone Application for Spectralz
    Provides native macOS window controls (traffic lights)
    without JUCE's default Options/Settings buttons
*/

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Plugin/PluginProcessor.h"
#include "../Plugin/PluginEditor.h"

//==============================================================================
class AudioSettingsWindow : public juce::DocumentWindow
{
public:
    AudioSettingsWindow(juce::AudioDeviceManager& dm)
        : DocumentWindow("Audio/MIDI Settings",
                         juce::Colour(0xff2a2a3e),
                         DocumentWindow::closeButton,
                         true),
          deviceManager(dm)
    {
        auto* selector = new juce::AudioDeviceSelectorComponent(
            deviceManager,
            0, 2,    // Min/max input channels
            0, 2,    // Min/max output channels
            false,   // Show MIDI input options
            false,   // Show MIDI output options
            false,   // Treat channels as stereo pairs
            false);  // Hide advanced options

        selector->setSize(500, 400);
        setContentOwned(selector, true);
        setUsingNativeTitleBar(true);
        setResizable(false, false);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }

private:
    juce::AudioDeviceManager& deviceManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsWindow)
};

//==============================================================================
class SpectralzWindow : public juce::DocumentWindow
{
public:
    SpectralzWindow(const juce::String& name,
                    std::unique_ptr<SpectralzAudioProcessor> proc,
                    juce::AudioDeviceManager& dm)
        : DocumentWindow(name,
                         juce::Colour(0xff1a1a1a),
                         DocumentWindow::allButtons,
                         true),
          processor(std::move(proc)),
          deviceManager(dm)
    {
        // Use native title bar for macOS traffic lights
        setUsingNativeTitleBar(true);
        setResizable(true, true);

        // Setup audio
        player.setProcessor(processor.get());
        deviceManager.addAudioCallback(&player);

        // Create editor
        if (auto* ed = processor->createEditorIfNeeded())
        {
            editor = dynamic_cast<SpectralzAudioProcessorEditor*>(ed);

            // Hook up audio settings callback
            if (editor != nullptr)
            {
                editor->onShowAudioSettings = [this]()
                {
                    showAudioSettings();
                };
            }

            setContentOwned(ed, true);
            centreWithSize(ed->getWidth(), ed->getHeight());
        }

        setVisible(true);
    }

    ~SpectralzWindow() override
    {
        deviceManager.removeAudioCallback(&player);
        player.setProcessor(nullptr);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    void showAudioSettings()
    {
        if (audioSettingsWindow == nullptr)
        {
            audioSettingsWindow = std::make_unique<AudioSettingsWindow>(deviceManager);
        }
        else
        {
            audioSettingsWindow->setVisible(true);
            audioSettingsWindow->toFront(true);
        }
    }

private:
    std::unique_ptr<SpectralzAudioProcessor> processor;
    SpectralzAudioProcessorEditor* editor = nullptr;
    juce::AudioDeviceManager& deviceManager;
    juce::AudioProcessorPlayer player;
    std::unique_ptr<AudioSettingsWindow> audioSettingsWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralzWindow)
};

//==============================================================================
class SpectralzApplication : public juce::JUCEApplication
{
public:
    SpectralzApplication() = default;

    const juce::String getApplicationName() override { return "Spectralz"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        // Initialize audio - output only to avoid feedback
        deviceManager.initialiseWithDefaultDevices(0, 2);

        // Create processor
        auto proc = std::make_unique<SpectralzAudioProcessor>();

        // Create main window
        mainWindow = std::make_unique<SpectralzWindow>(
            "Spectralz",
            std::move(proc),
            deviceManager);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override
    {
        if (mainWindow != nullptr)
            mainWindow->toFront(true);
    }

private:
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<SpectralzWindow> mainWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralzApplication)
};

//==============================================================================
START_JUCE_APPLICATION(SpectralzApplication)
