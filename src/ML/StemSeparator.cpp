#if SPECTRALZ_ENABLE_ML

#include "StemSeparator.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <complex>

namespace spectralz
{

// Factory function
std::unique_ptr<IStemSeparator> createStemSeparator(SeparationEngine engine)
{
    switch (engine)
    {
        case SeparationEngine::Demucs:
            return std::make_unique<DemucsEngine>();
        case SeparationEngine::Spleeter:
            return std::make_unique<SpleeterEngine>();
        case SeparationEngine::PythonDemucs:
            return std::make_unique<PythonDemucsEngine>();
        case SeparationEngine::PythonSpleeter:
            return std::make_unique<PythonSpleeterEngine>();
        default:
            return nullptr;
    }
}

// ============================================================================
// DemucsEngine Implementation
// ============================================================================

DemucsEngine::DemucsEngine()
    : inference(std::make_unique<ONNXInference>())
{
}

DemucsEngine::~DemucsEngine()
{
    unloadModel();
}

bool DemucsEngine::loadModel(const ModelInfo& model)
{
    DBG("DemucsEngine::loadModel ENTER - " << model.id);

    if (model.engine != SeparationEngine::Demucs)
    {
        DBG("DemucsEngine: Wrong engine type for model " << model.id);
        return false;
    }

    DBG("  Engine type: Demucs - OK");

    if (!model.modelFile.existsAsFile())
    {
        DBG("DemucsEngine: Model file not found: " << model.modelFile.getFullPathName());
        return false;
    }

    DBG("  Model file exists: " << model.modelFile.getFullPathName());
    DBG("  Calling inference->loadModel...");

    try
    {
        if (inference->loadModel(model.modelFile))
        {
            currentModel = model;
            modelLoaded = true;
            DBG("DemucsEngine: Loaded model " << model.displayName);
            return true;
        }
        else
        {
            DBG("DemucsEngine: inference->loadModel returned false");
            return false;
        }
    }
    catch (const std::exception& e)
    {
        DBG("DemucsEngine: Exception in loadModel: " << e.what());
        return false;
    }
    catch (...)
    {
        DBG("DemucsEngine: Unknown exception in loadModel");
        return false;
    }
}

void DemucsEngine::unloadModel()
{
    inference->unloadModel();
    modelLoaded = false;
    currentModel = {};
}

bool DemucsEngine::isModelLoaded() const
{
    return modelLoaded && inference->isModelLoaded();
}

std::vector<std::string> DemucsEngine::getAvailableStems() const
{
    if (modelLoaded)
        return currentModel.stems;
    return {};
}

SeparationResult DemucsEngine::separate(
    const juce::AudioBuffer<float>& input,
    double sampleRate,
    SeparationProgressCallback progressCallback,
    std::atomic<bool>* cancelFlag)
{
    DBG("DemucsEngine::separate() ENTER");
    DBG("  Input samples: " << input.getNumSamples() << ", channels: " << input.getNumChannels());
    DBG("  Sample rate: " << sampleRate);

    SeparationResult result;
    result.sampleRate = sampleRate;

    if (!isModelLoaded())
    {
        DBG("  ERROR: No model loaded");
        result.errorMessage = "No model loaded";
        return result;
    }
    DBG("  Model is loaded: " << currentModel.displayName);

    if (input.getNumSamples() == 0)
    {
        DBG("  ERROR: Empty input buffer");
        result.errorMessage = "Empty input buffer";
        return result;
    }

    // Check model input requirements - htdemucs expects 2 inputs
    auto inputNames = inference->getInputNames();
    auto outputNames = inference->getOutputNames();
    DBG("  Model expects " << inputNames.size() << " inputs:");
    for (const auto& name : inputNames)
        DBG("    - " << name);
    DBG("  Model has " << outputNames.size() << " outputs:");
    for (const auto& name : outputNames)
        DBG("    - " << name);

    // Support both 1-input (old format) and 2-input (htdemucs v4) models
    bool isTwoInputModel = (inputNames.size() == 2);
    if (inputNames.size() != 1 && inputNames.size() != 2)
    {
        DBG("  ERROR: Model has " << inputNames.size() << " inputs, expected 1 or 2");
        result.errorMessage = "Model input format not supported (expected 1 or 2 inputs, got " +
            std::to_string(inputNames.size()) + ")";
        return result;
    }

    // Resample to target sample rate if needed
    juce::AudioBuffer<float> processBuffer;

    if (std::abs(sampleRate - TARGET_SAMPLE_RATE) > 1.0)
    {
        if (progressCallback)
            progressCallback(0.0f, "Resampling audio...");

        processBuffer = resampleBuffer(input, sampleRate, TARGET_SAMPLE_RATE);
    }
    else
    {
        processBuffer = input;
    }

    // Check for cancellation
    if (cancelFlag && cancelFlag->load())
    {
        result.errorMessage = "Cancelled";
        return result;
    }

    const int totalSamples = processBuffer.getNumSamples();
    const int numChannels = processBuffer.getNumChannels();

    // Ensure stereo
    juce::AudioBuffer<float> stereoBuffer;
    if (numChannels == 1)
    {
        stereoBuffer.setSize(2, totalSamples);
        stereoBuffer.copyFrom(0, 0, processBuffer, 0, 0, totalSamples);
        stereoBuffer.copyFrom(1, 0, processBuffer, 0, 0, totalSamples);
    }
    else
    {
        stereoBuffer.setSize(2, totalSamples);
        stereoBuffer.copyFrom(0, 0, processBuffer, 0, 0, totalSamples);
        stereoBuffer.copyFrom(1, 0, processBuffer, 1, 0, totalSamples);
    }

    // Initialize output stems
    for (const auto& stemName : currentModel.stems)
    {
        result.stems[stemName].setSize(2, totalSamples);
        result.stems[stemName].clear();
    }

    // Process in chunks with overlap-add using correct segment size
    const int segmentSize = SEGMENT_SAMPLES;
    const int overlap = OVERLAP_SAMPLES;
    const int hopSize = segmentSize - overlap;

    int numChunks = (totalSamples + hopSize - 1) / hopSize;
    if (numChunks == 0) numChunks = 1;

    juce::AudioBuffer<float> chunkBuffer(2, segmentSize);
    std::vector<float> fadeIn(overlap), fadeOut(overlap);

    // Create crossfade windows
    for (int i = 0; i < overlap; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(overlap);
        fadeIn[i] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * t));
        fadeOut[i] = 0.5f * (1.0f + std::cos(juce::MathConstants<float>::pi * t));
    }

    // Create FFT for spectrogram computation (htdemucs uses nfft=4096, hop=1024)
    // PyTorch STFT parameters: normalized=True, center=True, pad_mode='reflect'
    juce::dsp::FFT fft(static_cast<int>(std::log2(STFT_NFFT)));

    // Create periodic Hann window (PyTorch default)
    std::vector<float> hannWindow(STFT_NFFT);
    for (int i = 0; i < STFT_NFFT; ++i)
    {
        hannWindow[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / STFT_NFFT));
    }

    // Normalization factor for PyTorch's normalized=True STFT
    const float stftNormFactor = 1.0f / std::sqrt(static_cast<float>(STFT_NFFT));

    for (int chunk = 0; chunk < numChunks; ++chunk)
    {
        if (cancelFlag && cancelFlag->load())
        {
            result.errorMessage = "Cancelled";
            return result;
        }

        float progress = static_cast<float>(chunk) / static_cast<float>(numChunks);
        if (progressCallback)
            progressCallback(progress, "Processing chunk " + std::to_string(chunk + 1) + "/" + std::to_string(numChunks));

        int startSample = chunk * hopSize;
        int endSample = std::min(startSample + segmentSize, totalSamples);
        int chunkSamples = endSample - startSample;

        // Copy chunk with zero padding if needed
        chunkBuffer.clear();
        for (int ch = 0; ch < 2; ++ch)
        {
            chunkBuffer.copyFrom(ch, 0, stereoBuffer, ch, startSample, chunkSamples);
        }

        // =============================================================
        // DEMUCS PREPROCESSING - EXACTLY MATCHING PYTHON IMPLEMENTATION
        // =============================================================
        // From htdemucs.py forward():
        //   1. Compute STFT with center=True, normalized=True, pad_mode='reflect'
        //   2. Normalize spectrogram: x = (x - mean) / (1e-5 + std) over dims (1,2,3)
        //   3. Normalize waveform: xt = (xt - meant) / (1e-5 + stdt) over dims (1,2)
        // =============================================================

        InferenceResult inferResult;

        if (isTwoInputModel)
        {
            // htdemucs v4 format: 2 inputs
            // Input 1: "input" - waveform [1, 2, SEGMENT_SAMPLES]
            // Input 2: "x" - spectrogram [1, 4, STFT_FREQ_BINS, STFT_TIME_FRAMES] (complex-as-channels)

            // =====================================================
            // STEP 1: Compute STFT exactly as Demucs does
            // PyTorch STFT: normalized=True, center=True, pad_mode='reflect'
            // =====================================================

            // For center=True, we need to pad with n_fft//2 on each side using reflect padding
            const int halfNfft = STFT_NFFT / 2;  // 2048
            const int paddedLength = segmentSize + STFT_NFFT;  // Add n_fft//2 on each side

            // Create padded buffers with reflect padding
            std::vector<float> paddedLeft(paddedLength);
            std::vector<float> paddedRight(paddedLength);

            const float* leftChannel = chunkBuffer.getReadPointer(0);
            const float* rightChannel = chunkBuffer.getReadPointer(1);

            // DEBUG: Print first 10 raw waveform samples for comparison with Python
            if (chunk == 0)
            {
                DBG("=== DEBUG: C++ vs Python comparison ===");
                DBG("Left channel first 10 samples:");
                for (int i = 0; i < 10 && i < segmentSize; ++i)
                    DBG("  [" << i << "] = " << leftChannel[i]);
                DBG("Right channel first 10 samples:");
                for (int i = 0; i < 10 && i < segmentSize; ++i)
                    DBG("  [" << i << "] = " << rightChannel[i]);
            }

            // Reflect padding at the beginning (n_fft//2 samples)
            for (int i = 0; i < halfNfft; ++i)
            {
                int reflectIdx = halfNfft - i;  // Reflect around index 0
                if (reflectIdx >= segmentSize) reflectIdx = segmentSize - 1;
                paddedLeft[i] = leftChannel[reflectIdx];
                paddedRight[i] = rightChannel[reflectIdx];
            }

            // Copy original signal
            for (int i = 0; i < segmentSize; ++i)
            {
                paddedLeft[halfNfft + i] = leftChannel[i];
                paddedRight[halfNfft + i] = rightChannel[i];
            }

            // Reflect padding at the end (n_fft//2 samples)
            for (int i = 0; i < halfNfft; ++i)
            {
                int reflectIdx = segmentSize - 2 - i;  // Reflect around last sample
                if (reflectIdx < 0) reflectIdx = 0;
                paddedLeft[halfNfft + segmentSize + i] = leftChannel[reflectIdx];
                paddedRight[halfNfft + segmentSize + i] = rightChannel[reflectIdx];
            }

            // Calculate number of STFT frames
            const int numFrames = (paddedLength - STFT_NFFT) / STFT_HOP + 1;
            DBG("  STFT: paddedLength=" << paddedLength << ", numFrames=" << numFrames << " (expected " << STFT_TIME_FRAMES << ")");

            // Prepare spectrogram output in complex-as-channels format
            // Shape: [1, 4, STFT_FREQ_BINS, STFT_TIME_FRAMES]
            // Layout: [left_real, left_imag, right_real, right_imag]
            std::vector<float> spectrogramData(4 * STFT_FREQ_BINS * STFT_TIME_FRAMES, 0.0f);

            // FFT buffer for JUCE FFT (interleaved real/imag)
            std::vector<float> fftBuffer(STFT_NFFT * 2);

            // Process each frame
            for (int frame = 0; frame < std::min(numFrames, STFT_TIME_FRAMES); ++frame)
            {
                int startIdx = frame * STFT_HOP;

                // Left channel STFT
                std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
                for (int i = 0; i < STFT_NFFT; ++i)
                {
                    if (startIdx + i < paddedLength)
                    {
                        fftBuffer[i] = paddedLeft[startIdx + i] * hannWindow[i];
                    }
                }
                fft.performRealOnlyForwardTransform(fftBuffer.data(), true);

                // Store left channel complex values (apply normalized=True: divide by sqrt(n_fft))
                for (int bin = 0; bin < STFT_FREQ_BINS; ++bin)
                {
                    // JUCE FFT: DC at index 0, then complex pairs
                    // For bin 0: DC component (real only, imag = 0)
                    // For bin > 0: [bin*2] = real, [bin*2+1] = imag
                    float real, imag;
                    if (bin == 0)
                    {
                        real = fftBuffer[0] * stftNormFactor;
                        imag = 0.0f;
                    }
                    else if (bin < STFT_NFFT / 2)
                    {
                        real = fftBuffer[bin * 2] * stftNormFactor;
                        imag = fftBuffer[bin * 2 + 1] * stftNormFactor;
                    }
                    else
                    {
                        // Nyquist bin
                        real = fftBuffer[1] * stftNormFactor;  // JUCE stores Nyquist at index 1
                        imag = 0.0f;
                    }

                    // Store in cac format: [left_real, left_imag, right_real, right_imag]
                    // Index = channel * (FREQ * TIME) + bin * TIME + frame
                    int leftRealIdx = 0 * STFT_FREQ_BINS * STFT_TIME_FRAMES + bin * STFT_TIME_FRAMES + frame;
                    int leftImagIdx = 1 * STFT_FREQ_BINS * STFT_TIME_FRAMES + bin * STFT_TIME_FRAMES + frame;
                    spectrogramData[leftRealIdx] = real;
                    spectrogramData[leftImagIdx] = imag;
                }

                // Right channel STFT
                std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
                for (int i = 0; i < STFT_NFFT; ++i)
                {
                    if (startIdx + i < paddedLength)
                    {
                        fftBuffer[i] = paddedRight[startIdx + i] * hannWindow[i];
                    }
                }
                fft.performRealOnlyForwardTransform(fftBuffer.data(), true);

                // Store right channel complex values
                for (int bin = 0; bin < STFT_FREQ_BINS; ++bin)
                {
                    float real, imag;
                    if (bin == 0)
                    {
                        real = fftBuffer[0] * stftNormFactor;
                        imag = 0.0f;
                    }
                    else if (bin < STFT_NFFT / 2)
                    {
                        real = fftBuffer[bin * 2] * stftNormFactor;
                        imag = fftBuffer[bin * 2 + 1] * stftNormFactor;
                    }
                    else
                    {
                        real = fftBuffer[1] * stftNormFactor;
                        imag = 0.0f;
                    }

                    int rightRealIdx = 2 * STFT_FREQ_BINS * STFT_TIME_FRAMES + bin * STFT_TIME_FRAMES + frame;
                    int rightImagIdx = 3 * STFT_FREQ_BINS * STFT_TIME_FRAMES + bin * STFT_TIME_FRAMES + frame;
                    spectrogramData[rightRealIdx] = real;
                    spectrogramData[rightImagIdx] = imag;
                }

                // DEBUG: Print first frame STFT values
                if (chunk == 0 && frame == 0)
                {
                    DBG("STFT first frame, first 5 bins (left channel):");
                    for (int bin = 0; bin < 5; ++bin)
                    {
                        int leftRealIdx = 0 * STFT_FREQ_BINS * STFT_TIME_FRAMES + bin * STFT_TIME_FRAMES + 0;
                        int leftImagIdx = 1 * STFT_FREQ_BINS * STFT_TIME_FRAMES + bin * STFT_TIME_FRAMES + 0;
                        DBG("  bin[" << bin << "] real=" << spectrogramData[leftRealIdx] << ", imag=" << spectrogramData[leftImagIdx]);
                    }
                    DBG("STFT first frame, first 5 bins (right channel):");
                    for (int bin = 0; bin < 5; ++bin)
                    {
                        int rightRealIdx = 2 * STFT_FREQ_BINS * STFT_TIME_FRAMES + bin * STFT_TIME_FRAMES + 0;
                        int rightImagIdx = 3 * STFT_FREQ_BINS * STFT_TIME_FRAMES + bin * STFT_TIME_FRAMES + 0;
                        DBG("  bin[" << bin << "] real=" << spectrogramData[rightRealIdx] << ", imag=" << spectrogramData[rightImagIdx]);
                    }
                }
            }

            // =====================================================
            // STEP 2: Normalize SPECTROGRAM over dims (1,2,3)
            // This matches: mean = x.mean(dim=(1, 2, 3), keepdim=True)
            // =====================================================
            float specSum = 0.0f;
            float specSumSq = 0.0f;
            const size_t specSize = spectrogramData.size();
            for (size_t i = 0; i < specSize; ++i)
            {
                specSum += spectrogramData[i];
                specSumSq += spectrogramData[i] * spectrogramData[i];
            }
            float specMean = specSum / static_cast<float>(specSize);
            float specVariance = (specSumSq / static_cast<float>(specSize)) - (specMean * specMean);
            float specStd = std::sqrt(std::max(specVariance, 0.0f));

            // Apply spectrogram normalization
            for (size_t i = 0; i < specSize; ++i)
            {
                spectrogramData[i] = (spectrogramData[i] - specMean) / (1e-5f + specStd);
            }

            DBG("  Spectrogram normalization: mean=" << specMean << ", std=" << specStd);

            // DEBUG: Print normalized CAC values at [0, 0, :5, 0]
            if (chunk == 0)
            {
                DBG("Normalized CAC [0, 0, :5, 0] (left_real first frame):");
                for (int bin = 0; bin < 5; ++bin)
                {
                    int idx = 0 * STFT_FREQ_BINS * STFT_TIME_FRAMES + bin * STFT_TIME_FRAMES + 0;
                    DBG("  [" << bin << "] = " << spectrogramData[idx]);
                }
                DBG("Normalized CAC [0, 1, :5, 0] (left_imag first frame):");
                for (int bin = 0; bin < 5; ++bin)
                {
                    int idx = 1 * STFT_FREQ_BINS * STFT_TIME_FRAMES + bin * STFT_TIME_FRAMES + 0;
                    DBG("  [" << bin << "] = " << spectrogramData[idx]);
                }
            }

            // =====================================================
            // STEP 3: Normalize WAVEFORM over dims (1,2)
            // This matches: meant = xt.mean(dim=(1, 2), keepdim=True)
            // =====================================================
            float waveSum = 0.0f;
            float waveSumSq = 0.0f;
            const size_t waveSize = 2 * segmentSize;  // Both channels
            for (int ch = 0; ch < 2; ++ch)
            {
                const float* data = chunkBuffer.getReadPointer(ch);
                for (int i = 0; i < segmentSize; ++i)
                {
                    waveSum += data[i];
                    waveSumSq += data[i] * data[i];
                }
            }
            float waveMean = waveSum / static_cast<float>(waveSize);
            float waveVariance = (waveSumSq / static_cast<float>(waveSize)) - (waveMean * waveMean);
            float waveStd = std::sqrt(std::max(waveVariance, 0.0f));

            // Prepare normalized waveform input [1, 2, SEGMENT_SAMPLES]
            std::vector<float> waveformData(2 * segmentSize);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float* src = chunkBuffer.getReadPointer(ch);
                for (int i = 0; i < segmentSize; ++i)
                {
                    waveformData[ch * segmentSize + i] = (src[i] - waveMean) / (1e-5f + waveStd);
                }
            }

            DBG("  Waveform normalization: mean=" << waveMean << ", std=" << waveStd);

            // DEBUG: Print first 10 normalized waveform samples
            if (chunk == 0)
            {
                DBG("Normalized waveform first 10 samples (left):");
                for (int i = 0; i < 10 && i < segmentSize; ++i)
                    DBG("  [" << i << "] = " << waveformData[i]);
                DBG("Normalized waveform first 10 samples (right):");
                for (int i = 0; i < 10 && i < segmentSize; ++i)
                    DBG("  [" << i << "] = " << waveformData[segmentSize + i]);
            }

            // Create input shapes
            TensorShape waveformShape;
            waveformShape.dims = {1, 2, static_cast<int64_t>(segmentSize)};

            TensorShape spectrogramShape;
            spectrogramShape.dims = {1, 4, static_cast<int64_t>(STFT_FREQ_BINS), static_cast<int64_t>(STFT_TIME_FRAMES)};

            // Run inference with both inputs
            std::vector<std::vector<float>> inputs = {waveformData, spectrogramData};
            std::vector<TensorShape> shapes = {waveformShape, spectrogramShape};

            DBG("  Running 2-input inference...");
            DBG("    Waveform shape: [1, 2, " << segmentSize << "], size=" << waveformData.size());
            DBG("    Spectrogram shape: [1, 4, " << STFT_FREQ_BINS << ", " << STFT_TIME_FRAMES << "], size=" << spectrogramData.size());

            inferResult = inference->run(inputs, shapes);
        }
        else
        {
            // Single input format (legacy) - no normalization applied
            TensorShape inputShape;
            inputShape.dims = {1, 2, static_cast<int64_t>(segmentSize)};

            std::vector<float> inputData(2 * segmentSize);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float* src = chunkBuffer.getReadPointer(ch);
                std::copy(src, src + segmentSize, inputData.begin() + ch * segmentSize);
            }

            inferResult = inference->run(inputData, inputShape);
        }

        // Compute waveform normalization params for denormalization (used for both branches)
        float waveSum = 0.0f;
        float waveSumSq = 0.0f;
        const size_t waveSize = 2 * segmentSize;
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* data = chunkBuffer.getReadPointer(ch);
            for (int i = 0; i < segmentSize; ++i)
            {
                waveSum += data[i];
                waveSumSq += data[i] * data[i];
            }
        }
        float mean = waveSum / static_cast<float>(waveSize);
        float variance = (waveSumSq / static_cast<float>(waveSize)) - (mean * mean);
        float std = std::sqrt(std::max(variance, 0.0f));

        if (!inferResult.success)
        {
            result.errorMessage = "Inference failed: " + inferResult.errorMessage;
            return result;
        }

        // Parse outputs - for 2-input model, use second output (time domain)
        // Output "add_67": [1, 4, 2, SEGMENT_SAMPLES] (4 stems, stereo)
        if (inferResult.outputs.empty())
        {
            result.errorMessage = "No output from model";
            return result;
        }

        // For 2-input model, prefer the time-domain output (usually second output)
        size_t outputIdx = 0;
        if (isTwoInputModel && inferResult.outputs.size() > 1)
        {
            // Second output is time-domain stems
            outputIdx = 1;
            DBG("  Using output " << outputIdx << " (time-domain stems)");
        }

        const auto& outputData = inferResult.outputs[outputIdx];
        const auto& outShape = inferResult.outputShapes[outputIdx];

        DBG("  Output " << outputIdx << " size: " << outputData.size());
        DBG("  Output shape: [");
        for (size_t d = 0; d < outShape.dims.size(); ++d)
        {
            if (d > 0) DBG(", ");
            DBG(outShape.dims[d]);
        }
        DBG("]");

        const size_t numStems = currentModel.stems.size();

        // Extract each stem and add to result with overlap
        // Output format: [1, num_stems, 2, segment_samples]
        for (size_t s = 0; s < numStems && s < currentModel.stems.size(); ++s)
        {
            const std::string& stemName = currentModel.stems[s];
            auto& stemBuffer = result.stems[stemName];

            for (int ch = 0; ch < 2; ++ch)
            {
                // Index into output: stem * (2 * segmentSize) + channel * segmentSize + sample
                size_t stemOffset = s * 2 * segmentSize + ch * segmentSize;

                // Apply crossfade for overlap regions
                for (int i = 0; i < chunkSamples; ++i)
                {
                    int outputSampleIdx = startSample + i;
                    if (outputSampleIdx >= totalSamples) break;

                    size_t dataIdx = stemOffset + i;
                    float sample = (dataIdx < outputData.size()) ? outputData[dataIdx] : 0.0f;

                    // Denormalize output for HTDemucs models
                    // The ONNX model outputs normalized values that need denormalization.
                    // Based on comparison with terminal demucs, there's a ~2.54x amplitude difference
                    // due to overlap-add windowing and reference normalization in the full demucs pipeline.
                    // The formula is: x = x * (1e-5 + std) + mean, then scale by correction factor.
                    if (isTwoInputModel)
                    {
                        // Standard denormalization
                        sample = sample * (1e-5f + std) + mean;

                        // Amplitude correction to match terminal demucs output level
                        // This factor accounts for differences in overlap-add processing
                        constexpr float HTDEMUCS_AMPLITUDE_CORRECTION = 2.54f;
                        sample *= HTDEMUCS_AMPLITUDE_CORRECTION;
                    }

                    // Apply fade-in at chunk start (except first chunk)
                    if (chunk > 0 && i < overlap)
                    {
                        sample *= fadeIn[i];
                        sample += stemBuffer.getSample(ch, outputSampleIdx) * fadeOut[i];
                    }

                    stemBuffer.setSample(ch, outputSampleIdx, sample);
                }
            }
        }
    }

    // Resample back to original sample rate if needed
    if (std::abs(sampleRate - TARGET_SAMPLE_RATE) > 1.0)
    {
        if (progressCallback)
            progressCallback(0.95f, "Resampling output...");

        for (auto& [stemName, stemBuffer] : result.stems)
        {
            result.stems[stemName] = resampleBuffer(stemBuffer, TARGET_SAMPLE_RATE, sampleRate);
        }
    }

    result.success = true;
    result.sampleRate = sampleRate;

    if (progressCallback)
        progressCallback(1.0f, "Complete");

    return result;
}

