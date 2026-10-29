#pragma once

#include "AudioAnalyzer.h"
#include "GenreProfiles.h"
#include <vector>

namespace MixSense
{

enum class IssueSeverity
{
    Optimal = 0, // Green
    Warning,     // Amber
    Critical     // Red
};

enum class IssueCategory
{
    Bass = 0,
    Mud,
    Harshness,
    Air,
    Stereo,
    Dynamics,
    Headroom
};

struct SuggestedFixAction
{
    bool enableHighPass = false;
    float highPassFreq = 30.0f;

    bool enableEqBand[5] = { false, false, false, false, false };
    float eqFreq[5] = { 80.0f, 320.0f, 1200.0f, 3500.0f, 10000.0f };
    float eqGainDb[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float eqQ[5] = { 0.707f, 1.4f, 1.0f, 2.5f, 0.707f };

    bool enableMonoBass = false;
    bool enableGlueCompressor = false;
    float outputGainTrimDb = 0.0f;
};

struct MixIssue
{
    juce::String id;
    IssueSeverity severity;
    IssueCategory category;
    juce::String title;
    juce::String explanation;
    juce::String dawAdvice;
    SuggestedFixAction fixAction;
    bool fixApplied = false;
};

class DiagnosticEngine
{
public:
    DiagnosticEngine();
    ~DiagnosticEngine() = default;

    // Evaluates snapshot against profile and mode, producing ranked issues and health score
    void evaluate(const AnalysisSnapshot& snapshot, 
                  const GenreProfile& profile, 
                  StageMode stageMode,
                  std::vector<MixIssue>& outIssues,
                  int& outHealthScore);

    // Combines all active suggestions into a single unified corrective DSP setting
    static SuggestedFixAction consolidateFixes(const std::vector<MixIssue>& issues);
};

} // namespace MixSense

