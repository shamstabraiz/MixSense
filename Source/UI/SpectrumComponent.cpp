#include "SpectrumComponent.h"
#include <cmath>

namespace MixSense
{

SpectrumComponent::SpectrumComponent(MixSenseAudioProcessor& p)
    : processor(p)
{
    smoothedDisplayMags.resize(512, -70.0f);
    startTimerHz(35);
}

SpectrumComponent::~SpectrumComponent()
{
    stopTimer();
}

void SpectrumComponent::timerCallback()
{
    processor.getAnalyzer().getSpectrumMagnitudes(spectrumData);
    if (!spectrumData.empty())
    {
        const size_t numDisplayPoints = smoothedDisplayMags.size();
        for (size_t i = 0; i < numDisplayPoints; ++i)
        {
            // Map display point index to FFT bin logarithmically
            float normX = (float)i / (float)numDisplayPoints;
            float freq = 20.0f * std::pow(20000.0f / 20.0f, normX);
            size_t bin = (size_t)juce::jlimit(1, 2047, (int)(freq * 4096.0f / 44100.0f));

            float targetDb = (bin < spectrumData.size()) ? spectrumData[bin] : -70.0f;
            targetDb = juce::jlimit(-70.0f, 15.0f, targetDb);

            // Smooth ballistics
            smoothedDisplayMags[i] = smoothedDisplayMags[i] * 0.70f + targetDb * 0.30f;
        }
    }
    repaint();
}

void SpectrumComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark background with subtle gradient
    juce::ColourGradient bgGrad(juce::Colour(0xff12141a), 0, 0,
                               juce::Colour(0xff0d0f14), 0, bounds.getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillRoundedRectangle(bounds, 8.0f);

    g.setColour(juce::Colour(0xff222834));
    g.drawRoundedRectangle(bounds, 8.0f, 1.2f);

    auto plotBounds = bounds.reduced(10.0f, 10.0f);
    plotBounds.removeFromBottom(28.0f); // Reserve bottom for 6-band pillars

    drawGrid(g, plotBounds);
    drawTargetCorridors(g, plotBounds);
    drawSpectrum(g, plotBounds);
    drawCorrectiveCurve(g, plotBounds);

    auto pillarBounds = bounds.removeFromBottom(34.0f).reduced(10.0f, 4.0f);
    drawBandPillars(g, pillarBounds);
}

void SpectrumComponent::resized()
{
}

float SpectrumComponent::mapFreqToX(float freqHz, float width) const
{
    freqHz = juce::jlimit(20.0f, 20000.0f, freqHz);
    return width * (std::log10(freqHz / 20.0f) / std::log10(20000.0f / 20.0f));
}

float SpectrumComponent::mapDbToY(float db, float height) const
{
    const float minDb = -60.0f;
    const float maxDb = 12.0f;
    float norm = (db - minDb) / (maxDb - minDb);
    norm = juce::jlimit(0.0f, 1.0f, norm);
    return height * (1.0f - norm);
}

void SpectrumComponent::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setFont(juce::FontOptions(10.0f));

    // Frequency grid
    const float gridFreqs[] = { 30.0f, 60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 5000.0f, 10000.0f, 20000.0f };
    const char* freqLabels[] = { "30", "60", "120", "250", "500", "1k", "2.5k", "5k", "10k", "20k" };

    for (int i = 0; i < 10; ++i)
    {
        float x = bounds.getX() + mapFreqToX(gridFreqs[i], bounds.getWidth());
        g.setColour(juce::Colour::fromRGBA(0x88, 0xaa, 0xcc, 0x1a));
        g.drawVerticalLine((int)x, bounds.getY(), bounds.getBottom());

        g.setColour(juce::Colour::fromRGBA(0x88, 0xaa, 0xcc, 0x66));
        g.drawText(freqLabels[i], (int)x - 18, (int)bounds.getBottom() - 14, 36, 12, juce::Justification::centred);
    }

    // dB grid lines (-48, -36, -24, -12, 0, +6 dB)
    const float dbs[] = { -48.0f, -36.0f, -24.0f, -12.0f, 0.0f, 6.0f };
    for (float db : dbs)
    {
        float y = bounds.getY() + mapDbToY(db, bounds.getHeight());
        g.setColour(juce::Colour::fromRGBA(0xff, 0xff, 0xff, 0x18));
        g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());

        g.setColour(juce::Colour::fromRGBA(0xff, 0xff, 0xff, 0x44));
        g.drawText(juce::String((int)db) + " dB", (int)bounds.getX() + 4, (int)y - 10, 40, 10, juce::Justification::left);
    }
}

