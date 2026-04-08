#pragma once

#include <vector>
#include <memory>
#include <optional>
#include <cmath>

namespace spectralz
{

// Types of spectral operations
enum class SpectralOpType
{
    GainMultiply,      // Multiply magnitude by factor
    GainAdd,           // Add dB to magnitude
    Erase,             // Set magnitude to floor value
    PhaseShift,        // Shift phase by delta
    Replace            // Replace with specific magnitude/phase
};

// Region specification for edits (in frame/bin coordinates)
struct SpectralRegion
{
    int startFrame = 0;
    int endFrame = 0;
    int startBin = 0;
    int endBin = 0;

    [[nodiscard]] bool contains(int frame, int bin) const
    {
        return frame >= startFrame && frame < endFrame &&
               bin >= startBin && bin < endBin;
    }

    [[nodiscard]] bool overlaps(const SpectralRegion& other) const
    {
        return !(endFrame <= other.startFrame || startFrame >= other.endFrame ||
                 endBin <= other.startBin || startBin >= other.endBin);
    }

    [[nodiscard]] int numFrames() const { return endFrame - startFrame; }
    [[nodiscard]] int numBins() const { return endBin - startBin; }
    [[nodiscard]] bool isValid() const { return numFrames() > 0 && numBins() > 0; }
};

// Brush stroke data (for smooth brush operations)
struct BrushStroke
{
    std::vector<std::pair<int, int>> points;  // (frame, bin) pairs
    float radius = 5.0f;                       // In bins
    float strength = 1.0f;                     // 0-1 normalized
    float falloff = 0.5f;                      // Edge falloff factor (0=hard, 1=soft)

    [[nodiscard]] bool isEmpty() const { return points.empty(); }

    // Get bounding region of brush stroke
    [[nodiscard]] SpectralRegion getBoundingRegion() const
    {
        if (points.empty())
            return {};

        int minFrame = points[0].first;
        int maxFrame = points[0].first;
        int minBin = points[0].second;
        int maxBin = points[0].second;

        for (const auto& p : points)
        {
            minFrame = std::min(minFrame, p.first);
            maxFrame = std::max(maxFrame, p.first);
            minBin = std::min(minBin, p.second);
            maxBin = std::max(maxBin, p.second);
        }

        int radiusInt = static_cast<int>(std::ceil(radius));
        return {
            minFrame - radiusInt,
            maxFrame + radiusInt + 1,
            minBin - radiusInt,
            maxBin + radiusInt + 1
        };
    }
};

// Single spectral edit operation
class SpectralEdit
{
public:
    // Create region-based edit
    SpectralEdit(SpectralOpType type, const SpectralRegion& region);

    // Create brush-based edit
    SpectralEdit(SpectralOpType type, const BrushStroke& stroke);

    // Parameter setters
    void setGainFactor(float factor) { gainFactor = factor; }
    void setGainDeltaDB(float deltaDB) { gainDeltaDB = deltaDB; }
    void setPhaseShift(float deltaPhi) { phaseShift = deltaPhi; }
    void setEraseFloor(float floorDB) { eraseFloorDB = floorDB; }
    void setReplacementData(
        const std::vector<std::vector<float>>& magnitudes,
        const std::vector<std::vector<float>>& phases);

    // Apply this edit to a single bin (magnitude in linear scale)
    void apply(float& magnitude, float& phase, int frame, int bin) const;

    // Check if edit affects a specific frame/bin
    [[nodiscard]] bool affects(int frame, int bin) const;

    // Get affected region
    [[nodiscard]] SpectralRegion getAffectedRegion() const;

    // Clone for undo system
    [[nodiscard]] std::unique_ptr<SpectralEdit> clone() const;

    // Accessors
    [[nodiscard]] SpectralOpType getType() const { return type; }
    [[nodiscard]] const SpectralRegion& getRegion() const { return region; }
    [[nodiscard]] bool isBrushBased() const { return brushStroke.has_value(); }
    [[nodiscard]] float getGainFactor() const { return gainFactor; }
    [[nodiscard]] float getGainDeltaDB() const { return gainDeltaDB; }

private:
    SpectralOpType type;
    SpectralRegion region;
    std::optional<BrushStroke> brushStroke;

    // Operation parameters
    float gainFactor = 1.0f;
    float gainDeltaDB = 0.0f;
    float phaseShift = 0.0f;
    float eraseFloorDB = -100.0f;

    // For Replace operation
    std::vector<std::vector<float>> replacementMagnitudes;
    std::vector<std::vector<float>> replacementPhases;

    // Get brush influence at a point (0-1)
    [[nodiscard]] float getBrushInfluence(int frame, int bin) const;
};

} // namespace spectralz
