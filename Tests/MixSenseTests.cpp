#include <iostream>
#include <cassert>
#include <cmath>
#include "../Source/Analysis/GenreProfiles.h"
#include "../Source/Analysis/AudioAnalyzer.h"
#include "../Source/Analysis/DiagnosticEngine.h"
#include "../Source/DSP/CorrectiveDSP.h"

using namespace MixSense;

void testGenreProfiles()
{
    std::cout << "[TEST] Running testGenreProfiles..." << std::endl;
    auto pop = getGenreProfile(GenreProfileType::ModernPop);
    assert(pop.bandTargets.size() == 6);
    assert(pop.targetCrestFactorMinDb < pop.targetCrestFactorMaxDb);

    auto edm = getGenreProfile(GenreProfileType::EDMClub);
    // EDM profile should have higher sub target than Rock
    auto rock = getGenreProfile(GenreProfileType::RockAcoustic);
    assert(edm.bandTargets[(size_t)FrequencyBand::Sub].relativeDb > rock.bandTargets[(size_t)FrequencyBand::Sub].relativeDb);

    std::cout << "  -> Genre profiles valid and calibrated!" << std::endl;
}

void testDiagnosticRules()
{
    std::cout << "[TEST] Running testDiagnosticRules..." << std::endl;
    DiagnosticEngine engine;
    auto profile = getGenreProfile(GenreProfileType::ModernPop);

    // 1. Boomy Bass test
    {
        AnalysisSnapshot snap;
        snap.hasSignal = true;
        snap.peakDb = -6.0f;
        snap.rmsDb = -18.0f;
        snap.crestFactorDb = 12.0f;
        snap.phaseCorrelation = 0.9f;
        snap.bandEnergiesDb[(size_t)FrequencyBand::Sub] = 8.0f; // Excessive sub
        snap.bandEnergiesDb[(size_t)FrequencyBand::Bass] = 7.0f;

        std::vector<MixIssue> issues;
        int score = 100;
        engine.evaluate(snap, profile, StageMode::MixPreMaster, issues, score);

        bool foundBoomy = false;
        for (const auto& iss : issues)
        {
            if (iss.id == "BOOMY_BASS")
            {
                foundBoomy = true;
                assert(iss.fixAction.enableHighPass);
                assert(iss.fixAction.enableEqBand[0]);
                assert(iss.fixAction.eqGainDb[0] < 0.0f);
            }
        }
        assert(foundBoomy);
        assert(score < 90);
        std::cout << "  -> Boomy Bass detection passed! (Score: " << score << ")" << std::endl;
    }

    // 2. Phase Cancellation & Stereo Sub-Bass test
    {
        AnalysisSnapshot snap;
        snap.hasSignal = true;
        snap.peakDb = -3.0f;
        snap.rmsDb = -14.0f;
        snap.crestFactorDb = 11.0f;
        snap.phaseCorrelation = -0.6f; // Severe phase cancellation
        snap.stereoWidthSubBass = 0.35f; // Wide stereo sub

        std::vector<MixIssue> issues;
        int score = 100;
        engine.evaluate(snap, profile, StageMode::MixPreMaster, issues, score);

        bool foundPhase = false;
        bool foundStereoSub = false;
        for (const auto& iss : issues)
        {
            if (iss.id == "PHASE_CANCELLATION") foundPhase = true;
            if (iss.id == "STEREO_SUB_BASS")
            {
                foundStereoSub = true;
                assert(iss.fixAction.enableMonoBass);
            }
        }
        assert(foundPhase);
        assert(foundStereoSub);
        assert(score <= 70);
        std::cout << "  -> Phase cancellation & Stereo Sub detection passed! (Score: " << score << ")" << std::endl;
    }

    // 3. Muddy Low-Mids & Harsh Resonance test
    {
        AnalysisSnapshot snap;
        snap.hasSignal = true;
        snap.peakDb = -4.0f;
        snap.rmsDb = -15.0f;
        snap.crestFactorDb = 11.0f;
        snap.phaseCorrelation = 0.85f;
        snap.bandEnergiesDb[(size_t)FrequencyBand::LowMid] = 4.5f; // Mud buildup

        ResonantPeak res;
        res.frequencyHz = 3520.0f;
        res.prominenceDb = 10.5f;
        res.noteName = "A7";
        snap.detectedResonances.push_back(res);

        std::vector<MixIssue> issues;
        int score = 100;
        engine.evaluate(snap, profile, StageMode::MixPreMaster, issues, score);

        bool foundMud = false;
        bool foundResonance = false;
        for (const auto& iss : issues)
        {
            if (iss.id == "MUDDY_LOW_MIDS")
            {
                foundMud = true;
                assert(iss.fixAction.enableEqBand[1]);
                assert(iss.fixAction.eqGainDb[1] < 0.0f);
            }
            if (iss.id == "HARSH_RESONANCE")
            {
                foundResonance = true;
                assert(iss.fixAction.enableEqBand[3]);
                assert(std::abs(iss.fixAction.eqFreq[3] - 3520.0f) < 5.0f);
            }
        }
        assert(foundMud);
        assert(foundResonance);
        std::cout << "  -> Muddy low-mids and harsh resonance detection passed!" << std::endl;
    }

    // 4. Over-compressed vs Clipping
    {
        AnalysisSnapshot snap;
        snap.hasSignal = true;
        snap.peakDb = 1.2f; // Clipping!
        snap.rmsDb = -3.5f;
        snap.crestFactorDb = 4.7f; // Heavily squashed dynamics

        std::vector<MixIssue> issues;
        int score = 100;
        engine.evaluate(snap, profile, StageMode::MasterStreaming, issues, score);

        bool foundClip = false;
        bool foundOverCompressed = false;
        for (const auto& iss : issues)
        {
            if (iss.id == "CLIPPING") foundClip = true;
            if (iss.id == "OVER_COMPRESSED") foundOverCompressed = true;
        }
        assert(foundClip);
        assert(foundOverCompressed);
        std::cout << "  -> Clipping and squashed dynamics detection passed!" << std::endl;
    }
}

