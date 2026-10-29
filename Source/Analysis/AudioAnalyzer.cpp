#include "AudioAnalyzer.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace MixSense
{

AudioAnalyzer::AudioAnalyzer()
    : forwardFFT(fftOrder),
      window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    fifoBuffer.fill(0.0f);
    fftData.fill(0.0f);
    smoothedSpectrum.resize(fftSize / 2, -100.0f);
}

void AudioAnalyzer::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    
    // 120 Hz 2nd order Butterworth Low-pass filter for sub-bass M/S width inspection
    auto filterCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, 120.0f, 0.7071f);
    midSubFilter.coefficients = filterCoeffs;
    sideSubFilter.coefficients = filterCoeffs;

    reset();
}

void AudioAnalyzer::reset()
{
    fifoIndex = 0;
    fifoBuffer.fill(0.0f);
    fftData.fill(0.0f);
    nextFFTBlockReady.store(false);

    {
        std::lock_guard<std::mutex> lock(spectrumMutex);
        std::fill(smoothedSpectrum.begin(), smoothedSpectrum.end(), -100.0f);
    }

    midSubFilter.reset();
    sideSubFilter.reset();

    currentPeak.store(0.0f);
    currentRms.store(0.0f);
    currentCorrelation.store(1.0f);
    currentSubSideRatio.store(0.0f);

    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        currentSnapshot = AnalysisSnapshot();
    }

    stopSmartListen();
}

void AudioAnalyzer::processBlock(const juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    const float* left = buffer.getReadPointer(0);
    const float* right = numChannels > 1 ? buffer.getReadPointer(1) : left;

    // Peak, RMS, Phase Correlation & Sub-bass M/S metrics
    float blockPeak = 0.0f;
    double sumSquares = 0.0;
    double sumLR = 0.0;
    double sumLL = 0.0;
    double sumRR = 0.0;
    double midSubSumSq = 0.0;
    double sideSubSumSq = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = left[i];
        const float r = right[i];
        const float mono = 0.5f * (l + r);

        // Peak
        blockPeak = std::max(blockPeak, std::max(std::abs(l), std::abs(r)));

        // RMS & Correlation
        sumSquares += (l * l + r * r) * 0.5;
        sumLR += (double)l * (double)r;
        sumLL += (double)l * (double)l;
        sumRR += (double)r * (double)r;

        // Sub Mid/Side filtering
        const float mid = (l + r) * 0.7071f;
        const float side = (l - r) * 0.7071f;

        const float midSub = midSubFilter.processSample(mid);
        const float sideSub = sideSubFilter.processSample(side);

        midSubSumSq += midSub * midSub;
        sideSubSumSq += sideSub * sideSub;

        // Push mono sample to FFT FIFO
        if (fifoIndex < fftSize)
        {
            fifoBuffer[(size_t)fifoIndex++] = mono;
        }
        else
        {
            if (!nextFFTBlockReady.load(std::memory_order_relaxed))
            {
                std::copy(fifoBuffer.begin(), fifoBuffer.begin() + fftSize, fftData.begin());
                std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
                nextFFTBlockReady.store(true, std::memory_order_release);
            }
            fifoIndex = 0;
            fifoBuffer[(size_t)fifoIndex++] = mono;
        }
    }

    // Compute instantaneous RMS & correlation
    const float blockRms = std::sqrt((float)(sumSquares / std::max(1, numSamples)));
    currentPeak.store(std::max(blockPeak, currentPeak.load() * 0.95f));
    currentRms.store(blockRms * 0.1f + currentRms.load() * 0.9f);

    const double denom = std::sqrt(std::max(1e-12, sumLL * sumRR));
    float correlation = denom > 1e-6 ? (float)(sumLR / denom) : 1.0f;
    correlation = juce::jlimit(-1.0f, 1.0f, correlation);
    currentCorrelation.store(correlation * 0.15f + currentCorrelation.load() * 0.85f);

    // Sub-bass side to mid ratio
    const float midSubRms = std::sqrt((float)(midSubSumSq / std::max(1, numSamples)));
    const float sideSubRms = std::sqrt((float)(sideSubSumSq / std::max(1, numSamples)));
    const float subRatio = sideSubRms / std::max(1e-5f, midSubRms + sideSubRms);
    currentSubSideRatio.store(subRatio * 0.1f + currentSubSideRatio.load() * 0.9f);

    // Track smart listen elapsed time
    if (smartListenActive.load())
    {
        double elapsed = listenElapsedSeconds.load() + ((double)numSamples / currentSampleRate);
        listenElapsedSeconds.store(elapsed);
        if (elapsed >= listenDurationTarget.load())
        {
            smartListenActive.store(false);
        }
    }
}