juce::AudioBuffer<float> DemucsEngine::resampleBuffer(
    const juce::AudioBuffer<float>& input,
    double sourceSampleRate,
    double targetSampleRate)
{
    if (std::abs(sourceSampleRate - targetSampleRate) < 1.0)
        return input;

    double ratio = targetSampleRate / sourceSampleRate;
    int newNumSamples = static_cast<int>(input.getNumSamples() * ratio);
    int numChannels = input.getNumChannels();

    juce::AudioBuffer<float> output(numChannels, newNumSamples);

    // Simple linear interpolation resampling
    // For production, consider using a proper resampler like libsamplerate
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = input.getReadPointer(ch);
        float* dst = output.getWritePointer(ch);

        for (int i = 0; i < newNumSamples; ++i)
        {
            double srcIdx = i / ratio;
            int idx0 = static_cast<int>(srcIdx);
            int idx1 = std::min(idx0 + 1, input.getNumSamples() - 1);
            float frac = static_cast<float>(srcIdx - idx0);

            dst[i] = src[idx0] * (1.0f - frac) + src[idx1] * frac;
        }
    }

    return output;
}

SeparationResult DemucsEngine::processChunk(
    const juce::AudioBuffer<float>& chunk,
    double sampleRate)
{
    // This is now integrated into the main separate() method
    SeparationResult result;
    result.success = false;
    result.errorMessage = "Use separate() method instead";
    return result;
}

