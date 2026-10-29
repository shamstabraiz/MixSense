#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace MixSense
{

class SpectrumComponent : public juce::Component,
                          public juce::Timer
{
public:
    explicit SpectrumComponent(MixSenseAudioProcessor& processor);
    ~SpectrumComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    MixSenseAudioProcessor& processor;
    std::vector<float> spectrumData;
    std::vector<float> smoothedDisplayMags;

    float mapFreqToX(float freqHz, float width) const;
    float mapDbToY(float db, float height) const;

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawTargetCorridors(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawCorrectiveCurve(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawBandPillars(juce::Graphics& g, juce::Rectangle<float> bounds);
};

} // namespace MixSense

