#include "PhaseVocoder.h"
#include <algorithm>
#include <cmath>

namespace spectralz
{

PhaseVocoder::PhaseVocoder()
{
    setParameters(2048, 512, 44100.0);
}

void PhaseVocoder::setParameters(int fftSizeParam, int hopSizeParam, double sampleRateParam)
{
    fftSize = fftSizeParam;
    hopSize = hopSizeParam;
    sampleRate = sampleRateParam;
    numBins = fftSize / 2 + 1;

    // Calculate expected phase increment per frame for each bin
    // The expected phase increment is 2*pi * bin * hopSize / fftSize
    expectedPhaseInc.resize(static_cast<size_t>(numBins));
    for (int bin = 0; bin < numBins; ++bin)
    {
        expectedPhaseInc[static_cast<size_t>(bin)] =
            TWO_PI * static_cast<float>(bin) * static_cast<float>(hopSize) / static_cast<float>(fftSize);
    }

    // Resize state arrays
    prevPhases.resize(static_cast<size_t>(numBins), 0.0f);
    prevMagnitudes.resize(static_cast<size_t>(numBins), 0.0f);

    reset();
}

void PhaseVocoder::reset()
{
    std::fill(prevPhases.begin(), prevPhases.end(), 0.0f);
    std::fill(prevMagnitudes.begin(), prevMagnitudes.end(), 0.0f);
    hasPrevFrame = false;
}

float PhaseVocoder::getExpectedFrequency(int bin) const
{
    return static_cast<float>(bin) * static_cast<float>(sampleRate) / static_cast<float>(fftSize);
}

float PhaseVocoder::unwrapPhase(float phase) const
{
    while (phase > PI) phase -= TWO_PI;
    while (phase < -PI) phase += TWO_PI;
    return phase;
}

float PhaseVocoder::computePhaseDeviation(float phase, float prevPhase, int bin) const
{
    // Expected phase increment for this bin
    float expected = expectedPhaseInc[static_cast<size_t>(bin)];

    // Actual phase difference
    float phaseDiff = phase - prevPhase;

    // Deviation from expected
    float deviation = phaseDiff - expected;

    // Wrap to [-pi, pi]
    return unwrapPhase(deviation);
}

std::vector<int> PhaseVocoder::findSpectralPeaks(const std::vector<float>& magnitudes) const
{
    std::vector<int> peaks;

    if (magnitudes.size() < 3)
        return peaks;

    // Find maximum magnitude for threshold
    float maxMag = *std::max_element(magnitudes.begin(), magnitudes.end());
    float threshold = maxMag * peakThreshold;

    // Find local maxima above threshold
    for (size_t i = 1; i < magnitudes.size() - 1; ++i)
    {
        if (magnitudes[i] > threshold &&
            magnitudes[i] >= magnitudes[i - 1] &&
            magnitudes[i] >= magnitudes[i + 1])
        {
            // Check minimum distance from last peak
            if (peaks.empty() ||
                static_cast<int>(i) - peaks.back() >= minPeakDistance)
            {
                peaks.push_back(static_cast<int>(i));
            }
            else if (magnitudes[i] > magnitudes[static_cast<size_t>(peaks.back())])
            {
                // Replace last peak if this one is stronger
                peaks.back() = static_cast<int>(i);
            }
        }
    }

    return peaks;
}

void PhaseVocoder::propagatePhases(std::vector<float>& phases,
                                   const std::vector<float>& prevPhasesLocal,
                                   const std::vector<float>& magnitudes)
{
    (void)magnitudes; // Not used in simple propagation

    for (int bin = 0; bin < numBins; ++bin)
    {
        size_t idx = static_cast<size_t>(bin);

        // Propagate phase based on expected increment
        float propagated = prevPhasesLocal[idx] + expectedPhaseInc[idx];
        phases[idx] = unwrapPhase(propagated);
    }
}

void PhaseVocoder::applyPeakLocking(std::vector<float>& phases,
                                    const std::vector<float>& magnitudes,
                                    const std::vector<int>& peaks)
{
    if (peaks.empty())
        return;

    // For each bin, find the nearest peak and lock phase to it
    for (int bin = 0; bin < numBins; ++bin)
    {
        // Find nearest peak
        int nearestPeak = peaks[0];
        int nearestDist = std::abs(bin - nearestPeak);

        for (int peak : peaks)
        {
            int dist = std::abs(bin - peak);
            if (dist < nearestDist)
            {
                nearestDist = dist;
                nearestPeak = peak;
            }
        }

        // Lock this bin's phase relative to the peak
        // The phase relationship should be maintained
        if (nearestDist > 0 && static_cast<size_t>(nearestPeak) < phases.size())
        {
            size_t peakIdx = static_cast<size_t>(nearestPeak);
            size_t binIdx = static_cast<size_t>(bin);

            // Calculate expected phase difference from peak
            float expectedDiff = expectedPhaseInc[binIdx] - expectedPhaseInc[peakIdx];

            // Set phase relative to peak
            phases[binIdx] = unwrapPhase(phases[peakIdx] + expectedDiff * static_cast<float>(nearestDist));
        }
    }
}

void PhaseVocoder::processFrame(std::vector<float>& magnitudes,
                                std::vector<float>& phases,
                                int frameIndex)
{
    (void)frameIndex; // Can be used for time-varying processing

    if (phaseLockMode == PhaseLockMode::None)
    {
        // No processing, just update state
        prevPhases = phases;
        prevMagnitudes = magnitudes;
        hasPrevFrame = true;
        return;
    }

    if (phaseLockMode == PhaseLockMode::Identity)
    {
        // Simple identity phase locking
        // Set phase to expected value based on bin frequency
        for (int bin = 0; bin < numBins; ++bin)
        {
            phases[static_cast<size_t>(bin)] = expectedPhaseInc[static_cast<size_t>(bin)] * static_cast<float>(frameIndex);
            phases[static_cast<size_t>(bin)] = unwrapPhase(phases[static_cast<size_t>(bin)]);
        }
    }
    else if (phaseLockMode == PhaseLockMode::PhaseGradient && hasPrevFrame)
    {
        // Propagate phases from previous frame
        propagatePhases(phases, prevPhases, magnitudes);
    }
    else if (phaseLockMode == PhaseLockMode::PeakLocking)
    {
        // Find peaks and lock phases to them
        auto peaks = findSpectralPeaks(magnitudes);

        if (hasPrevFrame)
        {
            // First propagate phases
            propagatePhases(phases, prevPhases, magnitudes);
        }

        // Then apply peak locking
        applyPeakLocking(phases, magnitudes, peaks);
    }

    // Update state
    prevPhases = phases;
    prevMagnitudes = magnitudes;
    hasPrevFrame = true;
}

void PhaseVocoder::process(FFTProcessor::SpectrogramData& data)
{
    reset();

    // Process each frame sequentially
    for (int frame = 0; frame < data.numFrames; ++frame)
    {
        processFrame(
            data.magnitudes[static_cast<size_t>(frame)],
            data.phases[static_cast<size_t>(frame)],
            frame);
    }
}

} // namespace spectralz