void DemucsEngine::computeDemucsSTFT(
    const juce::AudioBuffer<float>& stereoInput,
    std::vector<float>& spectrogramOutput)
{
    // STFT computation is now done inline in separate() for efficiency
    // This method is kept for API compatibility
    spectrogramOutput.resize(4 * STFT_FREQ_BINS * STFT_TIME_FRAMES, 0.0f);
}

// ============================================================================
// SpleeterEngine Implementation
// ============================================================================

SpleeterEngine::SpleeterEngine()
    : inference(std::make_unique<ONNXInference>())
{
}

SpleeterEngine::~SpleeterEngine()
{
    unloadModel();
}

bool SpleeterEngine::loadModel(const ModelInfo& model)
{
    if (model.engine != SeparationEngine::Spleeter)
    {
        DBG("SpleeterEngine: Wrong engine type for model " << model.id);
        return false;
    }

    // Handle multi-file models (one .onnx per stem)
    if (model.usesMultipleFiles)
    {
        if (!model.modelDir.isDirectory())
        {
            DBG("SpleeterEngine: Model directory not found: " << model.modelDir.getFullPathName());
            return false;
        }

        // Unload any existing models
        unloadModel();

        // Load each stem model
        bool allLoaded = true;
        for (const auto& stemName : model.stems)
        {
            auto stemFile = model.getStemModelFile(stemName);
            if (!stemFile.existsAsFile())
            {
                DBG("SpleeterEngine: Stem model file not found: " << stemFile.getFullPathName());
                allLoaded = false;
                break;
            }

            auto stemInference = std::make_unique<ONNXInference>();
            if (!stemInference->loadModel(stemFile))
            {
                DBG("SpleeterEngine: Failed to load stem model: " << stemName);
                allLoaded = false;
                break;
            }

            DBG("SpleeterEngine: Loaded stem model: " << stemName << " from " << stemFile.getFileName());
            stemInferences[stemName] = std::move(stemInference);
        }

        if (!allLoaded)
        {
            stemInferences.clear();
            return false;
        }

        currentModel = model;
        modelLoaded = true;
        DBG("SpleeterEngine: Loaded multi-file model " << model.displayName << " with " << model.stems.size() << " stems");
        return true;
    }

    // Handle single-file models (legacy)
    if (!model.modelFile.existsAsFile())
    {
        DBG("SpleeterEngine: Model file not found: " << model.modelFile.getFullPathName());
        return false;
    }

    if (inference->loadModel(model.modelFile))
    {
        currentModel = model;
        modelLoaded = true;
        DBG("SpleeterEngine: Loaded single-file model " << model.displayName);
        return true;
    }

    return false;
}

