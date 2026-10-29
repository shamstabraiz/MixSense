#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Analysis/AudioAnalyzer.h"
#include "Analysis/DiagnosticEngine.h"
#include "DSP/CorrectiveDSP.h"
#include <mutex>

namespace MixSense
{

class MixSenseAudioProcessor : public juce::AudioProcessor
{
public:
    MixSenseAudioProcessor();
    ~MixSenseAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MixSense"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Analyzer & Diagnostics access
    AudioAnalyzer& getAnalyzer() { return analyzer; }
    const AudioAnalyzer& getAnalyzer() const { return analyzer; }

    CorrectiveDSP& getDSP() { return correctiveDSP; }
    const CorrectiveDSP& getDSP() const { return correctiveDSP; }

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // Diagnostic evaluation and actions
    void updateDiagnostics();
    std::vector<MixIssue> getCurrentIssues() const;
    int getMixHealthScore() const { return currentHealthScore.load(); }

    void applyAllFixes();
    void toggleIssueFix(int issueIndex);
    void resetAllFixes();

    void triggerSmartListen();
    bool isListening() const { return analyzer.isListening(); }
    float getListenProgress() const { return analyzer.getListenProgress(); }

    GenreProfileType getActiveGenreProfileType() const;
    StageMode getActiveStageMode() const;

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    AudioAnalyzer analyzer;
    DiagnosticEngine diagnosticEngine;
    CorrectiveDSP correctiveDSP;

    // Diagnostic state
    std::vector<MixIssue> currentIssues;
    std::atomic<int> currentHealthScore { 100 };
    mutable std::mutex issuesMutex;

    SuggestedFixAction activeConsolidatedFixes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixSenseAudioProcessor)
};

} // namespace MixSense

