#include "SpectralLayer.h"
#include <algorithm>
#include <limits>

namespace spectralz
{

SpectralLayer::SpectralLayer(const std::string& layerName)
    : name(layerName)
{
}

void SpectralLayer::addEdit(std::unique_ptr<SpectralEdit> edit)
{
    if (edit)
    {
        edits.push_back(std::move(edit));
    }
}

void SpectralLayer::removeLastEdit()
{
    if (!edits.empty())
    {
        edits.pop_back();
    }
}

void SpectralLayer::removeEdit(size_t index)
{
    if (index < edits.size())
    {
        edits.erase(edits.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void SpectralLayer::clearEdits()
{
    edits.clear();
}

const SpectralEdit* SpectralLayer::getEdit(size_t index) const
{
    if (index < edits.size())
    {
        return edits[index].get();
    }
    return nullptr;
}

SpectralEdit* SpectralLayer::getEdit(size_t index)
{
    if (index < edits.size())
    {
        return edits[index].get();
    }
    return nullptr;
}

void SpectralLayer::applyEdits(float& magnitude, float& phase, int frame, int bin) const
{
    // Skip if layer is invisible or muted
    if (!visible || muted)
        return;

    // Store original values for opacity blending
    float originalMag = magnitude;
    float originalPhase = phase;

    // Apply all edits in order
    for (const auto& edit : edits)
    {
        edit->apply(magnitude, phase, frame, bin);
    }

    // Apply layer opacity (blend between original and edited)
    if (opacity < 1.0f)
    {
        magnitude = originalMag * (1.0f - opacity) + magnitude * opacity;
        phase = originalPhase * (1.0f - opacity) + phase * opacity;
    }
}

bool SpectralLayer::hasEditsIn(const SpectralRegion& region) const
{
    for (const auto& edit : edits)
    {
        if (edit->getAffectedRegion().overlaps(region))
        {
            return true;
        }
    }
    return false;
}

SpectralRegion SpectralLayer::getCombinedRegion() const
{
    if (edits.empty())
    {
        return {};
    }

    int minFrame = std::numeric_limits<int>::max();
    int maxFrame = std::numeric_limits<int>::min();
    int minBin = std::numeric_limits<int>::max();
    int maxBin = std::numeric_limits<int>::min();

    for (const auto& edit : edits)
    {
        const auto& region = edit->getAffectedRegion();
        minFrame = std::min(minFrame, region.startFrame);
        maxFrame = std::max(maxFrame, region.endFrame);
        minBin = std::min(minBin, region.startBin);
        maxBin = std::max(maxBin, region.endBin);
    }

    return {minFrame, maxFrame, minBin, maxBin};
}

std::unique_ptr<SpectralLayer> SpectralLayer::clone() const
{
    auto copy = std::make_unique<SpectralLayer>(name);
    copy->visible = visible;
    copy->muted = muted;
    copy->solo = solo;
    copy->opacity = opacity;

    for (const auto& edit : edits)
    {
        copy->edits.push_back(edit->clone());
    }

    return copy;
}

void SpectralLayer::toXml(juce::XmlElement& parent) const
{
    auto* layerElement = parent.createNewChildElement("Layer");
    layerElement->setAttribute("name", juce::String(name));
    layerElement->setAttribute("visible", visible);
    layerElement->setAttribute("muted", muted);
    layerElement->setAttribute("solo", solo);
    layerElement->setAttribute("opacity", static_cast<double>(opacity));

    auto* editsElement = layerElement->createNewChildElement("Edits");

    for (const auto& edit : edits)
    {
        auto* editElement = editsElement->createNewChildElement("Edit");

        // Store edit type
        editElement->setAttribute("type", static_cast<int>(edit->getType()));

        // Store region
        const auto& region = edit->getRegion();
        editElement->setAttribute("startFrame", region.startFrame);
        editElement->setAttribute("endFrame", region.endFrame);
        editElement->setAttribute("startBin", region.startBin);
        editElement->setAttribute("endBin", region.endBin);

        // Store parameters based on type
        switch (edit->getType())
        {
            case SpectralOpType::GainMultiply:
                editElement->setAttribute("gainFactor", static_cast<double>(edit->getGainFactor()));
                break;
            case SpectralOpType::GainAdd:
                editElement->setAttribute("gainDeltaDB", static_cast<double>(edit->getGainDeltaDB()));
                break;
            case SpectralOpType::Erase:
                // Floor is stored in the edit
                break;
            case SpectralOpType::PhaseShift:
            case SpectralOpType::Replace:
                // More complex serialization needed for these
                break;
        }
    }
}

std::unique_ptr<SpectralLayer> SpectralLayer::fromXml(const juce::XmlElement& element)
{
    auto layer = std::make_unique<SpectralLayer>(element.getStringAttribute("name").toStdString());
    layer->visible = element.getBoolAttribute("visible", true);
    layer->muted = element.getBoolAttribute("muted", false);
    layer->solo = element.getBoolAttribute("solo", false);
    layer->opacity = static_cast<float>(element.getDoubleAttribute("opacity", 1.0));

    if (auto* editsElement = element.getChildByName("Edits"))
    {
        for (auto* editElement : editsElement->getChildIterator())
        {
            if (editElement->hasTagName("Edit"))
            {
                SpectralRegion region;
                region.startFrame = editElement->getIntAttribute("startFrame");
                region.endFrame = editElement->getIntAttribute("endFrame");
                region.startBin = editElement->getIntAttribute("startBin");
                region.endBin = editElement->getIntAttribute("endBin");

                auto opType = static_cast<SpectralOpType>(editElement->getIntAttribute("type"));
                auto edit = std::make_unique<SpectralEdit>(opType, region);

                switch (opType)
                {
                    case SpectralOpType::GainMultiply:
                        edit->setGainFactor(static_cast<float>(editElement->getDoubleAttribute("gainFactor", 1.0)));
                        break;
                    case SpectralOpType::GainAdd:
                        edit->setGainDeltaDB(static_cast<float>(editElement->getDoubleAttribute("gainDeltaDB", 0.0)));
                        break;
                    case SpectralOpType::Erase:
                        edit->setEraseFloor(static_cast<float>(editElement->getDoubleAttribute("eraseFloor", -100.0)));
                        break;
                    default:
                        break;
                }

                layer->addEdit(std::move(edit));
            }
        }
    }

    return layer;
}

} // namespace spectralz
