#include "MeterStripComponent.h"
#include <cmath>

namespace MixSense
{

MeterStripComponent::MeterStripComponent(MixSenseAudioProcessor& p)
    : processor(p)
{
    monoBassBtn.setButtonText("MONO BASS");
    monoBassBtn.onClick = [this]() {
        // Toggle mono bass manually
        auto issues = processor.getCurrentIssues();
        for (int i = 0; i < (int)issues.size(); ++i)
        {
            if (issues[(size_t)i].id == "STEREO_SUB_BASS")
            {
                processor.toggleIssueFix(i);
                return;
            }
        }
        // If not flagged as issue, toggle APVTS param
        auto* param = processor.getAPVTS().getParameter("monoBassManual");
        if (param != nullptr)
        {
            float cur = param->getValue();
            param->setValueNotifyingHost(cur > 0.5f ? 0.0f : 1.0f);
        }
    };
    addAndMakeVisible(monoBassBtn);

    resetClipBtn.setButtonText("CLIP");
    resetClipBtn.onClick = [this]() {
        clipLatched = false;
        repaint();
    };
    addAndMakeVisible(resetClipBtn);

    startTimerHz(35);
}

MeterStripComponent::~MeterStripComponent()
{
    stopTimer();
}

void MeterStripComponent::timerCallback()
{
    auto snap = processor.getAnalyzer().getLatestSnapshot();

    dispPeakDb = dispPeakDb * 0.8f + snap.peakDb * 0.2f;
    dispLufs = dispLufs * 0.85f + snap.shortTermLufs * 0.15f;
    dispCrestDb = dispCrestDb * 0.9f + snap.crestFactorDb * 0.1f;
    dispPhase = dispPhase * 0.85f + snap.phaseCorrelation * 0.15f;
    dispSubWidth = dispSubWidth * 0.85f + snap.stereoWidthSubBass * 0.15f;

    if (snap.peakDb >= 0.0f)
        clipLatched = true;

    // Check if mono bass is active
    bool isMonoBassActive = (processor.getAPVTS().getRawParameterValue("monoBassManual")->load() > 0.5f);
    auto issues = processor.getCurrentIssues();
    for (const auto& iss : issues)
    {
        if (iss.id == "STEREO_SUB_BASS" && iss.fixApplied)
            isMonoBassActive = true;
    }

    if (isMonoBassActive)
    {
        monoBassBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00e676));
        monoBassBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        monoBassBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    }
    else
    {
        monoBassBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222a38));
        monoBassBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        monoBassBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }

    resetClipBtn.setColour(juce::TextButton::buttonColourId, clipLatched ? juce::Colour(0xffff1744) : juce::Colour(0xff1e232d));
    resetClipBtn.setColour(juce::TextButton::textColourOnId, clipLatched ? juce::Colours::white : juce::Colour(0xff607d8b));
    resetClipBtn.setColour(juce::TextButton::textColourOffId, clipLatched ? juce::Colours::white : juce::Colour(0xff607d8b));

    repaint();
}

void MeterStripComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark sleek container
    g.setColour(juce::Colour(0xff12141a));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(0xff222834));
    g.drawRoundedRectangle(bounds, 8.0f, 1.2f);

    auto inner = bounds.reduced(10.0f, 6.0f);
    float colW = inner.getWidth() / 4.0f;

    // Section 1: Loudness & Peak
    auto col1 = inner.removeFromLeft(colW).reduced(4.0f, 0.0f);
    drawLoudnessMeter(g, col1);

    // Section 2: Dynamic Range / Crest Factor
    auto col2 = inner.removeFromLeft(colW).reduced(4.0f, 0.0f);
    drawDynamicsMeter(g, col2);

    // Section 3: Phase Correlation
    auto col3 = inner.removeFromLeft(colW).reduced(4.0f, 0.0f);
    drawPhaseMeter(g, col3);

    // Section 4: Sub-Bass Stereo Width & Mono Maker
    auto col4 = inner.reduced(4.0f, 0.0f);
    drawSubBassMeter(g, col4);
}

void MeterStripComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10, 6);
    int colW = bounds.getWidth() / 4;

    // Clip button inside Col 1
    auto col1 = bounds.removeFromLeft(colW);
    resetClipBtn.setBounds(col1.getRight() - 42, col1.getY() + 2, 38, 18);

    // Mono bass button inside Col 4
    bounds.removeFromLeft(colW * 2);
    auto col4 = bounds;
    monoBassBtn.setBounds(col4.getRight() - 90, col4.getY() + 20, 86, 22);
}

void MeterStripComponent::drawLoudnessMeter(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xff90a4ae));
    g.drawText("LOUDNESS / PEAK", bounds.removeFromTop(16.0f), juce::Justification::left);

    // Peak readout
    auto textRow = bounds.removeFromTop(18.0f);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    juce::Colour peakCol = dispPeakDb >= 0.0f ? juce::Colour(0xffff5252) : juce::Colour(0xffeceff1);
    g.setColour(peakCol);
    g.drawText(juce::String::formatted("Peak: %.1f dBFS  |  LUFS: %.1f", dispPeakDb, dispLufs), textRow, juce::Justification::left);

    // Horizontal bar meter (-48 to +3 dBFS)
    auto barRect = bounds.removeFromTop(12.0f).reduced(0.0f, 2.0f);
    g.setColour(juce::Colour(0xff1a1e27));
    g.fillRoundedRectangle(barRect, 3.0f);

    float normPeak = juce::jlimit(0.0f, 1.0f, (dispPeakDb + 48.0f) / 51.0f);
    auto fillRect = barRect.withWidth(barRect.getWidth() * normPeak);

    juce::ColourGradient grad(juce::Colour(0xff00e676), barRect.getX(), 0,
                              juce::Colour(0xffff1744), barRect.getRight(), 0, false);
    grad.addColour(0.85, juce::Colour(0xffffb300));
    g.setGradientFill(grad);
    g.fillRoundedRectangle(fillRect, 3.0f);
}