void AudioAnalyzer::updateAnalysis()
{
    // Check if new FFT frame is ready
    if (nextFFTBlockReady.load(std::memory_order_acquire))
    {
        window.multiplyWithWindowingTable(fftData.data(), fftSize);
        forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());

        const int numBins = fftSize / 2;
        const float* mags = fftData.data();

        // Update smoothed spectrum
        {
            std::lock_guard<std::mutex> lock(spectrumMutex);
            for (int i = 0; i < numBins; ++i)
            {
                // Normalize and convert to dB
                float normMag = mags[i] / (float)fftSize;
                float db = juce::Decibels::gainToDecibels(normMag, -100.0f);
                
                // Tilt correction: add +3 dB/octave relative to 1 kHz to perceptually flatten pink noise
                float freq = (float)i * (float)currentSampleRate / (float)fftSize;
                if (freq > 20.0f)
                {
                    float tilt = 3.0f * std::log2(freq / 1000.0f);
                    db += tilt;
                }

                // Smooth
                smoothedSpectrum[(size_t)i] = smoothedSpectrum[(size_t)i] * 0.75f + db * 0.25f;
            }
        }

        // Build new analysis snapshot
        AnalysisSnapshot snap;
        float peak = currentPeak.load();
        float rms = currentRms.load();

        snap.peakDb = juce::Decibels::gainToDecibels(peak, -100.0f);
        snap.rmsDb = juce::Decibels::gainToDecibels(rms, -100.0f);
        snap.crestFactorDb = std::max(0.0f, snap.peakDb - snap.rmsDb);
        snap.phaseCorrelation = currentCorrelation.load();
        snap.stereoWidthSubBass = currentSubSideRatio.load();
        snap.hasSignal = (snap.peakDb > -60.0f);

        // Approximate short-term LUFS from RMS with K-weighting offset (~ -0.6 dB adjustment)
        snap.shortTermLufs = snap.rmsDb - 0.6f;

        computeBandEnergies(smoothedSpectrum.data(), snap.bandEnergiesDb);
        detectResonantPeaks(smoothedSpectrum.data(), snap.detectedResonances);

        {
            std::lock_guard<std::mutex> lock(snapshotMutex);
            currentSnapshot = snap;
        }

        // Accumulate for Smart Listen session
        if (smartListenActive.load() && snap.hasSignal)
        {
            std::lock_guard<std::mutex> lock(listenMutex);
            accumulatedSnapshots.push_back(snap);

            // Recompute running average snapshot
            if (!accumulatedSnapshots.empty())
            {
                AnalysisSnapshot avg;
                avg.hasSignal = true;
                const size_t count = accumulatedSnapshots.size();

                double sumPeak = 0.0, sumRms = 0.0, sumCrest = 0.0, sumLufs = 0.0;
                double sumCorr = 0.0, sumSubWidth = 0.0;
                std::array<double, 6> sumBands { 0.0 };

                for (const auto& s : accumulatedSnapshots)
                {
                    sumPeak += s.peakDb;
                    sumRms += s.rmsDb;
                    sumCrest += s.crestFactorDb;
                    sumLufs += s.shortTermLufs;
                    sumCorr += s.phaseCorrelation;
                    sumSubWidth += s.stereoWidthSubBass;
                    for (int b = 0; b < 6; ++b)
                        sumBands[(size_t)b] += s.bandEnergiesDb[(size_t)b];
                }

                avg.peakDb = (float)(sumPeak / count);
                avg.rmsDb = (float)(sumRms / count);
                avg.crestFactorDb = (float)(sumCrest / count);
                avg.shortTermLufs = (float)(sumLufs / count);
                avg.phaseCorrelation = (float)(sumCorr / count);
                avg.stereoWidthSubBass = (float)(sumSubWidth / count);

                for (int b = 0; b < 6; ++b)
                    avg.bandEnergiesDb[(size_t)b] = (float)(sumBands[(size_t)b] / count);

                // Take latest detected resonances
                avg.detectedResonances = snap.detectedResonances;
                averagedSnapshot = avg;
            }
        }

        nextFFTBlockReady.store(false, std::memory_order_release);
    }
}

