#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Analysis/DiagnosticEngine.h"

namespace MixSense
{

class CorrectiveDSP
{
public:
    CorrectiveDSP();
    ~CorrectiveDSP() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void process(juce::AudioBuffer<float>& buffer);

    // Apply auto-fix parameters
    void setFixParameters(const SuggestedFixAction& fixes);

    // Controls
    void setAssistantStrength(float strength0To1); // 0.0 to 1.0
    float getAssistantStrength() const { return assistantStrength; }

    void setBypassed(bool bypass) { isBypassed = bypass; }
    bool getBypassed() const { return isBypassed; }

    void setDeltaMode(bool delta) { isDeltaMode = delta; }
    bool getDeltaMode() const { return isDeltaMode; }

    // Query frequency response for UI curve rendering (returns dB gain at freqHz)
    float getResponseAtFrequency(float freqHz) const;

private:
    double currentSampleRate = 44100.0;
    float assistantStrength = 1.0f;
    bool isBypassed = false;
    bool isDeltaMode = false;

    SuggestedFixAction currentFixes;

    // Sub rumble HPF
    juce::dsp::IIR::Filter<float> hpfLeft;
    juce::dsp::IIR::Filter<float> hpfRight;

    // Bass Mono Maker (Side HPF at 120 Hz)
    juce::dsp::IIR::Filter<float> sideHpf;

    // 5-Band Parametric EQ Filters (Left and Right)
    std::array<juce::dsp::IIR::Filter<float>, 5> eqFiltersL;
    std::array<juce::dsp::IIR::Filter<float>, 5> eqFiltersR;

    // Glue Compressor
    juce::dsp::Compressor<float> glueCompressor;

    // Safety Limiter
    juce::dsp::Limiter<float> safetyLimiter;

    // Output trim
    juce::SmoothedValue<float> outputGainSmoothed;

    // Dry buffer for Delta mode
    juce::AudioBuffer<float> dryBuffer;

    void updateFilterCoefficients();
};

} // namespace MixSense

