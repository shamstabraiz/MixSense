#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "GenreProfiles.h"
#include <array>
#include <vector>
#include <atomic>
#include <mutex>

namespace MixSense
{

struct ResonantPeak
{
    float frequencyHz = 0.0f;
    float prominenceDb = 0.0f;
    juce::String noteName;
};

struct AnalysisSnapshot
{
    // Spectral
    std::array<float, 6> bandEnergiesDb { 0.0f }; // Deviation from reference
    std::vector<ResonantPeak> detectedResonances;

    // Loudness & Dynamics
    float peakDb = -100.0f;
    float rmsDb = -100.0f;
    float crestFactorDb = 0.0f;
    float shortTermLufs = -100.0f;

    // Stereo
    float phaseCorrelation = 1.0f; // [-1.0, +1.0]
    float stereoWidthSubBass = 0.0f; // ratio of side to mid below 120 Hz
    float overallStereoWidth = 1.0f;

    // Validity
    bool hasSignal = false;
};

class AudioAnalyzer
{
public:
    AudioAnalyzer();
    ~AudioAnalyzer() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    // Real-time audio feed (called on real-time audio thread)
    void processBlock(const juce::AudioBuffer<float>& buffer);

    // Analysis polling (called on UI thread / diagnostic thread, ~30-60 Hz)
    void updateAnalysis();

    // Retrieve latest analysis snapshot
    AnalysisSnapshot getLatestSnapshot() const;

    // Direct FFT spectrum points for GUI visualizer (e.g. 512 points)
    void getSpectrumMagnitudes(std::vector<float>& destMags) const;

    // "Smart Listen" mode controls
    void startSmartListen(double durationSeconds = 7.0);
    void stopSmartListen();
    bool isListening() const { return smartListenActive.load(); }
    float getListenProgress() const;
    AnalysisSnapshot getSmartListenSnapshot() const;

    static juce::String frequencyToNoteName(float freqHz);

private:
    double currentSampleRate = 44100.0;

    // FFT setup
    static constexpr int fftOrder = 12; // 4096 points for accurate sub/bass resolution
    static constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    // SPSC Fifo for audio thread -> analysis thread
    std::array<float, fftSize * 2> fifoBuffer;
    int fifoIndex = 0;
    std::array<float, fftSize * 2> fftData;
    std::atomic<bool> nextFFTBlockReady { false };

    // Smoothed FFT Spectrum
    std::vector<float> smoothedSpectrum;
    mutable std::mutex spectrumMutex;

    // Sub-bass filter for Mid/Side width check
    juce::dsp::IIR::Filter<float> midSubFilter;
    juce::dsp::IIR::Filter<float> sideSubFilter;

    // Rolling instantaneous metrics
    std::atomic<float> currentPeak { 0.0f };
    std::atomic<float> currentRms { 0.0f };
    std::atomic<float> currentCorrelation { 1.0f };
    std::atomic<float> currentSubSideRatio { 0.0f };

    // Analysis results
    AnalysisSnapshot currentSnapshot;
    mutable std::mutex snapshotMutex;

    // Smart Listen session accumulator
    std::atomic<bool> smartListenActive { false };
    std::atomic<double> listenDurationTarget { 7.0 };
    std::atomic<double> listenElapsedSeconds { 0.0 };
    
    // Accumulator data
    std::vector<AnalysisSnapshot> accumulatedSnapshots;
    AnalysisSnapshot averagedSnapshot;
    mutable std::mutex listenMutex;

    void computeBandEnergies(const float* fftMagnitudes, std::array<float, 6>& bandDbs);
    void detectResonantPeaks(const float* fftMagnitudes, std::vector<ResonantPeak>& peaks);
};

} // namespace MixSense

