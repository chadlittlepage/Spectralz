#pragma once

#include "SpectralLayer.h"
#include "SpectralEdit.h"
#include "FFTProcessor.h"
#include "../Core/UndoManager.h"
#include <juce_data_structures/juce_data_structures.h>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>

namespace spectralz
{

// Clipboard for copy/paste operations
struct SpectralClipboard
{
    SpectralRegion region;
    std::vector<std::vector<float>> magnitudes;
    std::vector<std::vector<float>> phases;
    bool hasData = false;

    void clear()
    {
        magnitudes.clear();
        phases.clear();
        hasData = false;
    }
};

class SpectralEditor
{
public:
    SpectralEditor();
    ~SpectralEditor();

    // Set source spectrogram (immutable reference to original data)
    void setSourceSpectrogram(const FFTProcessor::SpectrogramData& source);
    [[nodiscard]] bool hasSourceData() const { return sourceData != nullptr; }
    [[nodiscard]] const FFTProcessor::SpectrogramData* getSourceData() const { return sourceData.get(); }

    // Layer management
    int addLayer(const std::string& name = "Layer");
    void insertLayer(int index, std::unique_ptr<SpectralLayer> layer);
    void removeLayer(int index);
    void moveLayer(int fromIndex, int toIndex);
    void setActiveLayer(int index);

    [[nodiscard]] int getActiveLayerIndex() const { return activeLayerIndex; }
    [[nodiscard]] int getNumLayers() const { return static_cast<int>(layers.size()); }
    [[nodiscard]] SpectralLayer* getLayer(int index);
    [[nodiscard]] const SpectralLayer* getLayer(int index) const;
    [[nodiscard]] SpectralLayer* getActiveLayer();

    // Check if any layer has solo enabled
    [[nodiscard]] bool hasSoloLayer() const;

    // Edit operations (all go through undo system)
    void applyGain(const SpectralRegion& region, float gainDB);
    void applyBrushGain(const BrushStroke& stroke, float gainDB);
    void erase(const SpectralRegion& region, float floorDB = -100.0f);
    void brushErase(const BrushStroke& stroke, float floorDB = -100.0f);
    void phaseShift(const SpectralRegion& region, float deltaPhi);

    // Copy/paste
    void copyRegion(const SpectralRegion& region);
    void cutRegion(const SpectralRegion& region);
    void paste(int destFrame, int destBin);
    [[nodiscard]] bool hasClipboardData() const { return clipboard.hasData; }
    [[nodiscard]] const SpectralClipboard& getClipboard() const { return clipboard; }

    // Composite all visible layers and return edited spectrogram
    [[nodiscard]] FFTProcessor::SpectrogramData composite() const;

    // Get composited value for single frame/bin (for real-time display)
    void getComposited(int frame, int bin, float& magnitude, float& phase) const;

    // Invalidate composite cache (call when edits change)
    void invalidateCache();

    // Undo/Redo
    void undo();
    void redo();
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] std::string getUndoDescription() const;
    [[nodiscard]] std::string getRedoDescription() const;

    // Direct access to undo manager
    [[nodiscard]] UndoManager& getUndoManager() { return *undoManager; }

    // Clear all layers and edits
    void clear();

    // Callbacks
    std::function<void()> onLayersChanged;
    std::function<void()> onEditApplied;
    std::function<void()> onUndoStateChanged;

    // Serialization (for project save/load)
    void toXml(juce::XmlElement& parent) const;
    void fromXml(const juce::XmlElement& element);

private:
    void executeCommand(std::unique_ptr<Command> command);
    void notifyLayersChanged();
    void notifyEditApplied();

    std::unique_ptr<FFTProcessor::SpectrogramData> sourceData;
    std::vector<std::unique_ptr<SpectralLayer>> layers;
    int activeLayerIndex = 0;

    std::unique_ptr<UndoManager> undoManager;
    SpectralClipboard clipboard;

    // Cache for composited data (invalidated on edit)
    mutable bool compositeCacheValid = false;
    mutable FFTProcessor::SpectrogramData compositeCache;
    mutable std::mutex compositeMutex;
};

} // namespace spectralz