void SpleeterEngine::unloadModel()
{
    // Unload multi-file models
    stemInferences.clear();

    // Unload single-file model
    if (inference)
        inference->unloadModel();

    modelLoaded = false;
    currentModel = {};
}

bool SpleeterEngine::isModelLoaded() const
{
    if (!modelLoaded)
        return false;

    // For multi-file models, check all stem inferences are loaded
    if (currentModel.usesMultipleFiles)
    {
        if (stemInferences.empty())
            return false;
        for (const auto& [name, inf] : stemInferences)
        {
            if (!inf || !inf->isModelLoaded())
                return false;
        }
        return true;
    }

    // For single-file models
    return inference && inference->isModelLoaded();
}

InferenceResult SpleeterEngine::runStemInference(
    const std::string& stemName,
    const std::vector<float>& inputData,
    const TensorShape& inputShape)
{
    DBG("runStemInference() ENTER - stem: " << stemName);
    DBG("  Input data size: " << inputData.size());
    DBG("  Input shape dims: " << inputShape.dims.size());

    auto it = stemInferences.find(stemName);
    if (it == stemInferences.end() || !it->second)
    {
        DBG("  ERROR: Stem inference not found!");
        InferenceResult result;
        result.success = false;
        result.errorMessage = "Stem inference not found: " + stemName;
        return result;
    }

    DBG("  Found stem inference, calling run()...");
    std::cerr << std::flush;  // Flush before potentially crashing call

    // Check if inference object is valid
    if (!it->second->isModelLoaded())
    {
        DBG("  ERROR: Model not loaded in inference object!");
        InferenceResult result;
        result.success = false;
        result.errorMessage = "Stem model not loaded: " + stemName;
        return result;
    }

    DBG("  Model is loaded, about to call run()");
    std::cerr << std::flush;

    try
    {
        auto result = it->second->run(inputData, inputShape);
        DBG("  run() returned, success=" << (result.success ? "yes" : "no"));
        return result;
    }
    catch (const std::exception& e)
    {
        DBG("  EXCEPTION in run(): " << e.what());
        InferenceResult result;
        result.success = false;
        result.errorMessage = std::string("Exception during inference: ") + e.what();
        return result;
    }
    catch (...)
    {
        DBG("  UNKNOWN EXCEPTION in run()");
        InferenceResult result;
        result.success = false;
        result.errorMessage = "Unknown exception during inference";
        return result;
    }
}

std::vector<std::string> SpleeterEngine::getAvailableStems() const
{
    if (modelLoaded)
        return currentModel.stems;
    return {};
}

