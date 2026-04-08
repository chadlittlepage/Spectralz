#include "FFTProcessor.h"
#include <cmath>

namespace spectralz
{

FFTProcessor::FFTProcessor(int order)
    : fftOrder(order)
    , fftSize(1 << order)
    , hopSize(fftSize / 4)
    , fft(order)
{
    fftData.resize(fftSize * 2);
    workBuffer.resize(fftSize);
    createWindow();
}

void FFTProcessor::setFFTOrder(int order)
{
    if (order != fftOrder)
    {
        fftOrder = order;
        fftSize = 1 << order;
        hopSize = fftSize / 4;
        fft = juce::dsp::FFT(order);
        fftData.resize(fftSize * 2);
        workBuffer.resize(fftSize);
        createWindow();
    }
}

void FFTProcessor::setWindowType(WindowType type)
{
    if (type != windowType)
    {
        windowType = type;
        createWindow();
    }
}

void FFTProcessor::setHopSize(int newHopSize)
{
    hopSize = newHopSize;
}

void FFTProcessor::createWindow()
{
    window.resize(fftSize);

    const float n = static_cast<float>(fftSize);
    const float pi = juce::MathConstants<float>::pi;
    const float twoPi = juce::MathConstants<float>::twoPi;

    switch (windowType)
    {
        case WindowType::Hann:
            for (int i = 0; i < fftSize; ++i)
                window[i] = 0.5f * (1.0f - std::cos(twoPi * i / (n - 1)));
            break;

        case WindowType::Hamming:
            for (int i = 0; i < fftSize; ++i)
                window[i] = 0.54f - 0.46f * std::cos(twoPi * i / (n - 1));
            break;

        case WindowType::Blackman:
            for (int i = 0; i < fftSize; ++i)
            {
                float x = twoPi * i / (n - 1);
                window[i] = 0.42f - 0.5f * std::cos(x) + 0.08f * std::cos(2.0f * x);
            }
            break;

        case WindowType::BlackmanHarris:
            for (int i = 0; i < fftSize; ++i)
            {
                float x = twoPi * i / (n - 1);
                window[i] = 0.35875f - 0.48829f * std::cos(x)
                          + 0.14128f * std::cos(2.0f * x)
                          - 0.01168f * std::cos(3.0f * x);
            }
            break;

        case WindowType::FlatTop:
            for (int i = 0; i < fftSize; ++i)
            {
                float x = twoPi * i / (n - 1);
                window[i] = 0.21557895f - 0.41663158f * std::cos(x)
                          + 0.277263158f * std::cos(2.0f * x)
                          - 0.083578947f * std::cos(3.0f * x)
                          + 0.006947368f * std::cos(4.0f * x);
            }
            break;

        case WindowType::Rectangular:
        default:
            std::fill(window.begin(), window.end(), 1.0f);
            break;
    }
}

void FFTProcessor::applyWindow(float* data)
{
    for (int i = 0; i < fftSize; ++i)
        data[i] *= window[i];
}

void FFTProcessor::performFFT(const float* input, std::complex<float>* output)
{
    // Copy and window input
    std::copy(input, input + fftSize, fftData.begin());
    applyWindow(fftData.data());

    // Zero-pad the imaginary part
    std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);

    // Perform FFT (JUCE FFT operates in-place on interleaved real/imag data)
    fft.performRealOnlyForwardTransform(fftData.data());

    // Convert to complex output
    for (int i = 0; i <= fftSize / 2; ++i)
    {
        output[i] = std::complex<float>(fftData[i * 2], fftData[i * 2 + 1]);
    }
}

void FFTProcessor::performIFFT(const std::complex<float>* input, float* output)
{
    // Convert complex input to interleaved format
    for (int i = 0; i <= fftSize / 2; ++i)
    {
        fftData[i * 2] = input[i].real();
        fftData[i * 2 + 1] = input[i].imag();
    }

    // Perform inverse FFT
    fft.performRealOnlyInverseTransform(fftData.data());

    // Copy result
    std::copy(fftData.begin(), fftData.begin() + fftSize, output);
}

void FFTProcessor::computeMagnitudeSpectrum(const float* input, float* magnitudes)
{
    std::vector<std::complex<float>> spectrum(fftSize / 2 + 1);
    performFFT(input, spectrum.data());

    for (int i = 0; i <= fftSize / 2; ++i)
    {
        magnitudes[i] = std::abs(spectrum[i]);
    }
}

