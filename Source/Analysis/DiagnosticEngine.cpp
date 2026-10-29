#include "DiagnosticEngine.h"
#include <cmath>
#include <algorithm>

namespace MixSense
{

DiagnosticEngine::DiagnosticEngine() = default;

void DiagnosticEngine::evaluate(const AnalysisSnapshot& snapshot,
                                const GenreProfile& profile,
                                StageMode stageMode,
                                std::vector<MixIssue>& outIssues,
                                int& outHealthScore)
{
    outIssues.clear();

    if (!snapshot.hasSignal)
    {
        outHealthScore = 100;
        MixIssue info;
        info.id = "WAITING_SIGNAL";
        info.severity = IssueSeverity::Optimal;
        info.category = IssueCategory::Headroom;
        info.title = "Waiting for Audio Playback...";
        info.explanation = "Play your track in FL Studio to start real-time mix analysis.";
        info.dawAdvice = "Insert MixSense on the Master Track (or individual bus/submix) and press Play in FL Studio.";
        outIssues.push_back(info);
        return;
    }

    int penalty = 0;

    // 1. Headroom & Clipping
    if (snapshot.peakDb >= 0.0f)
    {
        MixIssue issue;
        issue.id = "CLIPPING";
        issue.severity = IssueSeverity::Critical;
        issue.category = IssueCategory::Headroom;
        issue.title = juce::String::formatted("Digital Clipping (+%.1f dBFS)", snapshot.peakDb);
        issue.explanation = "Audio is exceeding 0 dBFS, causing harsh inter-sample distortion and digital clipping.";
        issue.dawAdvice = "Lower the channel faders or master volume in FL Studio. Never let your master peak above 0 dBFS.";
        issue.fixAction.outputGainTrimDb = -(snapshot.peakDb + 0.5f);
        outIssues.push_back(issue);
        penalty += 25;
    }
    else if (stageMode == StageMode::MixPreMaster && snapshot.peakDb > -3.0f)
    {
        MixIssue issue;
        issue.id = "HOT_PREMASTER";
        issue.severity = IssueSeverity::Warning;
        issue.category = IssueCategory::Headroom;
        issue.title = juce::String::formatted("Pre-Master Too Hot (Peak: %.1f dBFS)", snapshot.peakDb);
        issue.explanation = "For pre-master mixing, leaving 3 to 6 dB of headroom gives limiters and mastering processing room to breathe.";
        issue.dawAdvice = "Trim input gain or group bus faders down by ~3 dB to maintain clean pre-master headroom.";
        issue.fixAction.outputGainTrimDb = -3.0f;
        outIssues.push_back(issue);
        penalty += 10;
    }

    // 2. Stereo Phase Correlation
    if (snapshot.phaseCorrelation < 0.0f)
    {
        MixIssue issue;
        issue.id = "PHASE_CANCELLATION";
        issue.severity = IssueSeverity::Critical;
        issue.category = IssueCategory::Stereo;
        issue.title = juce::String::formatted("Phase Cancellation Warning (Corr: %.2f)", snapshot.phaseCorrelation);
        issue.explanation = "Negative phase correlation detected! Left and right channels are canceling out when summed to mono (phones, laptops, club PAs).";
        issue.dawAdvice = "Check wideners, Haas delay plugins, or stereo synth patches. In FL Studio, test the mono sum knob in the Master track to hear what disappears.";
        outIssues.push_back(issue);
        penalty += 20;
    }
    else if (snapshot.phaseCorrelation < 0.25f)
    {
        MixIssue issue;
        issue.id = "LOW_PHASE_CORRELATION";
        issue.severity = IssueSeverity::Warning;
        issue.category = IssueCategory::Stereo;
        issue.title = juce::String::formatted("Weak Mono Compatibility (Corr: %.2f)", snapshot.phaseCorrelation);
        issue.explanation = "Mix is extremely wide with low phase correlation. Transients and punch may sound hollow in mono.";
        issue.dawAdvice = "Moderate excessive chorus, stereo detuning, or reverb width on rhythm elements.";
        outIssues.push_back(issue);
        penalty += 10;
    }

    // 3. Sub-Bass Stereo Width (< 120 Hz)
    if (snapshot.stereoWidthSubBass > profile.maxStereoWidthSubBass)
    {
        MixIssue issue;
        issue.id = "STEREO_SUB_BASS";
        issue.severity = IssueSeverity::Critical;
        issue.category = IssueCategory::Stereo;
        issue.title = "Wide Stereo Information in Sub-Bass (< 120 Hz)";
        issue.explanation = "Stereo width detected in frequencies below 120 Hz. Out-of-phase low-end causes weak speaker cone excursion and loss of punch on club subwoofers.";
        issue.dawAdvice = "Mono your low-end below 120 Hz. In FL Studio, use Fruity Stereo Shaper or turn the lower stereo separation knob fully mono on bass tracks.";
        issue.fixAction.enableMonoBass = true;
        outIssues.push_back(issue);
        penalty += 15;
    }

    // 4. Sub & Bass Balance
    const float subDev = snapshot.bandEnergiesDb[(size_t)FrequencyBand::Sub] - profile.bandTargets[(size_t)FrequencyBand::Sub].relativeDb;
    const float bassDev = snapshot.bandEnergiesDb[(size_t)FrequencyBand::Bass] - profile.bandTargets[(size_t)FrequencyBand::Bass].relativeDb;

    if (subDev > profile.bandTargets[(size_t)FrequencyBand::Sub].toleranceHighDb ||
        bassDev > profile.bandTargets[(size_t)FrequencyBand::Bass].toleranceHighDb)
    {
        MixIssue issue;
        issue.id = "BOOMY_BASS";
        issue.severity = IssueSeverity::Critical;
        issue.category = IssueCategory::Bass;
        const float maxExcess = std::max(subDev, bassDev);
        issue.title = juce::String::formatted("Boomy Bass Buildup (+%.1f dB)", maxExcess);
        issue.explanation = "Excessive low-end energy eats up dynamic headroom, triggering master limiters prematurely and causing muddy pumping.";
        issue.dawAdvice = "Apply a 24 dB/oct high-pass filter at 28-35 Hz to remove subsonic rumble. In FL Studio Fruity Parametric EQ 2, notch out 808/kick clashes.";
        issue.fixAction.enableHighPass = true;
        issue.fixAction.highPassFreq = 30.0f;
        issue.fixAction.enableEqBand[0] = true;
        issue.fixAction.eqFreq[0] = 75.0f;
        issue.fixAction.eqGainDb[0] = -std::min(5.0f, maxExcess * 0.75f);
        issue.fixAction.eqQ[0] = 0.8f;
        outIssues.push_back(issue);
        penalty += 15;
    }
    else if (subDev < profile.bandTargets[(size_t)FrequencyBand::Sub].toleranceLowDb &&
             bassDev < profile.bandTargets[(size_t)FrequencyBand::Bass].toleranceLowDb)
    {
        MixIssue issue;
        issue.id = "THIN_BASS";
        issue.severity = IssueSeverity::Warning;
        issue.category = IssueCategory::Bass;
        issue.title = "Thin Low-End / Lack of Weight";
        issue.explanation = "Low-end is significantly weaker than the reference target for this genre. Mix may sound lightweight and hollow on larger speakers.";
        issue.dawAdvice = "Add weight with a gentle low-shelf boost around 70-90 Hz or verify kick and 808 aren't destructively phase canceling.";
        issue.fixAction.enableEqBand[0] = true;
        issue.fixAction.eqFreq[0] = 80.0f;
        issue.fixAction.eqGainDb[0] = 2.5f;
        issue.fixAction.eqQ[0] = 0.707f;
        outIssues.push_back(issue);
        penalty += 10;
    }

    // 5. Low-Mid Mud (250 - 500 Hz)
    const float mudDev = snapshot.bandEnergiesDb[(size_t)FrequencyBand::LowMid] - profile.bandTargets[(size_t)FrequencyBand::LowMid].relativeDb;
    if (mudDev > profile.bandTargets[(size_t)FrequencyBand::LowMid].toleranceHighDb)
    {
        MixIssue issue;
        issue.id = "MUDDY_LOW_MIDS";
        issue.severity = IssueSeverity::Warning;
        issue.category = IssueCategory::Mud;
        issue.title = juce::String::formatted("Boxy / Muddy Buildup in 250-500 Hz (+%.1f dB)", mudDev);
        issue.explanation = "The 250-500 Hz zone contains overlapping warmth from synths, guitars, vocals, and bass harmonics, resulting in a cluttered 'cardboard' sound.";
        issue.dawAdvice = "High-pass non-bass instruments (guitars, synths, vocals) at 120-150 Hz. In FL Studio, use Parametric EQ 2 to dip 2-3 dB around 300-350 Hz.";
        issue.fixAction.enableEqBand[1] = true;
        issue.fixAction.eqFreq[1] = 320.0f;
        issue.fixAction.eqGainDb[1] = -std::min(4.5f, mudDev * 0.8f);
        issue.fixAction.eqQ[1] = 1.4f;
        outIssues.push_back(issue);
        penalty += 12;
    }

    // 6. Upper-Mid Harshness & Resonances
    const float harshDev = snapshot.bandEnergiesDb[(size_t)FrequencyBand::UpperMid] - profile.bandTargets[(size_t)FrequencyBand::UpperMid].relativeDb;
    if (!snapshot.detectedResonances.empty())
    {
        const auto& topRes = snapshot.detectedResonances[0];
        MixIssue issue;
        issue.id = "HARSH_RESONANCE";
        issue.severity = IssueSeverity::Critical;
        issue.category = IssueCategory::Harshness;
        issue.title = juce::String::formatted("Ringing Resonance at %.0f Hz (%s)", topRes.frequencyHz, topRes.noteName.toRawUTF8());
        issue.explanation = juce::String::formatted("A sharp resonance spike (+%.1f dB prominence) detected at %.0f Hz (%s), causing piercing listener fatigue.",
                                                    topRes.prominenceDb, topRes.frequencyHz, topRes.noteName.toRawUTF8());
        issue.dawAdvice = juce::String::formatted("In FL Studio Parametric EQ 2, create a narrow bell notch (high Q) at %.0f Hz and cut 3 to 5 dB.", topRes.frequencyHz);
        issue.fixAction.enableEqBand[3] = true;
        issue.fixAction.eqFreq[3] = topRes.frequencyHz;
        issue.fixAction.eqGainDb[3] = -std::min(5.0f, topRes.prominenceDb * 0.5f);
        issue.fixAction.eqQ[3] = 2.8f;
        outIssues.push_back(issue);
        penalty += 15;
    }
    else if (harshDev > profile.bandTargets[(size_t)FrequencyBand::UpperMid].toleranceHighDb)
    {
        MixIssue issue;
        issue.id = "HARSH_UPPER_MIDS";
        issue.severity = IssueSeverity::Warning;
        issue.category = IssueCategory::Harshness;
        issue.title = juce::String::formatted("Harsh Upper-Mid Energy (+%.1f dB)", harshDev);
        issue.explanation = "2.5 kHz to 5 kHz is the ear's most sensitive frequency range. Excess energy here sounds strident and fatiguing at higher listening levels.";
        issue.dawAdvice = "Tame harsh vocal sibilance with Maximus / Fruity De-Esser or apply a broad 2 dB dip between 3 kHz and 4 kHz.";
        issue.fixAction.enableEqBand[3] = true;
        issue.fixAction.eqFreq[3] = 3600.0f;
        issue.fixAction.eqGainDb[3] = -std::min(4.0f, harshDev * 0.7f);
        issue.fixAction.eqQ[3] = 1.5f;
        outIssues.push_back(issue);
        penalty += 10;
    }

    // 7. High End / Air (6-20 kHz)
    const float airDev = snapshot.bandEnergiesDb[(size_t)FrequencyBand::Air] - profile.bandTargets[(size_t)FrequencyBand::Air].relativeDb;
    if (airDev < profile.bandTargets[(size_t)FrequencyBand::Air].toleranceLowDb)
    {
        MixIssue issue;
        issue.id = "DULL_HIGHS";
        issue.severity = IssueSeverity::Warning;
        issue.category = IssueCategory::Air;
        issue.title = "Dull / Dark High-End";
        issue.explanation = "Mix lacks top-end air and sheen above 8 kHz, making it feel distant and closed-in.";
        issue.dawAdvice = "Add a gentle high-shelf boost (+2 dB) at 10-12 kHz using Fruity Parametric EQ 2 or exciter saturation on vocals/cymbals.";
        issue.fixAction.enableEqBand[4] = true;
        issue.fixAction.eqFreq[4] = 10000.0f;
        issue.fixAction.eqGainDb[4] = 2.0f;
        issue.fixAction.eqQ[4] = 0.707f;
        outIssues.push_back(issue);
        penalty += 8;
    }

    // 8. Dynamic Range & Crest Factor
    if (snapshot.crestFactorDb < profile.targetCrestFactorMinDb)
    {
        MixIssue issue;
        issue.id = "OVER_COMPRESSED";
        issue.severity = IssueSeverity::Warning;
        issue.category = IssueCategory::Dynamics;
        issue.title = juce::String::formatted("Over-Compressed / Squashed Dynamics (Crest: %.1f dB)", snapshot.crestFactorDb);
        issue.explanation = "The difference between peaks and average loudness is too low. Transients have lost impact and punch.";
        issue.dawAdvice = "Back off limiters or heavy bus compressors in FL Studio. Slower compressor attack (20-30 ms) lets drum transients punch through.";
        outIssues.push_back(issue);
        penalty += 12;
    }
    else if (snapshot.crestFactorDb > profile.targetCrestFactorMaxDb)
    {
        MixIssue issue;
        issue.id = "SPIKY_TRANSIENTS";
        issue.severity = IssueSeverity::Warning;
        issue.category = IssueCategory::Dynamics;
        issue.title = juce::String::formatted("Spiky / Uncontrolled Transients (Crest: %.1f dB)", snapshot.crestFactorDb);
        issue.explanation = "Peaks are significantly louder than the body of the mix, preventing the track from achieving competitive perceived loudness.";
        issue.dawAdvice = "Apply gentle bus glue compression (2:1 ratio, 20 ms attack, auto release) or soft clip transient peaks with Fruity Soft Clipper.";
        issue.fixAction.enableGlueCompressor = true;
        outIssues.push_back(issue);
        penalty += 8;
    }

    // If no issues were flagged, give an Optimal card
    if (outIssues.empty())
    {
        MixIssue good;
        good.id = "MIX_OPTIMAL";
        good.severity = IssueSeverity::Optimal;
        good.category = IssueCategory::Bass;
        good.title = "Tonal Balance & Dynamics Optimal!";
        good.explanation = "Spectral balance, low-end mono compatibility, phase correlation, and dynamic range match your target genre profile.";
        good.dawAdvice = "Mix is clean and translation-ready. Ready for final mastering.";
        outIssues.push_back(good);
    }

    outHealthScore = std::max(0, 100 - penalty);
}

SuggestedFixAction DiagnosticEngine::consolidateFixes(const std::vector<MixIssue>& issues)
{
    SuggestedFixAction result;

    for (const auto& issue : issues)
    {
        if (issue.fixAction.enableHighPass)
        {
            result.enableHighPass = true;
            result.highPassFreq = std::max(result.highPassFreq, issue.fixAction.highPassFreq);
        }

        if (issue.fixAction.enableMonoBass)
        {
            result.enableMonoBass = true;
        }

        if (issue.fixAction.enableGlueCompressor)
        {
            result.enableGlueCompressor = true;
        }

        if (std::abs(issue.fixAction.outputGainTrimDb) > std::abs(result.outputGainTrimDb))
        {
            result.outputGainTrimDb = issue.fixAction.outputGainTrimDb;
        }

        for (int b = 0; b < 5; ++b)
        {
            if (issue.fixAction.enableEqBand[b])
            {
                result.enableEqBand[b] = true;
                result.eqFreq[b] = issue.fixAction.eqFreq[b];
                result.eqGainDb[b] = issue.fixAction.eqGainDb[b];
                result.eqQ[b] = issue.fixAction.eqQ[b];
            }
        }
    }

    return result;
}

} // namespace MixSense

