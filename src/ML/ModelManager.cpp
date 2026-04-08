#if SPECTRALZ_ENABLE_ML

#include "ModelManager.h"

namespace spectralz
{

ModelManager::ModelManager()
{
    // Set default models directory based on platform
#if JUCE_MAC
    modelsDirectory = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
        .getChildFile("Spectralz")
        .getChildFile("models");
#elif JUCE_WINDOWS
    modelsDirectory = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
        .getChildFile("Spectralz")
        .getChildFile("models");
#else
    modelsDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Spectralz")
        .getChildFile("models");
#endif
}

void ModelManager::setModelsDirectory(const juce::File& directory)
{
    if (modelsDirectory != directory)
    {
        modelsDirectory = directory;
        modelsDirty = true;
    }
}

void ModelManager::scanForModels()
{
    models.clear();

    // List of directories to scan
    std::vector<juce::File> searchDirs;
    searchDirs.push_back(modelsDirectory);

    // Also check app bundle resources on macOS
#if JUCE_MAC
    auto appBundle = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    auto bundleResourcesDir = appBundle.getChildFile("Contents/Resources/models");
    if (bundleResourcesDir.isDirectory())
    {
        searchDirs.push_back(bundleResourcesDir);
    }
#endif

    // During development, also check the source resources directory
    // Look relative to the executable location
    auto executableDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();

    // Try various paths relative to build directory
    std::vector<juce::String> relativePaths = {
        "../../../resources/models",           // Debug build
        "../../../../resources/models",        // Release build
        "../resources/models",
        "../../resources/models",
        "../../../../../resources/models"
    };

    for (const auto& relPath : relativePaths)
    {
        auto devResourcesDir = executableDir.getChildFile(relPath);
        if (devResourcesDir.isDirectory())
        {
            searchDirs.push_back(devResourcesDir);
            DBG("Found dev resources at: " << devResourcesDir.getFullPathName());
            break;
        }
    }

    // Also check the project source directory directly (for development)
    auto sourceResourcesDir = juce::File("/Users/chadlittlepage/Documents/APPs/Spectralz/resources/models");
    if (sourceResourcesDir.isDirectory())
    {
        searchDirs.push_back(sourceResourcesDir);
    }

    // Create main models directory if it doesn't exist
    if (!modelsDirectory.isDirectory())
    {
        modelsDirectory.createDirectory();
    }

    // Scan all directories
    for (const auto& dir : searchDirs)
    {
        if (!dir.isDirectory())
            continue;

        DBG("Scanning for models in: " << dir.getFullPathName());

        // Scan Demucs subdirectory
        auto demucsDir = dir.getChildFile("demucs");
        if (demucsDir.isDirectory())
        {
            scanEngineDirectory(demucsDir, SeparationEngine::Demucs);
        }

        // Scan Spleeter subdirectory
        auto spleeterDir = dir.getChildFile("spleeter");
        if (spleeterDir.isDirectory())
        {
            scanEngineDirectory(spleeterDir, SeparationEngine::Spleeter);
        }
    }

    // If no models found via manifest, try to find .onnx files directly
    if (models.empty())
    {
        DBG("No manifests found, scanning for .onnx files directly");

        for (const auto& dir : searchDirs)
        {
            if (!dir.isDirectory())
                continue;

            // Scan for any .onnx files
            for (const auto& file : juce::RangedDirectoryIterator(dir, true, "*.onnx"))
            {
                auto modelFile = file.getFile();
                auto parentDir = modelFile.getParentDirectory();
                auto parentName = parentDir.getFileName().toLowerCase();

                SeparationEngine engine = SeparationEngine::Demucs;
                if (parentName.contains("spleeter"))
                    engine = SeparationEngine::Spleeter;

                // Create model info from filename
                ModelInfo info;
                info.id = modelFile.getFileNameWithoutExtension().toStdString();
                info.displayName = info.id;
                info.engine = engine;
                info.modelFile = modelFile;
                info.fileSizeBytes = modelFile.getSize();
                info.isValid = modelFile.existsAsFile();

                // Infer stems from common model names
                auto nameLower = juce::String(info.id).toLowerCase();
                if (nameLower.contains("6s") || nameLower.contains("6stem"))
                {
                    info.stems = {"drums", "bass", "other", "vocals", "guitar", "piano"};
                }
                else if (nameLower.contains("5stem"))
                {
                    info.stems = {"vocals", "drums", "bass", "piano", "other"};
                }
                else if (nameLower.contains("4stem") || nameLower.contains("htdemucs"))
                {
                    info.stems = {"drums", "bass", "other", "vocals"};
                }
                else if (nameLower.contains("2stem"))
                {
                    info.stems = {"vocals", "accompaniment"};
                }
                else
                {
                    info.stems = {"vocals", "accompaniment"};  // Default assumption
                }

                models.push_back(info);
                DBG("Found model: " << info.displayName << " (" << info.stems.size() << " stems)");
            }
        }
    }

    // If still no models, add built-in definitions (marked as not valid/not downloaded)
    if (models.empty())
    {
        models = getBuiltInModelDefinitions();
    }

    modelsDirty = false;
    notifyListeners();
}

void ModelManager::scanEngineDirectory(const juce::File& dir, SeparationEngine engine)
{
    // Look for manifest.json
    auto manifestFile = dir.getChildFile("manifest.json");
    if (manifestFile.existsAsFile())
    {
        parseManifest(manifestFile, engine);
    }
}

bool ModelManager::parseManifest(const juce::File& manifestFile, SeparationEngine engine)
{
    auto content = manifestFile.loadFileAsString();
    if (content.isEmpty())
        return false;

    auto json = juce::JSON::parse(content);
    if (!json.isObject())
        return false;

    auto modelsArray = json.getProperty("models", juce::var());
    if (!modelsArray.isArray())
        return false;

    auto parentDir = manifestFile.getParentDirectory();

    for (int i = 0; i < modelsArray.size(); ++i)
    {
        auto modelObj = modelsArray[i];
        if (!modelObj.isObject())
            continue;

        ModelInfo info;
        info.engine = engine;

        // Parse required fields - support both filename (single file) and modelDir (multi-file)
        auto filename = modelObj.getProperty("filename", "").toString();
        auto modelDir = modelObj.getProperty("modelDir", "").toString();

        if (filename.isEmpty() && modelDir.isEmpty())
            continue;

        // Parse stems array first (needed for multi-file validation)
        auto stemsArray = modelObj.getProperty("stems", juce::var());
        if (stemsArray.isArray())
        {
            for (int j = 0; j < stemsArray.size(); ++j)
            {
                info.stems.push_back(stemsArray[j].toString().toStdString());
            }
        }

        if (!modelDir.isEmpty())
        {
            // Multi-file model (Spleeter style: one .onnx per stem)
            info.modelDir = parentDir.getChildFile(modelDir);
            info.usesMultipleFiles = true;

            // Validate: check that all stem .onnx files exist
            info.isValid = info.modelDir.isDirectory();
            if (info.isValid)
            {
                for (const auto& stem : info.stems)
                {
                    auto stemFile = info.modelDir.getChildFile(juce::String(stem) + ".onnx");
                    if (!stemFile.existsAsFile())
                    {
                        info.isValid = false;
                        DBG("Missing stem file: " << stemFile.getFullPathName());
                        break;
                    }
                    info.fileSizeBytes += stemFile.getSize();
                }
            }
        }
        else
        {
            // Single-file model (Demucs style)
            info.modelFile = parentDir.getChildFile(filename);
            info.usesMultipleFiles = false;
            info.isValid = info.modelFile.existsAsFile();
            if (info.isValid)
            {
                info.fileSizeBytes = info.modelFile.getSize();
            }
        }

        info.id = modelObj.getProperty("id", filename.isEmpty() ? modelDir : filename).toString().toStdString();
        info.displayName = modelObj.getProperty("displayName", juce::String(info.id)).toString().toStdString();
        info.sampleRate = static_cast<int>(modelObj.getProperty("sampleRate", 44100));

        models.push_back(info);
        DBG("Loaded model from manifest: " << info.displayName
            << " (valid: " << (info.isValid ? "yes" : "no") << ")");
    }

    return true;
}

std::vector<ModelInfo> ModelManager::getAvailableModels() const
{
    return models;
}

std::vector<ModelInfo> ModelManager::getModelsForEngine(SeparationEngine engine) const
{
    std::vector<ModelInfo> result;
    for (const auto& model : models)
    {
        if (model.engine == engine)
            result.push_back(model);
    }
    return result;
}

std::optional<ModelInfo> ModelManager::getModelById(const std::string& id) const
{
    for (const auto& model : models)
    {
        if (model.id == id)
            return model;
    }
    return std::nullopt;
}

std::optional<ModelInfo> ModelManager::getDefaultModel() const
{
    // Prefer htdemucs (4 stem) as default - good balance of quality and speed
    for (const auto& model : models)
    {
        if (model.id == "htdemucs" && model.isValid)
            return model;
    }

    // Fallback to any valid model
    for (const auto& model : models)
    {
        if (model.isValid)
            return model;
    }

    // Return first model even if not valid (for UI purposes)
    if (!models.empty())
        return models[0];

    return std::nullopt;
}

bool ModelManager::isModelValid(const std::string& id) const
{
    auto model = getModelById(id);
    return model.has_value() && model->isValid;
}

void ModelManager::addListener(Listener* listener)
{
    listeners.add(listener);
}

void ModelManager::removeListener(Listener* listener)
{
    listeners.remove(listener);
}

void ModelManager::notifyListeners()
{
    listeners.call([](Listener& l) { l.modelsChanged(); });
}

std::vector<ModelInfo> ModelManager::getBuiltInModelDefinitions()
{
    std::vector<ModelInfo> builtIn;

    // Demucs models
    {
        ModelInfo info;
        info.id = "htdemucs";
        info.displayName = "Demucs HT (4 stems)";
        info.engine = SeparationEngine::Demucs;
        info.stems = {"drums", "bass", "other", "vocals"};
        info.sampleRate = 44100;
        info.isValid = false;
        builtIn.push_back(info);
    }

    {
        ModelInfo info;
        info.id = "htdemucs_6s";
        info.displayName = "Demucs HT 6-stem";
        info.engine = SeparationEngine::Demucs;
        info.stems = {"drums", "bass", "other", "vocals", "guitar", "piano"};
        info.sampleRate = 44100;
        info.isValid = false;
        builtIn.push_back(info);
    }

    // Spleeter models
    {
        ModelInfo info;
        info.id = "spleeter_2stems";
        info.displayName = "Spleeter (2 stems)";
        info.engine = SeparationEngine::Spleeter;
        info.stems = {"vocals", "accompaniment"};
        info.sampleRate = 44100;
        info.isValid = false;
        builtIn.push_back(info);
    }

    {
        ModelInfo info;
        info.id = "spleeter_5stems";
        info.displayName = "Spleeter (5 stems)";
        info.engine = SeparationEngine::Spleeter;
        info.stems = {"vocals", "drums", "bass", "piano", "other"};
        info.sampleRate = 44100;
        info.isValid = false;
        builtIn.push_back(info);
    }

    return builtIn;
}

} // namespace spectralz

#endif // SPECTRALZ_ENABLE_ML
