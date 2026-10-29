#include "PluginProcessor.h"
#include "UI/PluginEditor.h"

namespace MixSense
{

MixSenseAudioProcessor::MixSenseAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

MixSenseAudioProcessor::~MixSenseAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout MixSenseAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    juce::StringArray genres { "Modern Pop", "EDM / Club", "Hip-Hop / Trap", "Rock / Acoustic", "Vocal / Lead" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "genreProfile", 1 }, "Genre Profile", genres, 0));

    juce::StringArray stages { "Mix / Pre-Master", "Master / Streaming" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "stageMode", 1 }, "Stage Mode", stages, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "assistantStrength", 1 }, "Assistant Strength",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "bypassed", 1 }, "Bypass All Fixes", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "deltaMode", 1 }, "Delta Listen Mode", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "monoBassManual", 1 }, "Mono Bass < 120Hz", false));

    return { params.begin(), params.end() };
}

void MixSenseAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    analyzer.prepare(sampleRate, samplesPerBlock);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    spec.numChannels = (juce::uint32)getTotalNumOutputChannels();
    correctiveDSP.prepare(spec);

    resetAllFixes();
}

void MixSenseAudioProcessor::releaseResources()
{
    analyzer.reset();
    correctiveDSP.reset();
}

bool MixSenseAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return (mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo());
}

void MixSenseAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Sync parameters from APVTS
    float strength = apvts.getRawParameterValue("assistantStrength")->load();
    bool bypass = apvts.getRawParameterValue("bypassed")->load() > 0.5f;
    bool delta = apvts.getRawParameterValue("deltaMode")->load() > 0.5f;

    correctiveDSP.setAssistantStrength(strength);
    correctiveDSP.setBypassed(bypass);
    correctiveDSP.setDeltaMode(delta);

    // Analyze incoming audio
    analyzer.processBlock(buffer);

    // Apply corrective DSP processing in-place
    correctiveDSP.process(buffer);
}

void MixSenseAudioProcessor::updateDiagnostics()
{
    analyzer.updateAnalysis();

    AnalysisSnapshot snap = analyzer.isListening() || analyzer.getListenProgress() >= 1.0f
                                ? analyzer.getSmartListenSnapshot()
                                : analyzer.getLatestSnapshot();

    GenreProfile profile = getGenreProfile(getActiveGenreProfileType());
    StageMode stage = getActiveStageMode();

    std::vector<MixIssue> newIssues;
    int health = 100;
    diagnosticEngine.evaluate(snap, profile, stage, newIssues, health);

    // Preserve fixApplied state if issues match
    {
        std::lock_guard<std::mutex> lock(issuesMutex);
        for (auto& newIss : newIssues)
        {
            for (const auto& oldIss : currentIssues)
            {
                if (newIss.id == oldIss.id)
                {
                    newIss.fixApplied = oldIss.fixApplied;
                    break;
                }
            }
        }
        currentIssues = newIssues;
        currentHealthScore.store(health);
    }
}

std::vector<MixIssue> MixSenseAudioProcessor::getCurrentIssues() const
{
    std::lock_guard<std::mutex> lock(issuesMutex);
    return currentIssues;
}

void MixSenseAudioProcessor::applyAllFixes()
{
    std::lock_guard<std::mutex> lock(issuesMutex);
    std::vector<MixIssue> activeIssues;

    for (auto& issue : currentIssues)
    {
        issue.fixApplied = true;
        activeIssues.push_back(issue);
    }

    activeConsolidatedFixes = DiagnosticEngine::consolidateFixes(activeIssues);
    correctiveDSP.setFixParameters(activeConsolidatedFixes);
}

void MixSenseAudioProcessor::toggleIssueFix(int issueIndex)
{
    std::lock_guard<std::mutex> lock(issuesMutex);
    if (issueIndex < 0 || issueIndex >= (int)currentIssues.size())
        return;

    currentIssues[(size_t)issueIndex].fixApplied = !currentIssues[(size_t)issueIndex].fixApplied;

    std::vector<MixIssue> activeIssues;
    for (const auto& issue : currentIssues)
    {
        if (issue.fixApplied)
            activeIssues.push_back(issue);
    }

    activeConsolidatedFixes = DiagnosticEngine::consolidateFixes(activeIssues);
    correctiveDSP.setFixParameters(activeConsolidatedFixes);
}

void MixSenseAudioProcessor::resetAllFixes()
{
    std::lock_guard<std::mutex> lock(issuesMutex);
    for (auto& issue : currentIssues)
    {
        issue.fixApplied = false;
    }
    activeConsolidatedFixes = SuggestedFixAction();
    correctiveDSP.setFixParameters(activeConsolidatedFixes);
}

void MixSenseAudioProcessor::triggerSmartListen()
{
    analyzer.startSmartListen(7.0); // 7 second capture
}

GenreProfileType MixSenseAudioProcessor::getActiveGenreProfileType() const
{
    int index = (int)apvts.getRawParameterValue("genreProfile")->load();
    return static_cast<GenreProfileType>(juce::jlimit(0, 4, index));
}

StageMode MixSenseAudioProcessor::getActiveStageMode() const
{
    int index = (int)apvts.getRawParameterValue("stageMode")->load();
    return static_cast<StageMode>(juce::jlimit(0, 1, index));
}

juce::AudioProcessorEditor* MixSenseAudioProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void MixSenseAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MixSenseAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

} // namespace MixSense

// Factory function for JUCE
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MixSense::MixSenseAudioProcessor();
}

