#include "SpectralEditor.h"
#include <algorithm>

namespace spectralz
{

SpectralEditor::SpectralEditor()
    : undoManager(std::make_unique<UndoManager>(100))
{
    // Setup undo manager callback
    undoManager->onStateChanged = [this]()
    {
        if (onUndoStateChanged)
            onUndoStateChanged();
    };

    // Create default layer
    layers.push_back(std::make_unique<SpectralLayer>("Layer 1"));
}

SpectralEditor::~SpectralEditor() = default;

void SpectralEditor::setSourceSpectrogram(const FFTProcessor::SpectrogramData& source)
{
    // Make a copy of the spectrogram data (the source may be temporary)
    sourceData = std::make_unique<FFTProcessor::SpectrogramData>(source);
    invalidateCache();
}

int SpectralEditor::addLayer(const std::string& name)
{
    auto layer = std::make_unique<SpectralLayer>(name);
    layers.push_back(std::move(layer));
    int newIndex = static_cast<int>(layers.size()) - 1;

    notifyLayersChanged();
    return newIndex;
}

void SpectralEditor::insertLayer(int index, std::unique_ptr<SpectralLayer> layer)
{
    if (index < 0)
        index = 0;
    if (index > static_cast<int>(layers.size()))
        index = static_cast<int>(layers.size());

    layers.insert(layers.begin() + index, std::move(layer));

    // Adjust active layer index if needed
    if (activeLayerIndex >= index)
        activeLayerIndex++;

    notifyLayersChanged();
}

void SpectralEditor::removeLayer(int index)
{
    if (index < 0 || index >= static_cast<int>(layers.size()))
        return;

    // Don't remove the last layer
    if (layers.size() <= 1)
        return;

    layers.erase(layers.begin() + index);

    // Adjust active layer index
    if (activeLayerIndex >= static_cast<int>(layers.size()))
        activeLayerIndex = static_cast<int>(layers.size()) - 1;

    invalidateCache();
    notifyLayersChanged();
}

void SpectralEditor::moveLayer(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= static_cast<int>(layers.size()))
        return;
    if (toIndex < 0 || toIndex >= static_cast<int>(layers.size()))
        return;
    if (fromIndex == toIndex)
        return;

    auto layer = std::move(layers[static_cast<size_t>(fromIndex)]);
    layers.erase(layers.begin() + fromIndex);
    layers.insert(layers.begin() + toIndex, std::move(layer));

    // Update active layer index
    if (activeLayerIndex == fromIndex)
        activeLayerIndex = toIndex;
    else if (fromIndex < activeLayerIndex && toIndex >= activeLayerIndex)
        activeLayerIndex--;
    else if (fromIndex > activeLayerIndex && toIndex <= activeLayerIndex)
        activeLayerIndex++;

    invalidateCache();
    notifyLayersChanged();
}

void SpectralEditor::setActiveLayer(int index)
{
    if (index >= 0 && index < static_cast<int>(layers.size()))
    {
        activeLayerIndex = index;
        notifyLayersChanged();
    }
}

SpectralLayer* SpectralEditor::getLayer(int index)
{
    if (index >= 0 && index < static_cast<int>(layers.size()))
        return layers[static_cast<size_t>(index)].get();
    return nullptr;
}

const SpectralLayer* SpectralEditor::getLayer(int index) const
{
    if (index >= 0 && index < static_cast<int>(layers.size()))
        return layers[static_cast<size_t>(index)].get();
    return nullptr;
}

SpectralLayer* SpectralEditor::getActiveLayer()
{
    return getLayer(activeLayerIndex);
}

bool SpectralEditor::hasSoloLayer() const
{
    for (const auto& layer : layers)
    {
        if (layer->isSolo())
            return true;
    }
    return false;
}

void SpectralEditor::applyGain(const SpectralRegion& region, float gainDB)
{
    auto* layer = getActiveLayer();
    if (!layer || !region.isValid())
        return;

    auto edit = std::make_unique<SpectralEdit>(SpectralOpType::GainAdd, region);
    edit->setGainDeltaDB(gainDB);

    auto command = std::make_unique<AddEditCommand>(
        layer, std::move(edit),
        gainDB >= 0 ? "Boost Gain" : "Reduce Gain");

    executeCommand(std::move(command));
}