void testCorrectiveDSP()
{
    std::cout << "[TEST] Running testCorrectiveDSP..." << std::endl;
    CorrectiveDSP dsp;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    dsp.prepare(spec);

    SuggestedFixAction fix;
    fix.enableHighPass = true;
    fix.highPassFreq = 30.0f;
    fix.enableEqBand[0] = true;
    fix.eqFreq[0] = 80.0f;
    fix.eqGainDb[0] = -4.0f;
    fix.enableMonoBass = true;

    dsp.setFixParameters(fix);
    dsp.setAssistantStrength(1.0f);

    // Response at 80 Hz should show negative gain
    float dbAt80 = dsp.getResponseAtFrequency(80.0f);
    assert(dbAt80 < -2.0f);

    // Scale strength to 0.0 -> response should be close to 0 dB
    dsp.setAssistantStrength(0.0f);
    float dbAt80ZeroStrength = dsp.getResponseAtFrequency(80.0f);
    assert(std::abs(dbAt80ZeroStrength) < 0.1f);

    // Test audio processing without crash or NaNs
    juce::AudioBuffer<float> buffer(2, 512);
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < 512; ++i)
        {
            buffer.setSample(ch, i, std::sin(2.0f * 3.14159f * 100.0f * (float)i / 48000.0f));
        }
    }

    dsp.setAssistantStrength(1.0f);
    dsp.process(buffer);

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < 512; ++i)
        {
            float s = buffer.getSample(ch, i);
            assert(!std::isnan(s) && !std::isinf(s));
        }
    }

    std::cout << "  -> Corrective DSP processing & parameter scaling verified!" << std::endl;
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "     MixSense Automated Test Suite     " << std::endl;
    std::cout << "========================================" << std::endl;

    testGenreProfiles();
    testDiagnosticRules();
    testCorrectiveDSP();

    std::cout << "========================================" << std::endl;
    std::cout << "  ALL TESTS PASSED SUCCESSFULLY (3/3)   " << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}

