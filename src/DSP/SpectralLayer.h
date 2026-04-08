#pragma once

#include "SpectralEdit.h"
#include "FFTProcessor.h"
#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include <string>
#include <memory>

namespace spectralz
{

class SpectralLayer
{
public:
    explicit SpectralLayer(const std::string& layerName = "Layer");
    ~SpectralLayer() = default;

    // Non-copyable but movable
    SpectralLayer(const SpectralLayer&) = delete;
    SpectralLayer& operator=(const SpectralLayer&) = delete;
    SpectralLayer(SpectralLayer&&) = default;
    SpectralLayer& operator=(SpectralLayer&&) = default;

    // Layer properties
    void setName(const std::string& newName) { name = newName; }
    void setVisible(bool vis) { visible = vis; }
    void setMuted(bool mute) { muted = mute; }
    void setSolo(bool soloState) { solo = soloState; }
    void setOpacity(float op) { opacity = std::clamp(op, 0.0f, 1.0f); }

    [[nodiscard]] const std::string& getName() const { return name; }
    [[nodiscard]] bool isVisible() const { return visible; }
    [[nodiscard]] bool isMuted() const { return muted; }
    [[nodiscard]] bool isSolo() const { return solo; }
    [[nodiscard]] float getOpacity() const { return opacity; }

    // Edit management
    void addEdit(std::unique_ptr<SpectralEdit> edit);
    void removeLastEdit();
    void removeEdit(size_t index);
    void clearEdits();

    [[nodiscard]] size_t getNumEdits() const { return edits.size(); }
    [[nodiscard]] const SpectralEdit* getEdit(size_t index) const;
    [[nodiscard]] SpectralEdit* getEdit(size_t index);

    // Apply all edits to a magnitude/phase pair at given frame/bin
    // Respects layer opacity
    void applyEdits(float& magnitude, float& phase, int frame, int bin) const;

    // Check if layer has any edits affecting a region
    [[nodiscard]] bool hasEditsIn(const SpectralRegion& region) const;

    // Get combined bounding region of all edits
    [[nodiscard]] SpectralRegion getCombinedRegion() const;

    // Clone layer (deep copy of all edits)
    [[nodiscard]] std::unique_ptr<SpectralLayer> clone() const;

    // Serialization
    void toXml(juce::XmlElement& parent) const;
    static std::unique_ptr<SpectralLayer> fromXml(const juce::XmlElement& element);

private:
    std::string name;
    bool visible = true;
    bool muted = false;
    bool solo = false;
    float opacity = 1.0f;

    std::vector<std::unique_ptr<SpectralEdit>> edits;
};

} // namespace spectralz