SeparationResult SpleeterEngine::separate(
    const juce::AudioBuffer<float>& input,
    double sampleRate,
    SeparationProgressCallback progressCallback,
    std::atomic<bool>* cancelFlag)
{
    DBG("SpleeterEngine::separate() ENTER");
    DBG("  Input samples: " << input.getNumSamples() << ", channels: " << input.getNumChannels());
    DBG("  Sample rate: " << sampleRate);
    DBG("  Multi-file model: " << (currentModel.usesMultipleFiles ? "yes" : "no"));

    SeparationResult result;
    result.sampleRate = sampleRate;

    if (!isModelLoaded())
    {
        DBG("  ERROR: No model loaded");
        result.errorMessage = "No model loaded";
        return result;
    }

    if (input.getNumSamples() == 0)
    {
        DBG("  ERROR: Empty input buffer");
        result.errorMessage = "Empty input buffer";
        return result;
    }

    if (cancelFlag && cancelFlag->load())
    {
        result.errorMessage = "Cancelled";
        return result;
    }

    if (progressCallback)
        progressCallback(0.05f, "Preparing input...");

    const int totalSamples = input.getNumSamples();
    const int numChannels = std::min(input.getNumChannels(), 2);

    // Ensure stereo
    juce::AudioBuffer<float> stereoBuffer;
    if (numChannels == 1)
    {
        stereoBuffer.setSize(2, totalSamples);
        stereoBuffer.copyFrom(0, 0, input, 0, 0, totalSamples);
        stereoBuffer.copyFrom(1, 0, input, 0, 0, totalSamples);
    }
    else
    {
        stereoBuffer.setSize(2, totalSamples);
        stereoBuffer.copyFrom(0, 0, input, 0, 0, totalSamples);
        stereoBuffer.copyFrom(1, 0, input, 1, 0, totalSamples);
    }

    // Model expects input shape: [2, num_splits, 512, 1024]
    // This is spectrogram format: 2 channels, variable splits, 512 freq bins, 1024 time frames
    const int fftSize = STFT_FRAME_LENGTH;  // 1024
    const int hopSize = STFT_HOP_LENGTH;    // 512
    const int numBins = SPEC_FREQ_BINS;     // 512 (fftSize/2)
    const int chunkTimeFrames = SPEC_TIME_FRAMES;  // 1024 time frames per chunk

    // Compute total STFT frames
    const int totalFrames = (totalSamples - fftSize) / hopSize + 1;
    const int numChunks = (totalFrames + chunkTimeFrames - 1) / chunkTimeFrames;

    DBG("  FFT size: " << fftSize << ", hop: " << hopSize);
    DBG("  Total STFT frames: " << totalFrames);
    DBG("  Processing in " << numChunks << " chunks of " << chunkTimeFrames << " frames");

    if (progressCallback)
        progressCallback(0.1f, "Computing STFT...");

    // Compute Hann window
    std::vector<float> window(fftSize);
    for (int i = 0; i < fftSize; ++i)
    {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));
    }

    juce::dsp::FFT fft(static_cast<int>(std::log2(fftSize)));

    // Compute full STFT for both channels
    std::vector<std::vector<std::complex<float>>> stftLeft(totalFrames);
    std::vector<std::vector<std::complex<float>>> stftRight(totalFrames);

    for (int frame = 0; frame < totalFrames; ++frame)
    {
        int startIdx = frame * hopSize;
        stftLeft[frame].resize(numBins);
        stftRight[frame].resize(numBins);

        // Left channel
        std::vector<float> frameDataL(fftSize * 2, 0.0f);
        for (int i = 0; i < fftSize && (startIdx + i) < totalSamples; ++i)
        {
            frameDataL[i] = stereoBuffer.getSample(0, startIdx + i) * window[i];
        }
        fft.performRealOnlyForwardTransform(frameDataL.data(), true);
        for (int bin = 0; bin < numBins; ++bin)
        {
            stftLeft[frame][bin] = std::complex<float>(frameDataL[bin * 2], frameDataL[bin * 2 + 1]);
        }

        // Right channel
        std::vector<float> frameDataR(fftSize * 2, 0.0f);
        for (int i = 0; i < fftSize && (startIdx + i) < totalSamples; ++i)
        {
            frameDataR[i] = stereoBuffer.getSample(1, startIdx + i) * window[i];
        }
        fft.performRealOnlyForwardTransform(frameDataR.data(), true);
        for (int bin = 0; bin < numBins; ++bin)
        {
            stftRight[frame][bin] = std::complex<float>(frameDataR[bin * 2], frameDataR[bin * 2 + 1]);
        }
    }

    if (cancelFlag && cancelFlag->load())
    {
        result.errorMessage = "Cancelled";
        return result;
    }

    // Initialize output stem buffers
    const size_t numStems = currentModel.stems.size();
    for (const auto& stemName : currentModel.stems)
    {
        result.stems[stemName].setSize(2, totalSamples);
        result.stems[stemName].clear();
    }

    // Process each stem
    for (size_t stemIdx = 0; stemIdx < numStems; ++stemIdx)
    {
        const std::string& stemName = currentModel.stems[stemIdx];
        DBG("  Processing stem: " << stemName);

        if (cancelFlag && cancelFlag->load())
        {
            result.errorMessage = "Cancelled";
            return result;
        }

        float baseProgress = 0.2f + (0.7f * static_cast<float>(stemIdx) / static_cast<float>(numStems));
        if (progressCallback)
            progressCallback(baseProgress, "Processing: " + stemName);

        DBG("    Allocating masked STFT vectors for " << totalFrames << " frames");
        // Store masked STFT for this stem
        std::vector<std::vector<std::complex<float>>> maskedLeft(totalFrames);
        std::vector<std::vector<std::complex<float>>> maskedRight(totalFrames);
        for (int f = 0; f < totalFrames; ++f)
        {
            maskedLeft[f].resize(numBins, std::complex<float>(0, 0));
            maskedRight[f].resize(numBins, std::complex<float>(0, 0));
        }
        DBG("    Masked STFT vectors allocated");

        // Process in chunks
        for (int chunk = 0; chunk < numChunks; ++chunk)
        {
            DBG("    Processing chunk " << chunk << "/" << numChunks);
            int frameStart = chunk * chunkTimeFrames;
            int frameEnd = std::min(frameStart + chunkTimeFrames, totalFrames);
            int framesInChunk = frameEnd - frameStart;

            // Prepare input data: [2, 1, 512, 1024]
            // Shape: [channels=2, splits=1, freq_bins=512, time_frames=1024]
            TensorShape inputShape;
            inputShape.dims = {2, 1, static_cast<int64_t>(numBins), static_cast<int64_t>(chunkTimeFrames)};

            DBG("    Input shape: [2, 1, " << numBins << ", " << chunkTimeFrames << "]");
            std::vector<float> inputData(2 * 1 * numBins * chunkTimeFrames, 0.0f);
            DBG("    Input data allocated: " << inputData.size() << " floats");

            // Fill with magnitude spectrogram
            DBG("    Filling magnitude spectrogram (frames " << frameStart << " to " << frameEnd << ")");
            for (int t = 0; t < framesInChunk; ++t)
            {
                for (int f = 0; f < numBins; ++f)
                {
                    // Left channel: [0, 0, f, t]
                    int idxL = 0 * (1 * numBins * chunkTimeFrames) + 0 * (numBins * chunkTimeFrames) + f * chunkTimeFrames + t;
                    inputData[idxL] = std::abs(stftLeft[frameStart + t][f]);

                    // Right channel: [1, 0, f, t]
                    int idxR = 1 * (1 * numBins * chunkTimeFrames) + 0 * (numBins * chunkTimeFrames) + f * chunkTimeFrames + t;
                    inputData[idxR] = std::abs(stftRight[frameStart + t][f]);
                }
            }
            DBG("    Magnitude spectrogram filled");

            // Run inference
            DBG("    Running inference...");
            InferenceResult inferResult;
            if (currentModel.usesMultipleFiles)
            {
                DBG("    Using multi-file inference for stem: " << stemName);
                inferResult = runStemInference(stemName, inputData, inputShape);
            }
            else
            {
                inferResult = inference->run(inputData, inputShape);
            }

            if (!inferResult.success)
            {
                DBG("  Inference failed: " << inferResult.errorMessage);
                result.errorMessage = "Inference failed for " + stemName + ": " + inferResult.errorMessage;
                return result;
            }

            if (inferResult.outputs.empty())
            {
                result.errorMessage = "No output from model for " + stemName;
                return result;
            }

            // Apply mask to STFT
            // Output shape matches input: [2, 1, 512, 1024]
            const auto& outputMask = inferResult.outputs[0];
            DBG("  Chunk " << chunk << " output size: " << outputMask.size() << " (expected: " << inputData.size() << ")");

            for (int t = 0; t < framesInChunk; ++t)
            {
                for (int f = 0; f < numBins; ++f)
                {
                    // Left channel mask
                    int idxL = 0 * (1 * numBins * chunkTimeFrames) + 0 * (numBins * chunkTimeFrames) + f * chunkTimeFrames + t;
                    float maskL = (idxL < static_cast<int>(outputMask.size())) ? outputMask[idxL] : 0.0f;
                    maskL = std::clamp(maskL, 0.0f, 1.0f);

                    // Right channel mask
                    int idxR = 1 * (1 * numBins * chunkTimeFrames) + 0 * (numBins * chunkTimeFrames) + f * chunkTimeFrames + t;
                    float maskR = (idxR < static_cast<int>(outputMask.size())) ? outputMask[idxR] : 0.0f;
                    maskR = std::clamp(maskR, 0.0f, 1.0f);

                    maskedLeft[frameStart + t][f] = stftLeft[frameStart + t][f] * maskL;
                    maskedRight[frameStart + t][f] = stftRight[frameStart + t][f] * maskR;
                }
            }
        }

        // Inverse STFT for this stem
        auto& stemBuffer = result.stems[stemName];

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto& maskedSTFT = (ch == 0) ? maskedLeft : maskedRight;
            std::vector<float> outputBuffer(totalSamples + fftSize, 0.0f);
            std::vector<float> windowSum(totalSamples + fftSize, 0.0f);

            for (int frame = 0; frame < totalFrames; ++frame)
            {
                // Inverse FFT
                std::vector<float> fftBuffer(fftSize * 2, 0.0f);
                for (int bin = 0; bin < numBins; ++bin)
                {
                    fftBuffer[bin * 2] = maskedSTFT[frame][bin].real();
                    fftBuffer[bin * 2 + 1] = maskedSTFT[frame][bin].imag();
                }
                fft.performRealOnlyInverseTransform(fftBuffer.data());

                // Overlap-add
                int startIdx = frame * hopSize;
                for (int i = 0; i < fftSize; ++i)
                {
                    if (startIdx + i < static_cast<int>(outputBuffer.size()))
                    {
                        outputBuffer[startIdx + i] += fftBuffer[i] * window[i];
                        windowSum[startIdx + i] += window[i] * window[i];
                    }
                }
            }

            // Normalize and write to output
            for (int i = 0; i < totalSamples; ++i)
            {
                float sample = 0.0f;
                if (windowSum[i] > 1e-8f)
                {
                    sample = outputBuffer[i] / windowSum[i];
                }
                stemBuffer.setSample(ch, i, sample);
            }
        }

        DBG("  Completed stem: " << stemName);
    }

    result.success = true;
    DBG("  Separation complete!");

    if (progressCallback)
        progressCallback(1.0f, "Complete");

    return result;
}

std::vector<std::complex<float>> SpleeterEngine::computeSTFT(
    const float* input,
    int numSamples,
    int fftSize,
    int hopSize)
{
    // Implementation moved inline to separate() for efficiency
    return {};
}

juce::AudioBuffer<float> SpleeterEngine::computeISTFT(
    const std::vector<std::complex<float>>& stft,
    int numSamples,
    int fftSize,
    int hopSize)
{
    // Implementation moved inline to separate() for efficiency
    return {};
}

// ============================================================================
// PythonDemucsEngine Implementation
// Calls terminal Python demucs for 100% identical output
// ============================================================================

PythonDemucsEngine::PythonDemucsEngine()
{
    DBG("PythonDemucsEngine: Created");
}

PythonDemucsEngine::~PythonDemucsEngine()
{
    unloadModel();
}

