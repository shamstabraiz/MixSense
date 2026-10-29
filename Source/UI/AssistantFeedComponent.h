#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace MixSense
{

class IssueCardComponent : public juce::Component
{
public:
    IssueCardComponent(MixSenseAudioProcessor& processor, int issueIndex);
    ~IssueCardComponent() override = default;

    void update(const MixIssue& issue, int index);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    MixSenseAudioProcessor& processor;
    int issueIndex = 0;
    MixIssue currentIssue;

    juce::TextButton fixButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IssueCardComponent)
};

class AssistantFeedComponent : public juce::Component,
                               public juce::Timer
{
public:
    explicit AssistantFeedComponent(MixSenseAudioProcessor& processor);
    ~AssistantFeedComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    MixSenseAudioProcessor& processor;
    juce::Viewport viewport;
    juce::Component contentContainer;
    std::vector<std::unique_ptr<IssueCardComponent>> cardPool;

    int lastIssuesHash = 0;
    void rebuildCards();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AssistantFeedComponent)
};

} // namespace MixSense