void FFTProcessor::computeMagnitudeSpectrumDB(const float* input, float* magnitudesDB, float minDB)
{
    std::vector<float> magnitudes(fftSize / 2 + 1);
    computeMagnitudeSpectrum(input, magnitudes.data());

    for (int i = 0; i <= fftSize / 2; ++i)
    {
        float mag = magnitudes[i];
        if (mag > 0.0f)
        {
            magnitudesDB[i] = 20.0f * std::log10(mag);
            if (magnitudesDB[i] < minDB)
                magnitudesDB[i] = minDB;
        }
        else
        {
            magnitudesDB[i] = minDB;
        }
    }
}

FFTProcessor::SpectrogramData FFTProcessor::computeSpectrogram(
    const juce::AudioBuffer<float>& buffer,
    int channel,
    double sampleRate)
{
    SpectrogramData result;
    result.sampleRate = sampleRate;
    result.hopSize = hopSize;
    result.fftSize = fftSize;
    result.numBins = fftSize / 2 + 1;

    const int numSamples = buffer.getNumSamples();
    const float* data = buffer.getReadPointer(channel);

    // Calculate number of frames
    result.numFrames = (numSamples - fftSize) / hopSize + 1;
    if (result.numFrames <= 0)
        return result;

    result.magnitudes.resize(result.numFrames);
    result.phases.resize(result.numFrames);

    std::vector<std::complex<float>> spectrum(result.numBins);
    std::vector<float> frameBuffer(fftSize);

    for (int frame = 0; frame < result.numFrames; ++frame)
    {
        int startSample = frame * hopSize;

        // Extract frame with zero-padding if necessary
        for (int i = 0; i < fftSize; ++i)
        {
            int sampleIdx = startSample + i;
            frameBuffer[i] = (sampleIdx < numSamples) ? data[sampleIdx] : 0.0f;
        }

        // Compute FFT
        performFFT(frameBuffer.data(), spectrum.data());

        // Store magnitudes and phases
        result.magnitudes[frame].resize(result.numBins);
        result.phases[frame].resize(result.numBins);

        for (int bin = 0; bin < result.numBins; ++bin)
        {
            result.magnitudes[frame][bin] = std::abs(spectrum[bin]);
            result.phases[frame][bin] = std::arg(spectrum[bin]);
        }
    }

    return result;
}

void FFTProcessor::synthesize(
    const SpectrogramData& spectrogram,
    juce::AudioBuffer<float>& output,
    int channel)
{
    const int outputSamples = (spectrogram.numFrames - 1) * spectrogram.hopSize + spectrogram.fftSize;
    output.setSize(std::max(output.getNumChannels(), channel + 1), outputSamples, true, true, true);
    output.clear(channel, 0, outputSamples);

    float* outputData = output.getWritePointer(channel);

    std::vector<std::complex<float>> spectrum(spectrogram.numBins);
    std::vector<float> frameBuffer(fftSize);

    // Compute normalization factor for overlap-add
    float windowSum = 0.0f;
    for (int i = 0; i < fftSize; ++i)
        windowSum += window[i] * window[i];
    float normFactor = spectrogram.hopSize / windowSum;

    for (int frame = 0; frame < spectrogram.numFrames; ++frame)
    {
        // Reconstruct complex spectrum
        for (int bin = 0; bin < spectrogram.numBins; ++bin)
        {
            float mag = spectrogram.magnitudes[frame][bin];
            float phase = spectrogram.phases[frame][bin];
            spectrum[bin] = std::polar(mag, phase);
        }

        // Perform inverse FFT
        performIFFT(spectrum.data(), frameBuffer.data());

        // Apply synthesis window and overlap-add
        int startSample = frame * spectrogram.hopSize;
        for (int i = 0; i < fftSize; ++i)
        {
            int sampleIdx = startSample + i;
            if (sampleIdx < outputSamples)
            {
                outputData[sampleIdx] += frameBuffer[i] * window[i] * normFactor;
            }
        }
    }
}

float FFTProcessor::binToFrequency(int bin, double sampleRate) const
{
    return static_cast<float>(bin * sampleRate / fftSize);
}

int FFTProcessor::frequencyToBin(float frequency, double sampleRate) const
{
    return static_cast<int>(std::round(frequency * fftSize / sampleRate));
}

double FFTProcessor::frameToTime(int frame, double sampleRate) const
{
    return frame * hopSize / sampleRate;
}

int FFTProcessor::timeToFrame(double time, double sampleRate) const
{
    return static_cast<int>(time * sampleRate / hopSize);
}

} // namespace spectralz