juce::String PythonDemucsEngine::findDemucsPath()
{
    std::vector<juce::String> searchPaths;

#if JUCE_WINDOWS
    // Windows common locations
    juce::String userProfile = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName();
    searchPaths = {
        userProfile + "\\miniconda3\\Scripts\\demucs.exe",
        userProfile + "\\miniconda3\\envs\\demucs\\Scripts\\demucs.exe",
        userProfile + "\\anaconda3\\Scripts\\demucs.exe",
        userProfile + "\\anaconda3\\envs\\demucs\\Scripts\\demucs.exe",
        userProfile + "\\AppData\\Local\\Programs\\Python\\Python311\\Scripts\\demucs.exe",
        userProfile + "\\AppData\\Local\\Programs\\Python\\Python310\\Scripts\\demucs.exe",
        userProfile + "\\AppData\\Local\\Programs\\Python\\Python39\\Scripts\\demucs.exe",
        "C:\\ProgramData\\miniconda3\\Scripts\\demucs.exe",
        "C:\\ProgramData\\anaconda3\\Scripts\\demucs.exe",
    };
#else
    // macOS/Linux common locations
    searchPaths = {
        "/opt/homebrew/Caskroom/miniforge/base/bin/demucs",  // Miniforge (Apple Silicon)
        "/opt/homebrew/bin/demucs",                          // Homebrew (Apple Silicon)
        "/usr/local/bin/demucs",                             // Homebrew (Intel Mac)
        "/Users/" + juce::SystemStats::getLogonName() + "/miniforge3/bin/demucs",
        "/Users/" + juce::SystemStats::getLogonName() + "/miniconda3/bin/demucs",
        "/Users/" + juce::SystemStats::getLogonName() + "/anaconda3/bin/demucs",
        "/Users/" + juce::SystemStats::getLogonName() + "/.local/bin/demucs",
    };
#endif

    // Check each known path
    for (const auto& path : searchPaths)
    {
        juce::File f(path);
        if (f.existsAsFile())
        {
            DBG("PythonDemucsEngine: Found demucs at " << path);
            return path;
        }
    }

    // Try using shell to find it (works better for inherited PATH)
    juce::ChildProcess check;
    juce::StringArray shellCmd;

#if JUCE_WINDOWS
    shellCmd.add("cmd.exe");
    shellCmd.add("/c");
    shellCmd.add("where demucs");
#else
    shellCmd.add("/bin/bash");
    shellCmd.add("-l");
    shellCmd.add("-c");
    shellCmd.add("which demucs");
#endif

    if (check.start(shellCmd))
    {
        check.waitForProcessToFinish(5000);
        auto output = check.readAllProcessOutput().trim();
        if (output.isNotEmpty() && check.getExitCode() == 0)
        {
#if JUCE_WINDOWS
            // 'where' may return multiple lines, take the first
            if (output.contains("\n"))
                output = output.upToFirstOccurrenceOf("\n", false, false).trim();
#endif
            juce::File f(output);
            if (f.existsAsFile())
            {
                DBG("PythonDemucsEngine: Found demucs via shell at " << output);
                return output;
            }
        }
    }

    DBG("PythonDemucsEngine: demucs not found");
    return {};
}

bool PythonDemucsEngine::isDemucsAvailable()
{
    demucsPath = findDemucsPath();
    return demucsPath.isNotEmpty();
}

bool PythonDemucsEngine::loadModel(const ModelInfo& model)
{
    DBG("PythonDemucsEngine::loadModel ENTER - " << model.id);

    if (model.engine != SeparationEngine::PythonDemucs)
    {
        DBG("PythonDemucsEngine: Wrong engine type for model " << model.id);
        return false;
    }

    // Check if demucs is available
    if (!isDemucsAvailable())
    {
        DBG("PythonDemucsEngine: Python demucs not found in PATH. Install with: pip install demucs");
        return false;
    }

    // Map model ID to demucs model name
    // e.g., "htdemucs_4stems" -> "htdemucs"
    //       "htdemucs_6stems" -> "htdemucs_6s"
    if (model.id.find("6stems") != std::string::npos || model.id.find("6s") != std::string::npos)
    {
        demucsModelName = "htdemucs_6s";
    }
    else if (model.id.find("ft") != std::string::npos)
    {
        demucsModelName = "htdemucs_ft";
    }
    else
    {
        demucsModelName = "htdemucs";  // Default 4-stem model
    }

    currentModel = model;
    modelLoaded = true;
    DBG("PythonDemucsEngine: Model configured as " << demucsModelName);
    return true;
}

void PythonDemucsEngine::unloadModel()
{
    modelLoaded = false;
    currentModel = {};
    demucsModelName.clear();
}

bool PythonDemucsEngine::isModelLoaded() const
{
    return modelLoaded && !demucsModelName.empty();
}

std::vector<std::string> PythonDemucsEngine::getAvailableStems() const
{
    if (modelLoaded)
        return currentModel.stems;

    // Default stems for htdemucs
    if (demucsModelName == "htdemucs_6s")
        return {"vocals", "drums", "bass", "guitar", "piano", "other"};
    else
        return {"vocals", "drums", "bass", "other"};
}

juce::AudioBuffer<float> PythonDemucsEngine::loadAudioFile(const juce::File& file, double& sampleRate)
{
    juce::AudioBuffer<float> buffer;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader)
    {
        sampleRate = reader->sampleRate;
        buffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
        reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
    }

    return buffer;
}

juce::AudioBuffer<float> PythonDemucsEngine::resampleBuffer(
    const juce::AudioBuffer<float>& input,
    double sourceSampleRate,
    double targetSampleRate)
{
    // Skip if rates are the same (within 1Hz tolerance)
    if (std::abs(sourceSampleRate - targetSampleRate) < 1.0)
        return input;

    double ratio = targetSampleRate / sourceSampleRate;
    int newNumSamples = static_cast<int>(input.getNumSamples() * ratio);
    int numChannels = input.getNumChannels();

    juce::AudioBuffer<float> output(numChannels, newNumSamples);

    // Linear interpolation resampling
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = input.getReadPointer(ch);
        float* dst = output.getWritePointer(ch);

        for (int i = 0; i < newNumSamples; ++i)
        {
            double srcIdx = i / ratio;
            int idx0 = static_cast<int>(srcIdx);
            int idx1 = std::min(idx0 + 1, input.getNumSamples() - 1);
            float frac = static_cast<float>(srcIdx - idx0);

            dst[i] = src[idx0] * (1.0f - frac) + src[idx1] * frac;
        }
    }

    return output;
}

bool PythonDemucsEngine::runDemucsCommand(
    const juce::File& inputFile,
    const juce::File& outputDir,
    SeparationProgressCallback progressCallback)
{
    // Build demucs command as array (NOT a quoted string - JUCE doesn't parse quotes correctly)
    // Use the full path to demucs since GUI apps don't inherit shell PATH
    juce::StringArray args;
    args.add(demucsPath.isNotEmpty() ? demucsPath : "demucs");
    args.add("-n");
    args.add(juce::String(demucsModelName));
    args.add("--mp3");  // Use mp3 output to avoid torchcodec/FFmpeg dependency issues
    args.add("-o");
    args.add(outputDir.getFullPathName());  // No quotes needed - StringArray handles spaces
    args.add(inputFile.getFullPathName());  // No quotes needed - StringArray handles spaces

    DBG("PythonDemucsEngine: Running command: " << args.joinIntoString(" "));

    if (progressCallback)
        progressCallback(0.1f, "Starting Python Demucs...");

    juce::ChildProcess process;
    if (!process.start(args))
    {
        DBG("PythonDemucsEngine: Failed to start process");
        return false;
    }

    // Monitor process
    float progress = 0.1f;
    while (process.isRunning())
    {
        juce::Thread::sleep(500);

        // Read any output for progress
        auto output = process.readAllProcessOutput();
        if (output.isNotEmpty())
        {
            DBG("demucs output: " << output);

            // Try to parse progress from output
            // Demucs shows progress like "100%|██████████| 10/10 [00:05<00:00,  1.89it/s]"
            if (output.contains("%"))
            {
                int percentIdx = output.indexOf("%");
                if (percentIdx > 0)
                {
                    juce::String beforePercent = output.substring(std::max(0, percentIdx - 3), percentIdx);
                    int percent = beforePercent.getIntValue();
                    if (percent > 0 && percent <= 100)
                    {
                        progress = 0.1f + 0.8f * (percent / 100.0f);
                        if (progressCallback)
                            progressCallback(progress, ("Separating stems: " + juce::String(percent) + "%").toStdString());
                    }
                }
            }
        }

        // Slowly increment progress if we can't parse it
        if (progress < 0.85f)
        {
            progress += 0.01f;
            if (progressCallback)
                progressCallback(progress, "Running Python Demucs...");
        }
    }

    int exitCode = process.getExitCode();
    DBG("PythonDemucsEngine: Process exited with code " << exitCode);

    return exitCode == 0;
}

