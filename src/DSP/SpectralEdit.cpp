#include "SpectralEdit.h"
#include <algorithm>

namespace spectralz
{

SpectralEdit::SpectralEdit(SpectralOpType opType, const SpectralRegion& editRegion)
    : type(opType)
    , region(editRegion)
{
}

SpectralEdit::SpectralEdit(SpectralOpType opType, const BrushStroke& stroke)
    : type(opType)
    , brushStroke(stroke)
{
    // Set region from brush bounding box
    region = stroke.getBoundingRegion();
}

void SpectralEdit::setReplacementData(
    const std::vector<std::vector<float>>& magnitudes,
    const std::vector<std::vector<float>>& phases)
{
    replacementMagnitudes = magnitudes;
    replacementPhases = phases;
}

float SpectralEdit::getBrushInfluence(int frame, int bin) const
{
    if (!brushStroke.has_value() || brushStroke->points.empty())
        return 0.0f;

    const auto& stroke = *brushStroke;
    float maxInfluence = 0.0f;

    // Find closest brush point
    for (const auto& [pFrame, pBin] : stroke.points)
    {
        float dx = static_cast<float>(frame - pFrame);
        float dy = static_cast<float>(bin - pBin);
        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance <= stroke.radius)
        {
            // Calculate falloff
            float normalizedDist = distance / stroke.radius;
            float influence = stroke.strength;

            if (stroke.falloff > 0.0f)
            {
                // Smooth falloff using cosine interpolation
                float falloffFactor = 0.5f * (1.0f + std::cos(normalizedDist * 3.14159f * stroke.falloff));
                influence *= falloffFactor;
            }

            maxInfluence = std::max(maxInfluence, influence);
        }
    }

    return maxInfluence;
}

void SpectralEdit::apply(float& magnitude, float& phase, int frame, int bin) const
{
    if (!affects(frame, bin))
        return;

    // Get influence (1.0 for region-based, variable for brush)
    float influence = 1.0f;
    if (brushStroke.has_value())
    {
        influence = getBrushInfluence(frame, bin);
        if (influence <= 0.0f)
            return;
    }

    switch (type)
    {
        case SpectralOpType::GainMultiply:
        {
            // Interpolate gain factor based on influence
            float effectiveFactor = 1.0f + (gainFactor - 1.0f) * influence;
            magnitude *= effectiveFactor;
            break;
        }

        case SpectralOpType::GainAdd:
        {
            // Add dB (convert to linear multiply)
            // dB = 20 * log10(linear), so linear = 10^(dB/20)
            float effectiveDeltaDB = gainDeltaDB * influence;
            float linearFactor = std::pow(10.0f, effectiveDeltaDB / 20.0f);
            magnitude *= linearFactor;
            break;
        }

        case SpectralOpType::Erase:
        {
            // Fade to floor based on influence
            float floorLinear = std::pow(10.0f, eraseFloorDB / 20.0f);
            magnitude = magnitude * (1.0f - influence) + floorLinear * influence;
            break;
        }

        case SpectralOpType::PhaseShift:
        {
            phase += phaseShift * influence;
            // Wrap phase to [-pi, pi]
            while (phase > 3.14159f) phase -= 2.0f * 3.14159f;
            while (phase < -3.14159f) phase += 2.0f * 3.14159f;
            break;
        }

        case SpectralOpType::Replace:
        {
            int localFrame = frame - region.startFrame;
            int localBin = bin - region.startBin;

            if (localFrame >= 0 && localFrame < static_cast<int>(replacementMagnitudes.size()) &&
                localBin >= 0 && localBin < static_cast<int>(replacementMagnitudes[static_cast<size_t>(localFrame)].size()))
            {
                float newMag = replacementMagnitudes[static_cast<size_t>(localFrame)][static_cast<size_t>(localBin)];
                float newPhase = replacementPhases[static_cast<size_t>(localFrame)][static_cast<size_t>(localBin)];

                // Blend based on influence
                magnitude = magnitude * (1.0f - influence) + newMag * influence;
                phase = phase * (1.0f - influence) + newPhase * influence;
            }
            break;
        }
    }

    // Ensure magnitude stays non-negative
    magnitude = std::max(0.0f, magnitude);
}

bool SpectralEdit::affects(int frame, int bin) const
{
    if (brushStroke.has_value())
    {
        // Check if within brush bounding region first (fast check)
        if (!region.contains(frame, bin))
            return false;

        // Then check actual brush influence
        return getBrushInfluence(frame, bin) > 0.0f;
    }
    else
    {
        return region.contains(frame, bin);
    }
}

SpectralRegion SpectralEdit::getAffectedRegion() const
{
    return region;
}

std::unique_ptr<SpectralEdit> SpectralEdit::clone() const
{
    std::unique_ptr<SpectralEdit> copy;

    if (brushStroke.has_value())
    {
        copy = std::make_unique<SpectralEdit>(type, *brushStroke);
    }
    else
    {
        copy = std::make_unique<SpectralEdit>(type, region);
    }

    // Copy all parameters
    copy->gainFactor = gainFactor;
    copy->gainDeltaDB = gainDeltaDB;
    copy->phaseShift = phaseShift;
    copy->eraseFloorDB = eraseFloorDB;
    copy->replacementMagnitudes = replacementMagnitudes;
    copy->replacementPhases = replacementPhases;

    return copy;
}

} // namespace spectralz
