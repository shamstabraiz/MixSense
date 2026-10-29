#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace MixSense
{

class MeterStripComponent : public juce::Component,
                            public juce::Timer
{
public:
    explicit MeterStripComponent(MixSenseAudioProcessor& processor);
    ~MeterStripComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    MixSenseAudioProcessor& processor;

    float dispPeakDb = -70.0f;
    float dispLufs = -70.0f;
    float dispCrestDb = 10.0f;
    float dispPhase = 1.0f;
    float dispSubWidth = 0.0f;
    bool clipLatched = false;

    juce::TextButton monoBassBtn;
    juce::TextButton resetClipBtn;

    void drawLoudnessMeter(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawDynamicsMeter(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawPhaseMeter(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawSubBassMeter(juce::Graphics& g, juce::Rectangle<float> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterStripComponent)
};

} // namespace MixSense