void SpectralEditor::applyBrushGain(const BrushStroke& stroke, float gainDB)
{
    auto* layer = getActiveLayer();
    if (!layer || stroke.isEmpty())
        return;

    auto edit = std::make_unique<SpectralEdit>(SpectralOpType::GainAdd, stroke);
    edit->setGainDeltaDB(gainDB);

    auto command = std::make_unique<AddEditCommand>(
        layer, std::move(edit),
        gainDB >= 0 ? "Brush Boost" : "Brush Attenuate");

    executeCommand(std::move(command));
}

void SpectralEditor::erase(const SpectralRegion& region, float floorDB)
{
    auto* layer = getActiveLayer();
    if (!layer || !region.isValid())
        return;

    auto edit = std::make_unique<SpectralEdit>(SpectralOpType::Erase, region);
    edit->setEraseFloor(floorDB);

    auto command = std::make_unique<AddEditCommand>(
        layer, std::move(edit), "Erase");

    executeCommand(std::move(command));
}

void SpectralEditor::brushErase(const BrushStroke& stroke, float floorDB)
{
    auto* layer = getActiveLayer();
    if (!layer || stroke.isEmpty())
        return;

    auto edit = std::make_unique<SpectralEdit>(SpectralOpType::Erase, stroke);
    edit->setEraseFloor(floorDB);

    auto command = std::make_unique<AddEditCommand>(
        layer, std::move(edit), "Brush Erase");

    executeCommand(std::move(command));
}

void SpectralEditor::phaseShift(const SpectralRegion& region, float deltaPhi)
{
    auto* layer = getActiveLayer();
    if (!layer || !region.isValid())
        return;

    auto edit = std::make_unique<SpectralEdit>(SpectralOpType::PhaseShift, region);
    edit->setPhaseShift(deltaPhi);

    auto command = std::make_unique<AddEditCommand>(
        layer, std::move(edit), "Phase Shift");

    executeCommand(std::move(command));
}

void SpectralEditor::copyRegion(const SpectralRegion& region)
{
    if (!sourceData || !region.isValid())
        return;

    // Get composited data for the region
    clipboard.region = region;
    clipboard.magnitudes.clear();
    clipboard.phases.clear();

    for (int frame = region.startFrame; frame < region.endFrame; ++frame)
    {
        std::vector<float> magRow;
        std::vector<float> phaseRow;

        for (int bin = region.startBin; bin < region.endBin; ++bin)
        {
            float mag, phase;
            getComposited(frame, bin, mag, phase);
            magRow.push_back(mag);
            phaseRow.push_back(phase);
        }

        clipboard.magnitudes.push_back(std::move(magRow));
        clipboard.phases.push_back(std::move(phaseRow));
    }

    clipboard.hasData = true;
}

void SpectralEditor::cutRegion(const SpectralRegion& region)
{
    copyRegion(region);
    erase(region);
}

void SpectralEditor::paste(int destFrame, int destBin)
{
    auto* layer = getActiveLayer();
    if (!layer || !clipboard.hasData)
        return;

    SpectralRegion destRegion;
    destRegion.startFrame = destFrame;
    destRegion.endFrame = destFrame + static_cast<int>(clipboard.magnitudes.size());
    destRegion.startBin = destBin;
    destRegion.endBin = destBin + (clipboard.magnitudes.empty() ? 0 :
                        static_cast<int>(clipboard.magnitudes[0].size()));

    auto edit = std::make_unique<SpectralEdit>(SpectralOpType::Replace, destRegion);
    edit->setReplacementData(clipboard.magnitudes, clipboard.phases);

    auto command = std::make_unique<AddEditCommand>(
        layer, std::move(edit), "Paste");

    executeCommand(std::move(command));
}

FFTProcessor::SpectrogramData SpectralEditor::composite() const
{
    std::lock_guard<std::mutex> lock(compositeMutex);

    if (compositeCacheValid)
        return compositeCache;

    if (!sourceData)
        return {};

    // Start with a copy of source data
    compositeCache = *sourceData;

    // Check for solo layers
    bool hasSolo = hasSoloLayer();

    // Apply edits from all visible layers
    for (int frame = 0; frame < compositeCache.numFrames; ++frame)
    {
        for (int bin = 0; bin < compositeCache.numBins; ++bin)
        {
            float& mag = compositeCache.magnitudes[static_cast<size_t>(frame)][static_cast<size_t>(bin)];
            float& phase = compositeCache.phases[static_cast<size_t>(frame)][static_cast<size_t>(bin)];

            for (const auto& layer : layers)
            {
                // Skip non-visible or muted layers
                if (!layer->isVisible() || layer->isMuted())
                    continue;

                // If any layer is soloed, skip non-solo layers
                if (hasSolo && !layer->isSolo())
                    continue;

                layer->applyEdits(mag, phase, frame, bin);
            }
        }
    }

    compositeCacheValid = true;
    return compositeCache;
}