void MeterStripComponent::drawDynamicsMeter(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xff90a4ae));
    g.drawText("CREST FACTOR (DYNAMICS)", bounds.removeFromTop(16.0f), juce::Justification::left);

    auto textRow = bounds.removeFromTop(18.0f);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    
    juce::String feel = "Balanced";
    juce::Colour crestCol = juce::Colour(0xff00e676);
    if (dispCrestDb < 6.5f) { feel = "Squashed / Dense"; crestCol = juce::Colour(0xffff5252); }
    else if (dispCrestDb < 8.5f) { feel = "Loud / Heavy"; crestCol = juce::Colour(0xffffb74d); }
    else if (dispCrestDb > 15.0f) { feel = "Spiky / Wide DR"; crestCol = juce::Colour(0xffffb74d); }

    g.setColour(crestCol);
    g.drawText(juce::String::formatted("%.1f dB (%s)", dispCrestDb, feel.toRawUTF8()), textRow, juce::Justification::left);

    // Bar meter (0 to 20 dB)
    auto barRect = bounds.removeFromTop(12.0f).reduced(0.0f, 2.0f);
    g.setColour(juce::Colour(0xff1a1e27));
    g.fillRoundedRectangle(barRect, 3.0f);

    float normCrest = juce::jlimit(0.0f, 1.0f, dispCrestDb / 20.0f);
    auto fillRect = barRect.withWidth(barRect.getWidth() * normCrest);
    g.setColour(crestCol);
    g.fillRoundedRectangle(fillRect, 3.0f);
}

void MeterStripComponent::drawPhaseMeter(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xff90a4ae));
    g.drawText("PHASE CORRELATION", bounds.removeFromTop(16.0f), juce::Justification::left);

    auto textRow = bounds.removeFromTop(18.0f);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));

    juce::Colour phaseCol = dispPhase < 0.0f ? juce::Colour(0xffff1744) : (dispPhase < 0.3f ? juce::Colour(0xffffb300) : juce::Colour(0xff00e676));
    g.setColour(phaseCol);
    g.drawText(juce::String::formatted("r = %+.2f (%s)", dispPhase, dispPhase < 0.0f ? "Phase Cancel!" : (dispPhase > 0.4f ? "Mono Safe" : "Wide")),
               textRow, juce::Justification::left);

    // Phase bar (-1.0 to +1.0)
    auto barRect = bounds.removeFromTop(12.0f).reduced(0.0f, 2.0f);
    g.setColour(juce::Colour(0xff1a1e27));
    g.fillRoundedRectangle(barRect, 3.0f);

    // Center divider at 0.0
    float centerX = barRect.getX() + barRect.getWidth() * 0.5f;
    g.setColour(juce::Colour(0x55ffffff));
    g.drawVerticalLine((int)centerX, barRect.getY(), barRect.getBottom());

    float indicatorX = barRect.getX() + barRect.getWidth() * (dispPhase + 1.0f) * 0.5f;
    indicatorX = juce::jlimit(barRect.getX() + 3.0f, barRect.getRight() - 3.0f, indicatorX);

    g.setColour(phaseCol);
    g.fillRoundedRectangle(indicatorX - 3.0f, barRect.getY(), 6.0f, barRect.getHeight(), 2.0f);
}

void MeterStripComponent::drawSubBassMeter(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xff90a4ae));
    g.drawText("SUB STEREO (< 120Hz)", bounds.removeFromTop(16.0f), juce::Justification::left);

    auto textRow = bounds.removeFromTop(18.0f);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));

    float widthPct = dispSubWidth * 100.0f;
    juce::Colour widthCol = widthPct > 15.0f ? juce::Colour(0xffff5252) : (widthPct > 8.0f ? juce::Colour(0xffffb74d) : juce::Colour(0xff00e676));
    g.setColour(widthCol);
    g.drawText(juce::String::formatted("Side Energy: %.0f%%", widthPct), textRow, juce::Justification::left);

    // Meter bar (0% to 50%)
    auto barRect = bounds.removeFromTop(12.0f).reduced(0.0f, 2.0f);
    barRect.removeFromRight(100.0f); // Space for button
    g.setColour(juce::Colour(0xff1a1e27));
    g.fillRoundedRectangle(barRect, 3.0f);

    float normWidth = juce::jlimit(0.0f, 1.0f, widthPct / 40.0f);
    auto fillRect = barRect.withWidth(barRect.getWidth() * normWidth);
    g.setColour(widthCol);
    g.fillRoundedRectangle(fillRect, 3.0f);
}

} // namespace MixSense

