#include "CorrectiveDSP.h"
#include <cmath>
#include <algorithm>

namespace MixSense
{

CorrectiveDSP::CorrectiveDSP()
{
    outputGainSmoothed.setCurrentAndTargetValue(1.0f);
}

void CorrectiveDSP::prepare(const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

    glueCompressor.prepare(spec);
    glueCompressor.setThreshold(-18.0f);
    glueCompressor.setRatio(2.5f);
    glueCompressor.setAttack(25.0f); // 25ms attack preserves drum punch
    glueCompressor.setRelease(120.0f);

    safetyLimiter.prepare(spec);
    safetyLimiter.setThreshold(-0.5f);
    safetyLimiter.setRelease(80.0f);

    outputGainSmoothed.reset(spec.sampleRate, 0.05); // 50ms smoothing

    dryBuffer.setSize((int)spec.numChannels, (int)spec.maximumBlockSize);

    updateFilterCoefficients();
    reset();
}

void CorrectiveDSP::reset()
{
    hpfLeft.reset();
    hpfRight.reset();
    sideHpf.reset();

    for (int b = 0; b < 5; ++b)
    {
        eqFiltersL[(size_t)b].reset();
        eqFiltersR[(size_t)b].reset();
    }

    glueCompressor.reset();
    safetyLimiter.reset();
}

void CorrectiveDSP::setFixParameters(const SuggestedFixAction& fixes)
{
    currentFixes = fixes;
    updateFilterCoefficients();

    float targetGainLinear = juce::Decibels::decibelsToGain(currentFixes.outputGainTrimDb * assistantStrength);
    outputGainSmoothed.setTargetValue(targetGainLinear);
}

void CorrectiveDSP::setAssistantStrength(float strength0To1)
{
    assistantStrength = juce::jlimit(0.0f, 1.0f, strength0To1);
    updateFilterCoefficients();

    float targetGainLinear = juce::Decibels::decibelsToGain(currentFixes.outputGainTrimDb * assistantStrength);
    outputGainSmoothed.setTargetValue(targetGainLinear);
}

void CorrectiveDSP::updateFilterCoefficients()
{
    if (currentSampleRate <= 0.0)
        return;

    // Sub HPF
    if (currentFixes.enableHighPass && assistantStrength > 0.05f)
    {
        auto hpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, currentFixes.highPassFreq, 0.7071f);
        hpfLeft.coefficients = hpfCoeffs;
        hpfRight.coefficients = hpfCoeffs;
    }
    else
    {
        auto bypassCoeffs = juce::dsp::IIR::Coefficients<float>::makeAllPass(currentSampleRate, 1000.0f);
        hpfLeft.coefficients = bypassCoeffs;
        hpfRight.coefficients = bypassCoeffs;
    }

    // Mono Bass Side-HPF at 120 Hz
    if (currentFixes.enableMonoBass && assistantStrength > 0.05f)
    {
        sideHpf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, 120.0f, 0.7071f);
    }
    else
    {
        sideHpf.coefficients = juce::dsp::IIR::Coefficients<float>::makeAllPass(currentSampleRate, 1000.0f);
    }

    // 5-Band Parametric EQ
    for (int b = 0; b < 5; ++b)
    {
        if (currentFixes.enableEqBand[b] && assistantStrength > 0.01f)
        {
            float scaledGainDb = currentFixes.eqGainDb[b] * assistantStrength;
            float freq = juce::jlimit(20.0f, (float)(currentSampleRate * 0.49), currentFixes.eqFreq[b]);
            float q = juce::jlimit(0.1f, 10.0f, currentFixes.eqQ[b]);
            float gainLin = juce::Decibels::decibelsToGain(scaledGainDb);

            typename juce::dsp::IIR::Coefficients<float>::Ptr coeffs;

            if (b == 0) // Band 0: Low Shelf
            {
                coeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(currentSampleRate, freq, q, gainLin);
            }
            else if (b == 4) // Band 4: High Shelf
            {
                coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(currentSampleRate, freq, q, gainLin);
            }
            else // Bands 1, 2, 3: Peak / Bell filters
            {
                coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, freq, q, gainLin);
            }

            eqFiltersL[(size_t)b].coefficients = coeffs;
            eqFiltersR[(size_t)b].coefficients = coeffs;
        }
        else
        {
            auto flatCoeffs = juce::dsp::IIR::Coefficients<float>::makeAllPass(currentSampleRate, 1000.0f);
            eqFiltersL[(size_t)b].coefficients = flatCoeffs;
            eqFiltersR[(size_t)b].coefficients = flatCoeffs;
        }
    }
}

