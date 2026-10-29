#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "SpectrumComponent.h"
#include "AssistantFeedComponent.h"
#include "MeterStripComponent.h"

namespace MixSense
{

class PluginEditor : public juce::AudioProcessorEditor,
                     public juce::Timer
{
public:
    explicit PluginEditor(MixSenseAudioProcessor& processor);
    ~PluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    MixSenseAudioProcessor& processor;

    // Header Controls
    juce::ComboBox genreSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> genreAttachment;

    juce::ComboBox stageSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stageAttachment;

    juce::TextButton listenButton;
    juce::TextButton applyAllButton;
    juce::TextButton clearAllButton;

    juce::Slider strengthSlider;
    juce::Label strengthLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> strengthAttachment;

    juce::ToggleButton deltaToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> deltaAttachment;

    juce::ToggleButton bypassToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    // Subcomponents
    SpectrumComponent spectrumComponent;
    AssistantFeedComponent assistantFeed;
    MeterStripComponent meterStrip;

    void drawHealthScoreBadge(juce::Graphics& g, juce::Rectangle<float> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace MixSense

