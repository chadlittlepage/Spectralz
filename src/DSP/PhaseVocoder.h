#pragma once

#include "FFTProcessor.h"
#include <vector>
#include <cmath>

namespace spectralz
{

// Phase vocoder for maintaining phase coherence during spectral editing
// This helps prevent artifacts when magnitudes are modified
class PhaseVocoder
{
public:
    PhaseVocoder();
    ~PhaseVocoder() = default;

    // Configure vocoder parameters (must match FFT settings)
    void setParameters(int fftSize, int hopSize, double sampleRate);

    // Phase locking modes
    enum class PhaseLockMode
    {
        None,              // Use phases as-is (may have artifacts)
        Identity,          // Lock to identity phase (simple)
        PeakLocking,       // Lock to nearest spectral peak (best quality)
        PhaseGradient      // Propagate phase using frequency gradient
    };

    void setPhaseLockMode(PhaseLockMode mode) { phaseLockMode = mode; }
    [[nodiscard]] PhaseLockMode getPhaseLockMode() const { return phaseLockMode; }

    // Process edited spectrogram to improve phase coherence
    // This should be called after edits and before synthesis
    void process(FFTProcessor::SpectrogramData& data);

    // Process a single frame pair (for incremental processing)
    void processFrame(std::vector<float>& magnitudes,
                      std::vector<float>& phases,
                      int frameIndex);

    // Reset internal state (call when starting new audio)
    void reset();

    // Get expected frequency for each bin
    [[nodiscard]] float getExpectedFrequency(int bin) const;

private:
    // Find spectral peaks in magnitude array
    std::vector<int> findSpectralPeaks(const std::vector<float>& magnitudes) const;

    // Phase unwrapping utility
    [[nodiscard]] float unwrapPhase(float phase) const;

    // Compute phase deviation from expected
    [[nodiscard]] float computePhaseDeviation(float phase, float prevPhase, int bin) const;

    // Propagate phase from previous frame
    void propagatePhases(std::vector<float>& phases,
                        const std::vector<float>& prevPhases,
                        const std::vector<float>& magnitudes);

    // Apply peak-locked phase vocoder
    void applyPeakLocking(std::vector<float>& phases,
                         const std::vector<float>& magnitudes,
                         const std::vector<int>& peaks);

    // Parameters
    int fftSize = 2048;
    int hopSize = 512;
    double sampleRate = 44100.0;
    int numBins = 1025;  // fftSize/2 + 1

    PhaseLockMode phaseLockMode = PhaseLockMode::PeakLocking;

    // Expected phase increment per frame for each bin
    std::vector<float> expectedPhaseInc;

    // Previous frame data for phase propagation
    std::vector<float> prevPhases;
    std::vector<float> prevMagnitudes;
    bool hasPrevFrame = false;

    // Constants
    static constexpr float PI = 3.14159265358979323846f;
    static constexpr float TWO_PI = 2.0f * PI;

    // Peak detection threshold (relative to max magnitude)
    float peakThreshold = 0.01f;

    // Minimum distance between peaks (in bins)
    int minPeakDistance = 3;
};

} // namespace spectralz