void AudioAnalyzer::computeBandEnergies(const float* fftMagnitudes, std::array<float, 6>& bandDbs)
{
    const int numBins = fftSize / 2;
    const auto& bandDefs = getBandDefinitions();
    
    // Overall spectral energy for normalization
    double totalEnergy = 0.0;
    int totalCount = 0;

    std::array<double, 6> bandSums { 0.0 };
    std::array<int, 6> bandCounts { 0 };

    for (int i = 1; i < numBins; ++i)
    {
        const float freq = (float)i * (float)currentSampleRate / (float)fftSize;
        if (freq < 20.0f || freq > 20000.0f)
            continue;

        const float val = fftMagnitudes[i];
        if (val < -90.0f)
            continue;

        const double linEnergy = std::pow(10.0, (double)val / 10.0);
        totalEnergy += linEnergy;
        totalCount++;

        for (int b = 0; b < 6; ++b)
        {
            if (freq >= bandDefs[(size_t)b].lowHz && freq < bandDefs[(size_t)b].highHz)
            {
                bandSums[(size_t)b] += linEnergy;
                bandCounts[(size_t)b]++;
                break;
            }
        }
    }

    const double avgTotalEnergy = totalCount > 0 ? (totalEnergy / totalCount) : 1e-9;
    const double avgTotalDb = 10.0 * std::log10(std::max(1e-9, avgTotalEnergy));

    for (int b = 0; b < 6; ++b)
    {
        if (bandCounts[(size_t)b] > 0)
        {
            double bandAvgLin = bandSums[(size_t)b] / bandCounts[(size_t)b];
            double bandDb = 10.0 * std::log10(std::max(1e-9, bandAvgLin));
            // Energy deviation relative to overall average
            bandDbs[(size_t)b] = (float)(bandDb - avgTotalDb);
        }
        else
        {
            bandDbs[(size_t)b] = 0.0f;
        }
    }
}

void AudioAnalyzer::detectResonantPeaks(const float* fftMagnitudes, std::vector<ResonantPeak>& peaks)
{
    peaks.clear();
    const int numBins = fftSize / 2;

    // Scan between 200 Hz and 10 kHz for sharp resonances
    const int minBin = (int)(200.0f * (float)fftSize / (float)currentSampleRate);
    const int maxBin = (int)(10000.0f * (float)fftSize / (float)currentSampleRate);

    for (int i = minBin + 5; i < std::min(numBins - 5, maxBin); ++i)
    {
        const float centerVal = fftMagnitudes[i];
        if (centerVal < -50.0f)
            continue;

        // Peak condition (higher than immediate neighbors)
        if (centerVal > fftMagnitudes[i - 1] && centerVal > fftMagnitudes[i + 1] &&
            centerVal > fftMagnitudes[i - 2] && centerVal > fftMagnitudes[i + 2])
        {
            // Compute average of local background neighborhood (excluding peak)
            float bgSum = 0.0f;
            int bgCount = 0;
            for (int k = -8; k <= 8; ++k)
            {
                if (std::abs(k) >= 3)
                {
                    bgSum += fftMagnitudes[i + k];
                    bgCount++;
                }
            }
            const float localBg = bgSum / (float)bgCount;
            const float prominence = centerVal - localBg;

            // If it sticks out by 7.5 dB or more, it's a harsh resonance / ringing node
            if (prominence >= 7.5f)
            {
                ResonantPeak p;
                p.frequencyHz = (float)i * (float)currentSampleRate / (float)fftSize;
                p.prominenceDb = prominence;
                p.noteName = frequencyToNoteName(p.frequencyHz);
                peaks.push_back(p);

                // Skip ahead to prevent duplicate hits on the same resonance flank
                i += 6;
                if (peaks.size() >= 3)
                    break;
            }
        }
    }
}

void AudioAnalyzer::getSpectrumMagnitudes(std::vector<float>& destMags) const
{
    std::lock_guard<std::mutex> lock(spectrumMutex);
    destMags = smoothedSpectrum;
}

AnalysisSnapshot AudioAnalyzer::getLatestSnapshot() const
{
    std::lock_guard<std::mutex> lock(snapshotMutex);
    return currentSnapshot;
}

void AudioAnalyzer::startSmartListen(double durationSeconds)
{
    std::lock_guard<std::mutex> lock(listenMutex);
    accumulatedSnapshots.clear();
    averagedSnapshot = AnalysisSnapshot();
    listenDurationTarget.store(durationSeconds);
    listenElapsedSeconds.store(0.0);
    smartListenActive.store(true);
}

void AudioAnalyzer::stopSmartListen()
{
    smartListenActive.store(false);
}

float AudioAnalyzer::getListenProgress() const
{
    double target = listenDurationTarget.load();
    if (target <= 0.0) return 0.0f;
    return juce::jlimit(0.0f, 1.0f, (float)(listenElapsedSeconds.load() / target));
}

AnalysisSnapshot AudioAnalyzer::getSmartListenSnapshot() const
{
    std::lock_guard<std::mutex> lock(listenMutex);
    if (averagedSnapshot.hasSignal)
        return averagedSnapshot;
    return getLatestSnapshot();
}

juce::String AudioAnalyzer::frequencyToNoteName(float freqHz)
{
    if (freqHz <= 0.0f)
        return "-";

    const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    float midiNum = 69.0f + 12.0f * std::log2(freqHz / 440.0f);
    int roundedMidi = (int)std::round(midiNum);
    int noteIndex = (roundedMidi % 12 + 12) % 12;
    int octave = (roundedMidi / 12) - 1;

    return juce::String(noteNames[noteIndex]) + juce::String(octave);
}

} // namespace MixSense