void SpectralEditor::getComposited(int frame, int bin, float& magnitude, float& phase) const
{
    if (!sourceData)
    {
        magnitude = 0.0f;
        phase = 0.0f;
        return;
    }

    // Bounds check
    if (frame < 0 || frame >= sourceData->numFrames ||
        bin < 0 || bin >= sourceData->numBins)
    {
        magnitude = 0.0f;
        phase = 0.0f;
        return;
    }

    // Start with source data
    magnitude = sourceData->magnitudes[static_cast<size_t>(frame)][static_cast<size_t>(bin)];
    phase = sourceData->phases[static_cast<size_t>(frame)][static_cast<size_t>(bin)];

    // Check for solo layers
    bool hasSolo = hasSoloLayer();

    // Apply edits from all visible layers
    for (const auto& layer : layers)
    {
        if (!layer->isVisible() || layer->isMuted())
            continue;

        if (hasSolo && !layer->isSolo())
            continue;

        layer->applyEdits(magnitude, phase, frame, bin);
    }
}

void SpectralEditor::invalidateCache()
{
    std::lock_guard<std::mutex> lock(compositeMutex);
    compositeCacheValid = false;
}

void SpectralEditor::undo()
{
    undoManager->undo();
    invalidateCache();
    notifyEditApplied();
}

void SpectralEditor::redo()
{
    undoManager->redo();
    invalidateCache();
    notifyEditApplied();
}

bool SpectralEditor::canUndo() const
{
    return undoManager->canUndo();
}

bool SpectralEditor::canRedo() const
{
    return undoManager->canRedo();
}

std::string SpectralEditor::getUndoDescription() const
{
    return undoManager->getUndoDescription();
}

std::string SpectralEditor::getRedoDescription() const
{
    return undoManager->getRedoDescription();
}

void SpectralEditor::clear()
{
    layers.clear();
    layers.push_back(std::make_unique<SpectralLayer>("Layer 1"));
    activeLayerIndex = 0;
    undoManager->clear();
    clipboard.clear();
    invalidateCache();
    notifyLayersChanged();
}

void SpectralEditor::executeCommand(std::unique_ptr<Command> command)
{
    undoManager->executeCommand(std::move(command));
    invalidateCache();
    notifyEditApplied();
}

void SpectralEditor::notifyLayersChanged()
{
    if (onLayersChanged)
        onLayersChanged();
}

void SpectralEditor::notifyEditApplied()
{
    if (onEditApplied)
        onEditApplied();
}

void SpectralEditor::toXml(juce::XmlElement& parent) const
{
    auto* editorElement = parent.createNewChildElement("SpectralEditor");
    editorElement->setAttribute("activeLayer", activeLayerIndex);

    auto* layersElement = editorElement->createNewChildElement("Layers");
    for (const auto& layer : layers)
    {
        layer->toXml(*layersElement);
    }
}

void SpectralEditor::fromXml(const juce::XmlElement& element)
{
    clear();
    layers.clear();

    if (auto* editorElement = element.getChildByName("SpectralEditor"))
    {
        activeLayerIndex = editorElement->getIntAttribute("activeLayer", 0);

        if (auto* layersElement = editorElement->getChildByName("Layers"))
        {
            for (auto* layerElement : layersElement->getChildIterator())
            {
                if (layerElement->hasTagName("Layer"))
                {
                    auto layer = SpectralLayer::fromXml(*layerElement);
                    if (layer)
                    {
                        layers.push_back(std::move(layer));
                    }
                }
            }
        }
    }

    // Ensure at least one layer exists
    if (layers.empty())
    {
        layers.push_back(std::make_unique<SpectralLayer>("Layer 1"));
    }

    // Validate active layer index
    if (activeLayerIndex >= static_cast<int>(layers.size()))
    {
        activeLayerIndex = 0;
    }

    invalidateCache();
    notifyLayersChanged();
}

} // namespace spectralz