void SpectrumComponent::drawTargetCorridors(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    GenreProfile profile = getGenreProfile(processor.getActiveGenreProfileType());
    const auto& bandDefs = getBandDefinitions();

    for (int b = 0; b < 6; ++b)
    {
        float x1 = bounds.getX() + mapFreqToX(bandDefs[(size_t)b].lowHz, bounds.getWidth());
        float x2 = bounds.getX() + mapFreqToX(bandDefs[(size_t)b].highHz, bounds.getWidth());

        float targetDb = profile.bandTargets[(size_t)b].relativeDb - 15.0f; // Scaled to display baseline
        float topDb = targetDb + profile.bandTargets[(size_t)b].toleranceHighDb;
        float bottomDb = targetDb + profile.bandTargets[(size_t)b].toleranceLowDb;

        float yTop = bounds.getY() + mapDbToY(topDb, bounds.getHeight());
        float yBottom = bounds.getY() + mapDbToY(bottomDb, bounds.getHeight());

        juce::Rectangle<float> bandRect(x1, yTop, x2 - x1, yBottom - yTop);
        // Soft glowing green target corridor
        g.setColour(juce::Colour::fromRGBA(0x00, 0xe6, 0x76, 0x16));
        g.fillRect(bandRect);
        g.setColour(juce::Colour::fromRGBA(0x00, 0xe6, 0x76, 0x35));
        g.drawRect(bandRect, 1.0f);
    }
}

void SpectrumComponent::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (smoothedDisplayMags.empty())
        return;

    juce::Path path;
    const size_t numPoints = smoothedDisplayMags.size();
    path.startNewSubPath(bounds.getX(), bounds.getBottom());

    for (size_t i = 0; i < numPoints; ++i)
    {
        float normX = (float)i / (float)numPoints;
        float x = bounds.getX() + normX * bounds.getWidth();
        float y = bounds.getY() + mapDbToY(smoothedDisplayMags[i], bounds.getHeight());
        path.lineTo(x, y);
    }

    path.lineTo(bounds.getRight(), bounds.getBottom());
    path.closeSubPath();

    // Gradient fill under spectrum
    juce::ColourGradient fillGrad(juce::Colour::fromRGBA(0x44, 0x88, 0xff, 0x55), bounds.getCentreX(), bounds.getY(),
                                  juce::Colour::fromRGBA(0x11, 0x22, 0x44, 0x05), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(fillGrad);
    g.fillPath(path);

    // Spectrum outline stroke
    g.setColour(juce::Colour::fromRGB(0x64, 0xb5, 0xf6));
    g.strokePath(path, juce::PathStrokeType(1.6f));
}

void SpectrumComponent::drawCorrectiveCurve(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto& dsp = processor.getDSP();
    if (dsp.getBypassed())
        return;

    juce::Path curve;
    const int steps = 200;
    bool started = false;

    for (int i = 0; i <= steps; ++i)
    {
        float norm = (float)i / (float)steps;
        float freq = 20.0f * std::pow(20000.0f / 20.0f, norm);
        float eqGainDb = dsp.getResponseAtFrequency(freq);

        float x = bounds.getX() + norm * bounds.getWidth();
        // Curve plotted around 0 dB center baseline
        float y = bounds.getY() + mapDbToY(eqGainDb - 15.0f, bounds.getHeight());

        if (!started)
        {
            curve.startNewSubPath(x, y);
            started = true;
        }
        else
        {
            curve.lineTo(x, y);
        }
    }

    // Glowing cyan corrective curve
    g.setColour(juce::Colour::fromRGB(0x00, 0xe5, 0xff));
    g.strokePath(curve, juce::PathStrokeType(2.0f));
}

void SpectrumComponent::drawBandPillars(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto& bandDefs = getBandDefinitions();
    AnalysisSnapshot snap = processor.getAnalyzer().getLatestSnapshot();
    GenreProfile profile = getGenreProfile(processor.getActiveGenreProfileType());

    float bandW = bounds.getWidth() / 6.0f;
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));

    for (int b = 0; b < 6; ++b)
    {
        auto rect = bounds.removeFromLeft(bandW).reduced(2.0f, 0.0f);
        float dev = snap.bandEnergiesDb[(size_t)b] - profile.bandTargets[(size_t)b].relativeDb;

        juce::Colour statusColor;
        if (dev > profile.bandTargets[(size_t)b].toleranceHighDb)
            statusColor = juce::Colour(0xffff5252); // Red (Excess / Mud / Harsh)
        else if (dev < profile.bandTargets[(size_t)b].toleranceLowDb)
            statusColor = juce::Colour(0xffffb74d); // Amber (Deficit / Thin)
        else
            statusColor = juce::Colour(0xff00e676); // Green (Optimal)

        // Pill background
        g.setColour(statusColor.withAlpha(0.18f));
        g.fillRoundedRectangle(rect, 4.0f);
        g.setColour(statusColor.withAlpha(0.7f));
        g.drawRoundedRectangle(rect, 4.0f, 1.0f);

        // Text: Short Name + Deviation
        juce::String text = juce::String(bandDefs[(size_t)b].shortName) + " " +
                            (dev >= 0.0f ? "+" : "") + juce::String(dev, 1) + "dB";
        g.setColour(statusColor);
        g.drawText(text, rect, juce::Justification::centred, false);
    }
}

} // namespace MixSense

