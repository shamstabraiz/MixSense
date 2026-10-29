#include "AssistantFeedComponent.h"

namespace MixSense
{

IssueCardComponent::IssueCardComponent(MixSenseAudioProcessor& p, int index)
    : processor(p), issueIndex(index)
{
    fixButton.setButtonText("FIX IT");
    fixButton.onClick = [this]() {
        processor.toggleIssueFix(issueIndex);
    };
    addAndMakeVisible(fixButton);
}

void IssueCardComponent::update(const MixIssue& issue, int index)
{
    currentIssue = issue;
    issueIndex = index;

    bool hasAutoFix = (issue.fixAction.enableHighPass ||
                       issue.fixAction.enableMonoBass ||
                       issue.fixAction.enableGlueCompressor ||
                       std::abs(issue.fixAction.outputGainTrimDb) > 0.01f ||
                       issue.fixAction.enableEqBand[0] ||
                       issue.fixAction.enableEqBand[1] ||
                       issue.fixAction.enableEqBand[2] ||
                       issue.fixAction.enableEqBand[3] ||
                       issue.fixAction.enableEqBand[4]);

    fixButton.setVisible(hasAutoFix);
    if (hasAutoFix)
    {
        if (issue.fixApplied)
        {
            fixButton.setButtonText("FIX APPLIED");
            fixButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00c853));
            fixButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
            fixButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        }
        else
        {
            fixButton.setButtonText("1-CLICK FIX");
            fixButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2979ff));
            fixButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            fixButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        }
    }

    repaint();
}

void IssueCardComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    juce::Colour cardBg = juce::Colour(0xff181b22);
    juce::Colour borderCol = juce::Colour(0xff2c3444);
    juce::Colour accentCol;

    juce::String severityText;
    switch (currentIssue.severity)
    {
        case IssueSeverity::Critical:
            accentCol = juce::Colour(0xffff5252);
            severityText = "CRITICAL";
            break;
        case IssueSeverity::Warning:
            accentCol = juce::Colour(0xffffab40);
            severityText = "WARNING";
            break;
        case IssueSeverity::Optimal:
            accentCol = juce::Colour(0xff00e676);
            severityText = "OPTIMAL";
            break;
    }

    // Card background
    g.setColour(cardBg);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(borderCol);
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    // Left accent strip
    auto strip = bounds.removeFromLeft(4.0f);
    g.setColour(accentCol);
    g.fillRect(strip);

    // Padding inside card
    auto inner = bounds.reduced(10.0f, 8.0f);

    // Header line: Severity badge + Title
    auto headerArea = inner.removeFromTop(20.0f);
    auto badgeArea = headerArea.removeFromLeft(68.0f);

    g.setColour(accentCol.withAlpha(0.2f));
    g.fillRoundedRectangle(badgeArea, 3.0f);
    g.setColour(accentCol);
    g.drawRoundedRectangle(badgeArea, 3.0f, 1.0f);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText(severityText, badgeArea, juce::Justification::centred);

    headerArea.removeFromLeft(8.0f);
    g.setColour(juce::Colour(0xfff0f4f8));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    // Reserve room for fixButton if visible
    if (fixButton.isVisible())
        headerArea.removeFromRight(100.0f);
    g.drawText(currentIssue.title, headerArea, juce::Justification::centredLeft, true);

    // Explanation text
    inner.removeFromTop(4.0f);
    auto explArea = inner.removeFromTop(32.0f);
    g.setColour(juce::Colour(0xffb0bec5));
    g.setFont(juce::FontOptions(11.0f));
    g.drawFittedText(currentIssue.explanation, explArea.toNearestInt(), juce::Justification::topLeft, 2);

    // DAW / FL Studio Pro Tip box
    if (currentIssue.dawAdvice.isNotEmpty())
    {
        inner.removeFromTop(4.0f);
        auto tipBox = inner.removeFromTop(36.0f);
        g.setColour(juce::Colour(0xff12151c));
        g.fillRoundedRectangle(tipBox, 4.0f);
        g.setColour(juce::Colour(0xff222a38));
        g.drawRoundedRectangle(tipBox, 4.0f, 1.0f);

        auto tipInner = tipBox.reduced(6.0f, 2.0f);
        g.setColour(juce::Colour(0xff80d8ff));
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.drawText("FL Studio / Mix Tip:", tipInner.removeFromTop(14.0f), juce::Justification::left);

        g.setColour(juce::Colour(0xffeceff1));
        g.setFont(juce::FontOptions(10.5f));
        g.drawFittedText(currentIssue.dawAdvice, tipInner.toNearestInt(), juce::Justification::topLeft, 2);
    }
}

void IssueCardComponent::resized()
{
    auto bounds = getLocalBounds().reduced(12.0f, 8.0f);
    auto headerRight = bounds.removeFromTop(24.0f);
    fixButton.setBounds(headerRight.removeFromRight(95).reduced(0, 1));
}

// ==========================================
// AssistantFeedComponent implementation
// ==========================================
AssistantFeedComponent::AssistantFeedComponent(MixSenseAudioProcessor& p)
    : processor(p)
{
    viewport.setViewedComponent(&contentContainer, false);
    viewport.setScrollBarsShown(true, false); // Vertical scrollbar only
    addAndMakeVisible(viewport);

    startTimerHz(12);
}

AssistantFeedComponent::~AssistantFeedComponent()
{
    stopTimer();
}

void AssistantFeedComponent::timerCallback()
{
    rebuildCards();
}

void AssistantFeedComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff12141a));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(0xff222834));
    g.drawRoundedRectangle(bounds, 8.0f, 1.2f);
}

void AssistantFeedComponent::resized()
{
    viewport.setBounds(getLocalBounds().reduced(6));
    rebuildCards();
}

void AssistantFeedComponent::rebuildCards()
{
    auto issues = processor.getCurrentIssues();

    // Compute simple hash to check if issues changed
    int hash = (int)issues.size();
    for (const auto& iss : issues)
    {
        hash = hash * 31 + iss.id.hashCode() + (iss.fixApplied ? 1 : 0);
    }

    if (hash == lastIssuesHash && cardPool.size() == issues.size())
    {
        // Only update states
        for (size_t i = 0; i < issues.size(); ++i)
        {
            if (i < cardPool.size())
                cardPool[i]->update(issues[i], (int)i);
        }
        return;
    }

    lastIssuesHash = hash;

    cardPool.clear();
    contentContainer.removeAllChildren();

    const int cardHeight = 118;
    const int gap = 6;
    int y = 0;
    const int w = std::max(200, viewport.getWidth() - 14);

    for (size_t i = 0; i < issues.size(); ++i)
    {
        auto card = std::make_unique<IssueCardComponent>(processor, (int)i);
        card->setBounds(0, y, w, cardHeight);
        card->update(issues[i], (int)i);
        contentContainer.addAndMakeVisible(card.get());
        cardPool.push_back(std::move(card));
        y += cardHeight + gap;
    }

    contentContainer.setBounds(0, 0, w, std::max(viewport.getHeight(), y));
}

} // namespace MixSense

