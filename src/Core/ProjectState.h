#pragma once

#include "../DSP/SpectralEditor.h"
#include "../DSP/FFTProcessor.h"
#include <juce_core/juce_core.h>
#include <memory>

namespace spectralz
{

class ProjectState
{
public:
    ProjectState();
    ~ProjectState();

    // Project file operations
    [[nodiscard]] bool saveProject(const juce::File& file);
    [[nodiscard]] bool loadProject(const juce::File& file);
    [[nodiscard]] bool isModified() const { return modified; }
    void setModified(bool mod = true) { modified = mod; }

    // Source audio reference
    void setSourceAudioFile(const juce::File& file);
    [[nodiscard]] const juce::File& getSourceAudioFile() const { return sourceAudioFile; }

    // SpectralEditor access
    SpectralEditor& getEditor() { return editor; }
    const SpectralEditor& getEditor() const { return editor; }

    // Project metadata
    void setProjectName(const std::string& name) { projectName = name; }
    [[nodiscard]] const std::string& getProjectName() const { return projectName; }

    // Current project file
    void setProjectFile(const juce::File& file) { projectFile = file; }
    [[nodiscard]] const juce::File& getProjectFile() const { return projectFile; }
    [[nodiscard]] bool hasProjectFile() const { return projectFile.existsAsFile(); }

    // FFT settings used for this project
    struct FFTSettings
    {
        int fftOrder = 11;  // 2048
        int hopSize = 512;
        WindowType windowType = WindowType::Hann;
    };

    void setFFTSettings(const FFTSettings& settings) { fftSettings = settings; }
    [[nodiscard]] const FFTSettings& getFFTSettings() const { return fftSettings; }

    // Reset to new project state
    void newProject();

    // Callbacks
    std::function<void()> onModifiedChanged;
    std::function<void()> onProjectLoaded;

private:
    SpectralEditor editor;
    juce::File sourceAudioFile;
    juce::File projectFile;
    std::string projectName = "Untitled";
    FFTSettings fftSettings;
    bool modified = false;

    // File format version
    static constexpr int PROJECT_VERSION = 1;
};

} // namespace spectralz