SeparationResult PythonDemucsEngine::separate(
    const juce::AudioBuffer<float>& input,
    double sampleRate,
    SeparationProgressCallback progressCallback,
    std::atomic<bool>* cancelFlag)
{
    DBG("PythonDemucsEngine::separate() ENTER");
    DBG("  Input samples: " << input.getNumSamples() << ", channels: " << input.getNumChannels());
    DBG("  Sample rate: " << sampleRate);
    DBG("  Model: " << demucsModelName);

    SeparationResult result;
    result.sampleRate = sampleRate;

    if (!isModelLoaded())
    {
        result.errorMessage = "No model loaded";
        return result;
    }

    if (input.getNumSamples() == 0)
    {
        result.errorMessage = "Empty input buffer";
        return result;
    }

    if (progressCallback)
        progressCallback(0.0f, "Preparing input file...");

    // Create temp directory for processing
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("spectralz_demucs_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.createDirectory();

    DBG("  Temp directory: " << tempDir.getFullPathName());

    // Save input to temp WAV file
    juce::File inputFile = tempDir.getChildFile("input.wav");

    {
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(
                new juce::FileOutputStream(inputFile),
                sampleRate,
                static_cast<unsigned int>(input.getNumChannels()),
                24,
                {},
                0));

        if (writer)
        {
            writer->writeFromAudioSampleBuffer(input, 0, input.getNumSamples());
        }
        else
        {
            result.errorMessage = "Failed to write temp input file";
            tempDir.deleteRecursively();
            return result;
        }
    }

    DBG("  Input file written: " << inputFile.getFullPathName());

    if (cancelFlag && cancelFlag->load())
    {
        result.errorMessage = "Cancelled";
        tempDir.deleteRecursively();
        return result;
    }

    // Create output directory
    juce::File outputDir = tempDir.getChildFile("output");
    outputDir.createDirectory();

    // Run demucs
    if (!runDemucsCommand(inputFile, outputDir, progressCallback))
    {
        result.errorMessage = "Python Demucs failed. Make sure it's installed: pip install demucs";
        tempDir.deleteRecursively();
        return result;
    }

    if (cancelFlag && cancelFlag->load())
    {
        result.errorMessage = "Cancelled";
        tempDir.deleteRecursively();
        return result;
    }

    if (progressCallback)
        progressCallback(0.9f, "Loading separated stems...");

    // Find output directory (demucs creates model_name/input subdirectory)
    juce::File stemDir = outputDir.getChildFile(demucsModelName).getChildFile("input");
    if (!stemDir.isDirectory())
    {
        // Try without "input" subfolder (depends on demucs version)
        stemDir = outputDir.getChildFile(demucsModelName);
    }

    DBG("  Looking for stems in: " << stemDir.getFullPathName());

    if (!stemDir.isDirectory())
    {
        result.errorMessage = ("Demucs output directory not found: " + stemDir.getFullPathName()).toStdString();
        tempDir.deleteRecursively();
        return result;
    }

    // Load each stem (demucs outputs mp3 with --mp3 flag to avoid torchcodec issues)
    auto expectedStems = getAvailableStems();
    for (const auto& stemName : expectedStems)
    {
        juce::File stemFile = stemDir.getChildFile(juce::String(stemName) + ".mp3");
        if (!stemFile.existsAsFile())
        {
            // Fallback to .wav in case of older demucs versions
            stemFile = stemDir.getChildFile(juce::String(stemName) + ".wav");
        }
        if (!stemFile.existsAsFile())
        {
            DBG("  Stem file not found: " << stemFile.getFullPathName());
            continue;
        }

        double stemSampleRate = 0;
        auto stemBuffer = loadAudioFile(stemFile, stemSampleRate);

        if (stemBuffer.getNumSamples() > 0)
        {
            DBG("  Loaded stem: " << stemName << " (" << stemBuffer.getNumSamples()
                << " samples @ " << stemSampleRate << "Hz)");

            // Demucs always outputs at 44.1kHz internally
            // Resample back to original input sample rate if needed
            if (std::abs(stemSampleRate - sampleRate) > 1.0)
            {
                DBG("  Resampling " << stemName << " from " << stemSampleRate
                    << "Hz to " << sampleRate << "Hz");
                auto resampledBuffer = resampleBuffer(stemBuffer, stemSampleRate, sampleRate);
                DBG("  After resample: " << resampledBuffer.getNumSamples() << " samples");
                result.stems[stemName] = std::move(resampledBuffer);
            }
            else
            {
                result.stems[stemName] = std::move(stemBuffer);
            }
        }
    }

    // Cleanup temp files
    tempDir.deleteRecursively();

    if (result.stems.empty())
    {
        result.errorMessage = "No stems were loaded from Demucs output";
        return result;
    }

    result.success = true;
    // Output sample rate matches input sample rate (resampled if needed)
    result.sampleRate = sampleRate;

    if (progressCallback)
        progressCallback(1.0f, "Complete - " + std::to_string(result.stems.size()) + " stems");

    DBG("PythonDemucsEngine: Separation complete with " << result.stems.size() << " stems");
    return result;
}

// ============================================================================
// PythonSpleeterEngine Implementation
// ============================================================================

PythonSpleeterEngine::PythonSpleeterEngine()
{
}

PythonSpleeterEngine::~PythonSpleeterEngine()
{
    unloadModel();
}

juce::String PythonSpleeterEngine::findSpleeterPath()
{
    std::vector<juce::String> searchPaths;

#if JUCE_WINDOWS
    // Windows common locations
    juce::String userProfile = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName();
    searchPaths = {
        userProfile + "\\miniconda3\\Scripts\\spleeter.exe",
        userProfile + "\\miniconda3\\envs\\spleeter\\Scripts\\spleeter.exe",
        userProfile + "\\anaconda3\\Scripts\\spleeter.exe",
        userProfile + "\\anaconda3\\envs\\spleeter\\Scripts\\spleeter.exe",
        userProfile + "\\AppData\\Local\\Programs\\Python\\Python311\\Scripts\\spleeter.exe",
        userProfile + "\\AppData\\Local\\Programs\\Python\\Python310\\Scripts\\spleeter.exe",
        userProfile + "\\AppData\\Local\\Programs\\Python\\Python39\\Scripts\\spleeter.exe",
        "C:\\ProgramData\\miniconda3\\Scripts\\spleeter.exe",
        "C:\\ProgramData\\anaconda3\\Scripts\\spleeter.exe",
    };
#else
    // macOS/Linux common locations
    searchPaths = {
        "/opt/homebrew/Caskroom/miniforge/base/envs/spleeter/bin/spleeter",  // Conda env on Apple Silicon
        "/opt/homebrew/Caskroom/miniforge/base/bin/spleeter",
        "/opt/homebrew/bin/spleeter",
        "/usr/local/bin/spleeter",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("miniforge3/envs/spleeter/bin/spleeter").getFullPathName(),
        juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("miniforge3/bin/spleeter").getFullPathName(),
        juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("miniconda3/envs/spleeter/bin/spleeter").getFullPathName(),
        juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("miniconda3/bin/spleeter").getFullPathName(),
        juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".local/bin/spleeter").getFullPathName(),
        juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("anaconda3/envs/spleeter/bin/spleeter").getFullPathName(),
    };
#endif

    // Check each known path
    for (const auto& path : searchPaths)
    {
        juce::File f(path);
        if (f.existsAsFile())
        {
            DBG("PythonSpleeterEngine: Found spleeter at " << path);
            return path;
        }
    }

    // Try using shell to find it (works better for inherited PATH)
    juce::ChildProcess check;
    juce::StringArray shellCmd;

#if JUCE_WINDOWS
    shellCmd.add("cmd.exe");
    shellCmd.add("/c");
    shellCmd.add("where spleeter");
#else
    shellCmd.add("/bin/bash");
    shellCmd.add("-l");
    shellCmd.add("-c");
    shellCmd.add("which spleeter");
#endif

    if (check.start(shellCmd))
    {
        check.waitForProcessToFinish(5000);
        auto output = check.readAllProcessOutput().trim();
        if (output.isNotEmpty() && check.getExitCode() == 0)
        {
#if JUCE_WINDOWS
            // 'where' may return multiple lines, take the first
            if (output.contains("\n"))
                output = output.upToFirstOccurrenceOf("\n", false, false).trim();
#endif
            juce::File f(output);
            if (f.existsAsFile())
            {
                DBG("PythonSpleeterEngine: Found spleeter via shell at " << output);
                return output;
            }
        }
    }

    DBG("PythonSpleeterEngine: spleeter not found");
    return {};
}

bool PythonSpleeterEngine::isSpleeterAvailable()
{
    spleeterPath = findSpleeterPath();
    return spleeterPath.isNotEmpty();
}

bool PythonSpleeterEngine::loadModel(const ModelInfo& model)
{
    DBG("PythonSpleeterEngine::loadModel ENTER - " << model.id);

    if (model.engine != SeparationEngine::PythonSpleeter)
    {
        DBG("PythonSpleeterEngine: Wrong engine type for model " << model.id);
        return false;
    }

    // Check if spleeter is available
    if (!isSpleeterAvailable())
    {
        DBG("PythonSpleeterEngine: Python spleeter not found. Install with: pip install spleeter");
        return false;
    }

    // Map model ID to spleeter preset
    // Spleeter presets: spleeter:2stems, spleeter:4stems, spleeter:5stems
    if (model.id.find("5stems") != std::string::npos || model.id.find("5s") != std::string::npos)
    {
        spleeterModelName = "spleeter:5stems";
    }
    else if (model.id.find("4stems") != std::string::npos || model.id.find("4s") != std::string::npos)
    {
        spleeterModelName = "spleeter:4stems";
    }
    else
    {
        spleeterModelName = "spleeter:2stems";  // Default 2-stem model
    }

    currentModel = model;
    modelLoaded = true;
    DBG("PythonSpleeterEngine: Model configured as " << spleeterModelName);
    return true;
}

void PythonSpleeterEngine::unloadModel()
{
    modelLoaded = false;
    currentModel = {};
    spleeterModelName.clear();
}

bool PythonSpleeterEngine::isModelLoaded() const
{
    return modelLoaded && !spleeterModelName.empty();
}

std::vector<std::string> PythonSpleeterEngine::getAvailableStems() const
{
    if (modelLoaded)
        return currentModel.stems;

    // Default stems based on model
    if (spleeterModelName == "spleeter:5stems")
        return {"vocals", "drums", "bass", "piano", "other"};
    else if (spleeterModelName == "spleeter:4stems")
        return {"vocals", "drums", "bass", "other"};
    else
        return {"vocals", "accompaniment"};  // 2stems
}

