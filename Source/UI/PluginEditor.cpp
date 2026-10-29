#include "PluginEditor.h"

namespace MixSense
{

PluginEditor::PluginEditor(MixSenseAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      spectrumComponent(p),
      assistantFeed(p),
      meterStrip(p)
{
    setSize(1000, 660);
    setResizable(true, true);
    setResizeLimits(850, 560, 1600, 1100);

    // Genre Selector
    genreSelector.addItem("Pop / Balanced", 1);
    genreSelector.addItem("EDM / Club", 2);
    genreSelector.addItem("Hip-Hop / 808", 3);
    genreSelector.addItem("Rock / Acoustic", 4);
    genreSelector.addItem("Vocal / Bus", 5);
    addAndMakeVisible(genreSelector);
    genreAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "genreProfile", genreSelector);

    // Stage Selector
    stageSelector.addItem("Pre-Master (-6dB)", 1);
    stageSelector.addItem("Mastering (LUFS)", 2);
    addAndMakeVisible(stageSelector);
    stageAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "stageMode", stageSelector);

    // Smart Listen Button
    listenButton.setButtonText("SMART LISTEN");
    listenButton.onClick = [this]() {
        processor.triggerSmartListen();
    };
    addAndMakeVisible(listenButton);

    // Apply All Fixes Button
    applyAllButton.setButtonText("APPLY ALL FIXES");
    applyAllButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2979ff));
    applyAllButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    applyAllButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    applyAllButton.onClick = [this]() {
        processor.applyAllFixes();
    };
    addAndMakeVisible(applyAllButton);

    // Clear All Button
    clearAllButton.setButtonText("RESET");
    clearAllButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263238));
    clearAllButton.onClick = [this]() {
        processor.resetAllFixes();
    };
    addAndMakeVisible(clearAllButton);

    // Strength Slider
    strengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    strengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
    strengthSlider.setTextValueSuffix("x");
    addAndMakeVisible(strengthSlider);
    strengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "assistantStrength", strengthSlider);

    strengthLabel.setText("Strength:", juce::dontSendNotification);
    strengthLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    strengthLabel.setColour(juce::Label::textColourId, juce::Colour(0xff90a4ae));
    addAndMakeVisible(strengthLabel);

    // Delta Toggle
    deltaToggle.setButtonText("Delta");
    deltaToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffeceff1));
    addAndMakeVisible(deltaToggle);
    deltaAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getAPVTS(), "deltaMode", deltaToggle);

    // Bypass Toggle
    bypassToggle.setButtonText("Bypass");
    bypassToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffeceff1));
    addAndMakeVisible(bypassToggle);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getAPVTS(), "bypassed", bypassToggle);

    // Subcomponents
    addAndMakeVisible(spectrumComponent);
    addAndMakeVisible(assistantFeed);
    addAndMakeVisible(meterStrip);

    startTimerHz(15);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
}

void PluginEditor::timerCallback()
{
    processor.updateDiagnostics();

    if (processor.isListening())
    {
        int pct = (int)(processor.getListenProgress() * 100.0f);
        listenButton.setButtonText(juce::String::formatted("LISTENING... %d%%", pct));
        listenButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff9100));
        listenButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    }
    else
    {
        listenButton.setButtonText("SMART LISTEN");
        listenButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        listenButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }

    repaint();
}

void PluginEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Deep modern studio background
    g.setColour(juce::Colour(0xff0b0d11));
    g.fillAll();

    // Top Header Bar background
    auto header = bounds.removeFromTop(56.0f);
    g.setColour(juce::Colour(0xff141720));
    g.fillRect(header);
    g.setColour(juce::Colour(0xff1f2533));
    g.drawHorizontalLine((int)header.getBottom(), header.getX(), header.getRight());

    // Plugin Logo & Branding
    auto logoArea = header.removeFromLeft(155.0f).reduced(12.0f, 6.0f);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xff00e5ff)); // Neon Cyan
    g.drawText("MIX", logoArea.removeFromLeft(42.0f), juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffffffff));
    g.drawText("SENSE", logoArea.removeFromLeft(68.0f), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(9.5f));
    g.setColour(juce::Colour(0xff90a4ae));
    g.drawText("AI MIX COACH", logoArea, juce::Justification::bottomLeft);

    // Health Score Badge
    auto healthArea = header.removeFromLeft(125.0f).reduced(6.0f, 4.0f);
    drawHealthScoreBadge(g, healthArea);
}

void PluginEditor::drawHealthScoreBadge(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    int health = processor.getMixHealthScore();
    juce::Colour col = health >= 80 ? juce::Colour(0xff00e676)
                                    : (health >= 60 ? juce::Colour(0xffffab40) : juce::Colour(0xffff5252));

    // Badge container
    g.setColour(juce::Colour(0xff0f1218));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(col.withAlpha(0.6f));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    // Health Score number
    auto numArea = bounds.removeFromLeft(48.0f);
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.setColour(col);
    g.drawText(juce::String(health) + "%", numArea, juce::Justification::centred);

    // Label
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xffeceff1));
    g.drawText("MIX HEALTH", bounds.removeFromTop(16.0f), juce::Justification::left);
    g.setFont(juce::FontOptions(9.0f));
    g.setColour(juce::Colour(0xff78909c));
    g.drawText(health >= 80 ? "Translation Ready" : (health >= 60 ? "Action Recommended" : "Issues Detected"),
               bounds, juce::Justification::left);
}

void PluginEditor::resized()
{
    auto bounds = getLocalBounds();

    // Top Header Layout (56 px)
    auto header = bounds.removeFromTop(56);
    header.removeFromLeft(285); // Skip Logo & Health Badge

    genreSelector.setBounds(header.removeFromLeft(125).reduced(2, 12));
    stageSelector.setBounds(header.removeFromLeft(135).reduced(2, 12));
    listenButton.setBounds(header.removeFromLeft(120).reduced(3, 12));
    applyAllButton.setBounds(header.removeFromLeft(115).reduced(3, 12));
    clearAllButton.setBounds(header.removeFromLeft(60).reduced(2, 12));

    strengthLabel.setBounds(header.removeFromLeft(55).reduced(0, 14));
    strengthSlider.setBounds(header.removeFromLeft(95).reduced(0, 14));
    deltaToggle.setBounds(header.removeFromLeft(60).reduced(2, 12));
    bypassToggle.setBounds(header.removeFromLeft(65).reduced(2, 12));

    // Bottom Metering Strip Layout (76 px)
    auto meterArea = bounds.removeFromBottom(76).reduced(8, 4);
    meterStrip.setBounds(meterArea);

    // Center Area: Left (Spectrum Visualizer 57%) + Right (Assistant Cards 43%)
    auto centerArea = bounds.reduced(8, 4);
    int spectrumWidth = (int)(centerArea.getWidth() * 0.57f);

    spectrumComponent.setBounds(centerArea.removeFromLeft(spectrumWidth).reduced(2));
    assistantFeed.setBounds(centerArea.reduced(2));
}

} // namespace MixSense

