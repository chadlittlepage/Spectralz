#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <complex>

namespace spectralz
{

enum class WindowType
{
    Hann,
    Hamming,
    Blackman,
    BlackmanHarris,
    FlatTop,
    Rectangular
};

class FFTProcessor
{
public:
    explicit FFTProcessor(int fftOrder = 11); // Default 2048 points
    ~FFTProcessor() = default;

    // Configuration
    void setFFTOrder(int order);
    void setWindowType(WindowType type);
    void setHopSize(int hopSize);

    [[nodiscard]] int getFFTSize() const { return fftSize; }
    [[nodiscard]] int getHopSize() const { return hopSize; }
    [[nodiscard]] int getFFTOrder() const { return fftOrder; }

    // Single-frame FFT
    void performFFT(const float* input, std::complex<float>* output);
    void performIFFT(const std::complex<float>* input, float* output);

    // Compute magnitude spectrum (for visualization)
    void computeMagnitudeSpectrum(const float* input, float* magnitudes);
    void computeMagnitudeSpectrumDB(const float* input, float* magnitudesDB, float minDB = -100.0f);

    // Full STFT analysis
    struct SpectrogramData
    {
        std::vector<std::vector<float>> magnitudes;  // [time][frequency]
        std::vector<std::vector<float>> phases;      // [time][frequency]
        int numFrames = 0;
        int numBins = 0;
        double sampleRate = 44100.0;
        int hopSize = 512;
        int fftSize = 2048;
    };

    [[nodiscard]] SpectrogramData computeSpectrogram(
        const juce::AudioBuffer<float>& buffer,
        int channel = 0,
        double sampleRate = 44100.0);

    // ISTFT synthesis (overlap-add)
    void synthesize(
        const SpectrogramData& spectrogram,
        juce::AudioBuffer<float>& output,
        int channel = 0);

    // Utility functions
    [[nodiscard]] float binToFrequency(int bin, double sampleRate) const;
    [[nodiscard]] int frequencyToBin(float frequency, double sampleRate) const;
    [[nodiscard]] double frameToTime(int frame, double sampleRate) const;
    [[nodiscard]] int timeToFrame(double time, double sampleRate) const;

    // Window access
    [[nodiscard]] const std::vector<float>& getWindow() const { return window; }

private:
    void createWindow();
    void applyWindow(float* data);

    int fftOrder;
    int fftSize;
    int hopSize;
    WindowType windowType = WindowType::Hann;

    std::vector<float> window;
    std::vector<float> fftData;
    std::vector<float> workBuffer;

    juce::dsp::FFT fft;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FFTProcessor)
};

} // namespace spectralz