void CorrectiveDSP::process(juce::AudioBuffer<float>& buffer)
{
    if (isBypassed)
        return;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    if (isDeltaMode)
    {
        dryBuffer.makeCopyOf(buffer, true);
    }

    float* left = buffer.getWritePointer(0);
    float* right = numChannels > 1 ? buffer.getWritePointer(1) : left;

    // 1. Sub High-Pass Filter
    if (currentFixes.enableHighPass && assistantStrength > 0.05f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            left[i] = hpfLeft.processSample(left[i]);
            if (numChannels > 1)
                right[i] = hpfRight.processSample(right[i]);
        }
    }

    // 2. Bass Mono Maker (Mid/Side processing)
    if (currentFixes.enableMonoBass && numChannels > 1 && assistantStrength > 0.05f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float l = left[i];
            float r = right[i];

            float mid = (l + r) * 0.7071f;
            float side = (l - r) * 0.7071f;

            // Filter side channel
            float sideFiltered = sideHpf.processSample(side);

            // Reconstruct L/R
            left[i] = (mid + sideFiltered) * 0.7071f;
            right[i] = (mid - sideFiltered) * 0.7071f;
        }
    }

    // 3. 5-Band Parametric EQ
    for (int b = 0; b < 5; ++b)
    {
        if (currentFixes.enableEqBand[b] && assistantStrength > 0.01f)
        {
            auto& fL = eqFiltersL[(size_t)b];
            auto& fR = eqFiltersR[(size_t)b];

            for (int i = 0; i < numSamples; ++i)
            {
                left[i] = fL.processSample(left[i]);
                if (numChannels > 1)
                    right[i] = fR.processSample(right[i]);
            }
        }
    }

    // 4. Glue Compressor
    if (currentFixes.enableGlueCompressor && assistantStrength > 0.1f)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        glueCompressor.process(context);
    }

    // 5. Output Gain Trim
    if (outputGainSmoothed.isSmoothing() || std::abs(outputGainSmoothed.getTargetValue() - 1.0f) > 1e-4f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = outputGainSmoothed.getNextValue();
            left[i] *= g;
            if (numChannels > 1)
                right[i] *= g;
        }
    }

    // 6. Safety Limiter
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        safetyLimiter.process(context);
    }

    // 7. Delta Mode: output = Dry - Processed
    if (isDeltaMode)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* dry = dryBuffer.getReadPointer(ch);
            float* wet = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                wet[i] = dry[i] - wet[i];
            }
        }
    }
}

float CorrectiveDSP::getResponseAtFrequency(float freqHz) const
{
    if (isBypassed || currentSampleRate <= 0.0 || freqHz <= 0.0f)
        return 0.0f;

    double totalMag = 1.0;

    // Sub HPF
    if (currentFixes.enableHighPass && assistantStrength > 0.05f && hpfLeft.coefficients != nullptr)
    {
        totalMag *= hpfLeft.coefficients->getMagnitudeForFrequency(freqHz, currentSampleRate);
    }

    // 5-Band EQ
    for (int b = 0; b < 5; ++b)
    {
        if (currentFixes.enableEqBand[b] && assistantStrength > 0.01f && eqFiltersL[(size_t)b].coefficients != nullptr)
        {
            totalMag *= eqFiltersL[(size_t)b].coefficients->getMagnitudeForFrequency(freqHz, currentSampleRate);
        }
    }

    float db = juce::Decibels::gainToDecibels((float)totalMag, -40.0f);
    db += currentFixes.outputGainTrimDb * assistantStrength;
    return db;
}

} // namespace MixSense