juce::AudioBuffer<float> PythonSpleeterEngine::loadAudioFile(const juce::File& file, double& sampleRate)
{
    juce::AudioBuffer<float> buffer;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader)
    {
        sampleRate = reader->sampleRate;
        buffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
        reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
    }

    return buffer;
}

juce::AudioBuffer<float> PythonSpleeterEngine::resampleBuffer(
    const juce::AudioBuffer<float>& input,
    double sourceSampleRate,
    double targetSampleRate)
{
    // Skip if rates are the same (within 1Hz tolerance)
    if (std::abs(sourceSampleRate - targetSampleRate) < 1.0)
        return input;

    double ratio = targetSampleRate / sourceSampleRate;
    int newNumSamples = static_cast<int>(input.getNumSamples() * ratio);
    int numChannels = input.getNumChannels();

    juce::AudioBuffer<float> output(numChannels, newNumSamples);

    // Linear interpolation resampling
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = input.getReadPointer(ch);
        float* dst = output.getWritePointer(ch);

        for (int i = 0; i < newNumSamples; ++i)
        {
            double srcIdx = i / ratio;
            int idx0 = static_cast<int>(srcIdx);
            int idx1 = std::min(idx0 + 1, input.getNumSamples() - 1);
            float frac = static_cast<float>(srcIdx - idx0);

            dst[i] = src[idx0] * (1.0f - frac) + src[idx1] * frac;
        }
    }

    return output;
}

bool PythonSpleeterEngine::runSpleeterCommand(
    const juce::File& inputFile,
    const juce::File& outputDir,
    SeparationProgressCallback progressCallback)
{
    juce::File homeDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory);

    if (progressCallback)
        progressCallback(0.1f, "Starting Python Spleeter...");

    juce::StringArray args;

#if JUCE_WINDOWS
    // Windows: Run spleeter directly with full path (no conda activation needed if in PATH)
    // If we found spleeter via findSpleeterPath(), use that path directly
    args.add(spleeterPath.isNotEmpty() ? spleeterPath : "spleeter");
    args.add("separate");
    args.add("-p");
    args.add(juce::String(spleeterModelName));
    args.add("-o");
    args.add(outputDir.getFullPathName());
    args.add(inputFile.getFullPathName());

    DBG("PythonSpleeterEngine: Running command: " << args.joinIntoString(" "));
#else
    // macOS/Linux: Use shell with conda activation
    // Spleeter looks for pretrained_models/ relative to current working directory
    // We need to run it from the home directory where the models are cached

    // Build shell command EXACTLY like terminal:
    // 1. Source conda.sh to initialize conda
    // 2. Activate the spleeter environment
    // 3. cd to home directory (where pretrained_models/ is)
    // 4. Run spleeter separate command
    juce::String condaBase = "/opt/homebrew/Caskroom/miniforge/base";
    juce::String shellCommand = "source " + condaBase + "/etc/profile.d/conda.sh && "
        + "conda activate spleeter && "
        + "cd " + homeDir.getFullPathName().quoted() + " && "
        + "spleeter separate"
        + " -p " + juce::String(spleeterModelName)
        + " -o " + outputDir.getFullPathName().quoted()
        + " " + inputFile.getFullPathName().quoted();

    DBG("PythonSpleeterEngine: Running shell command: " << shellCommand);

    args.add("/bin/sh");
    args.add("-c");
    args.add(shellCommand);
#endif

    juce::ChildProcess process;
    if (!process.start(args))
    {
        DBG("PythonSpleeterEngine: Failed to start process");
        return false;
    }

    // Monitor process
    float progress = 0.1f;
    while (process.isRunning())
    {
        juce::Thread::sleep(500);

        // Read any output for progress
        auto output = process.readAllProcessOutput();
        if (output.isNotEmpty())
        {
            DBG("spleeter output: " << output);

            // Spleeter doesn't show progress percentage like demucs
            // Just show a generic message
            if (progressCallback)
                progressCallback(progress, "Running Python Spleeter...");
        }

        // Slowly increment progress
        if (progress < 0.85f)
        {
            progress += 0.01f;
            if (progressCallback)
                progressCallback(progress, "Running Python Spleeter...");
        }
    }

    int exitCode = process.getExitCode();
    DBG("PythonSpleeterEngine: Process exited with code " << exitCode);

    return exitCode == 0;
}

SeparationResult PythonSpleeterEngine::separate(
    const juce::AudioBuffer<float>& input,
    double sampleRate,
    SeparationProgressCallback progressCallback,
    std::atomic<bool>* cancelFlag)
{
    DBG("PythonSpleeterEngine::separate() ENTER");
    DBG("  Input samples: " << input.getNumSamples() << ", channels: " << input.getNumChannels());
    DBG("  Sample rate: " << sampleRate);
    DBG("  Model: " << spleeterModelName);

    SeparationResult result;
    result.sampleRate = sampleRate;

    if (!isModelLoaded())
    {
        result.errorMessage = "No model loaded";
        return result;
    }

    if (input.getNumSamples() == 0)
    {
        result.errorMessage = "Empty input buffer";
        return result;
    }

    if (progressCallback)
        progressCallback(0.0f, "Preparing input file...");

    // Create temp directory for processing
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("spectralz_spleeter_" + juce::String(juce::Time::currentTimeMillis()));
    tempDir.createDirectory();

    DBG("  Temp directory: " << tempDir.getFullPathName());

    // Save input to temp WAV file
    juce::File inputFile = tempDir.getChildFile("input.wav");

    {
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(
                new juce::FileOutputStream(inputFile),
                sampleRate,
                static_cast<unsigned int>(input.getNumChannels()),
                24,
                {},
                0));

        if (writer)
        {
            writer->writeFromAudioSampleBuffer(input, 0, input.getNumSamples());
        }
        else
        {
            result.errorMessage = "Failed to write temp input file";
            tempDir.deleteRecursively();
            return result;
        }
    }

    DBG("  Input file written: " << inputFile.getFullPathName());

    if (cancelFlag && cancelFlag->load())
    {
        result.errorMessage = "Cancelled";
        tempDir.deleteRecursively();
        return result;
    }

    // Create output directory
    juce::File outputDir = tempDir.getChildFile("output");
    outputDir.createDirectory();

    // Run spleeter
    if (!runSpleeterCommand(inputFile, outputDir, progressCallback))
    {
        result.errorMessage = "Python Spleeter failed. Make sure it's installed: pip install spleeter";
        tempDir.deleteRecursively();
        return result;
    }

    if (cancelFlag && cancelFlag->load())
    {
        result.errorMessage = "Cancelled";
        tempDir.deleteRecursively();
        return result;
    }

    if (progressCallback)
        progressCallback(0.9f, "Loading separated stems...");

    // Find output directory (spleeter creates input/ subdirectory)
    juce::File stemDir = outputDir.getChildFile("input");
    if (!stemDir.isDirectory())
    {
        // Try without "input" subfolder
        stemDir = outputDir;
    }

    DBG("  Looking for stems in: " << stemDir.getFullPathName());

    if (!stemDir.isDirectory())
    {
        result.errorMessage = ("Spleeter output directory not found: " + stemDir.getFullPathName()).toStdString();
        tempDir.deleteRecursively();
        return result;
    }

    // Load each stem (spleeter outputs WAV files)
    auto expectedStems = getAvailableStems();
    for (const auto& stemName : expectedStems)
    {
        juce::File stemFile = stemDir.getChildFile(juce::String(stemName) + ".wav");
        if (!stemFile.existsAsFile())
        {
            DBG("  Stem file not found: " << stemFile.getFullPathName());
            continue;
        }

        double stemSampleRate = 0;
        auto stemBuffer = loadAudioFile(stemFile, stemSampleRate);

        if (stemBuffer.getNumSamples() > 0)
        {
            DBG("  Loaded stem: " << stemName << " (" << stemBuffer.getNumSamples()
                << " samples @ " << stemSampleRate << "Hz)");

            // Spleeter outputs at 44.1kHz by default
            // Resample back to original input sample rate if needed
            if (std::abs(stemSampleRate - sampleRate) > 1.0)
            {
                DBG("  Resampling " << stemName << " from " << stemSampleRate
                    << "Hz to " << sampleRate << "Hz");
                auto resampledBuffer = resampleBuffer(stemBuffer, stemSampleRate, sampleRate);
                DBG("  After resample: " << resampledBuffer.getNumSamples() << " samples");
                result.stems[stemName] = std::move(resampledBuffer);
            }
            else
            {
                result.stems[stemName] = std::move(stemBuffer);
            }
        }
    }

    // Cleanup temp files
    tempDir.deleteRecursively();

    if (result.stems.empty())
    {
        result.errorMessage = "No stems were loaded from Spleeter output";
        return result;
    }

    result.success = true;
    result.sampleRate = sampleRate;

    if (progressCallback)
        progressCallback(1.0f, "Complete - " + std::to_string(result.stems.size()) + " stems");

    DBG("PythonSpleeterEngine: Separation complete with " << result.stems.size() << " stems");
    return result;
}

} // namespace spectralz

#endif // SPECTRALZ_ENABLE_ML
