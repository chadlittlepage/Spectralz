#include "ProjectState.h"

namespace spectralz
{

ProjectState::ProjectState()
{
    // Connect to editor callbacks to track modifications
    editor.onEditApplied = [this]()
    {
        setModified(true);
    };

    editor.onLayersChanged = [this]()
    {
        setModified(true);
    };
}

ProjectState::~ProjectState() = default;

void ProjectState::setSourceAudioFile(const juce::File& file)
{
    sourceAudioFile = file;
    setModified(true);
}

void ProjectState::newProject()
{
    editor.clear();
    sourceAudioFile = juce::File();
    projectFile = juce::File();
    projectName = "Untitled";
    fftSettings = FFTSettings();
    modified = false;

    if (onModifiedChanged)
        onModifiedChanged();
}

bool ProjectState::saveProject(const juce::File& file)
{
    // Create XML document
    auto xml = std::make_unique<juce::XmlElement>("SpectralzProject");
    xml->setAttribute("version", PROJECT_VERSION);

    // Metadata
    auto* metadata = xml->createNewChildElement("Metadata");
    metadata->setAttribute("name", juce::String(projectName));
    metadata->setAttribute("created", juce::Time::getCurrentTime().toISO8601(true));

    // Source audio file
    auto* source = xml->createNewChildElement("Source");
    if (sourceAudioFile.existsAsFile())
    {
        source->setAttribute("audioFile", sourceAudioFile.getFullPathName());
        // Note: File hash for integrity checking can be added later if needed
    }

    // FFT settings
    auto* fftElement = xml->createNewChildElement("FFTSettings");
    fftElement->setAttribute("fftOrder", fftSettings.fftOrder);
    fftElement->setAttribute("hopSize", fftSettings.hopSize);
    fftElement->setAttribute("windowType", static_cast<int>(fftSettings.windowType));

    // Editor state (layers and edits)
    editor.toXml(*xml);

    // Write to file
    if (!xml->writeTo(file))
    {
        return false;
    }

    projectFile = file;
    modified = false;

    if (onModifiedChanged)
        onModifiedChanged();

    return true;
}

bool ProjectState::loadProject(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse(file);
    if (!xml || !xml->hasTagName("SpectralzProject"))
        return false;

    // Check version
    int version = xml->getIntAttribute("version", 0);
    if (version > PROJECT_VERSION)
    {
        // File is from a newer version
        return false;
    }

    // Load metadata
    if (auto* metadata = xml->getChildByName("Metadata"))
    {
        projectName = metadata->getStringAttribute("name").toStdString();
    }

    // Load source audio reference
    if (auto* source = xml->getChildByName("Source"))
    {
        juce::String audioPath = source->getStringAttribute("audioFile");
        if (audioPath.isNotEmpty())
        {
            sourceAudioFile = juce::File(audioPath);

            // TODO: Verify hash if file exists
        }
    }

    // Load FFT settings
    if (auto* fftElement = xml->getChildByName("FFTSettings"))
    {
        fftSettings.fftOrder = fftElement->getIntAttribute("fftOrder", 11);
        fftSettings.hopSize = fftElement->getIntAttribute("hopSize", 512);
        fftSettings.windowType = static_cast<WindowType>(
            fftElement->getIntAttribute("windowType", 0));
    }

    // Load editor state
    editor.fromXml(*xml);

    projectFile = file;
    modified = false;

    if (onModifiedChanged)
        onModifiedChanged();

    if (onProjectLoaded)
        onProjectLoaded();

    return true;
}

} // namespace spectralz
