#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace MixSense
{

enum class GenreProfileType
{
    ModernPop = 0,
    EDMClub,
    HipHopTrap,
    RockAcoustic,
    VocalLead,
    Count
};

enum class StageMode
{
    MixPreMaster = 0,
    MasterStreaming,
    Count
};

enum class FrequencyBand
{
    Sub = 0,       // 20 - 60 Hz
    Bass,          // 60 - 250 Hz
    LowMid,        // 250 - 500 Hz (Mud zone)
    Mid,           // 500 - 2000 Hz (Body / Presence)
    UpperMid,      // 2000 - 6000 Hz (Harshness / Clarity)
    Air,           // 6000 - 20000 Hz (High sizzle / Sheen)
    Count
};

struct BandDefinition
{
    float lowHz;
    float highHz;
    const char* name;
    const char* shortName;
};

inline const std::array<BandDefinition, 6>& getBandDefinitions()
{
    static const std::array<BandDefinition, 6> defs {{
        { 20.0f,   60.0f,    "Sub Bass",      "SUB" },
        { 60.0f,   250.0f,   "Bass",          "BASS" },
        { 250.0f,  500.0f,   "Low Mid (Mud)", "LOW-MID" },
        { 500.0f,  2000.0f,  "Midrange",      "MID" },
        { 2000.0f, 6000.0f,  "Upper Mid",     "UP-MID" },
        { 6000.0f, 20000.0f, "High / Air",    "AIR" }
    }};
    return defs;
}

struct BandTarget
{
    float relativeDb;       // Target relative energy in dB compared to flat pink-noise reference
    float toleranceLowDb;   // Negative deviation allowed before warning
    float toleranceHighDb;  // Positive deviation allowed before warning
};

struct GenreProfile
{
    GenreProfileType type;
    const char* displayName;
    std::array<BandTarget, 6> bandTargets;
    float targetCrestFactorMinDb;
    float targetCrestFactorMaxDb;
    float targetLufsMin;
    float targetLufsMax;
    float maxStereoWidthSubBass; // Max side-to-mid ratio under 120Hz (-12 dB is safe)
};

inline GenreProfile getGenreProfile(GenreProfileType type)
{
    switch (type)
    {
        case GenreProfileType::ModernPop:
            return {
                GenreProfileType::ModernPop,
                "Modern Pop",
                {{
                    {  0.0f, -3.5f,  3.0f }, // Sub
                    {  0.5f, -3.0f,  2.5f }, // Bass
                    { -1.0f, -2.5f,  2.0f }, // LowMid (mud dip)
                    {  0.0f, -2.5f,  2.5f }, // Mid
                    {  0.5f, -2.5f,  2.5f }, // UpperMid
                    {  0.0f, -3.0f,  3.0f }  // Air
                }},
                8.0f, 14.0f, // Crest factor (dB)
                -15.0f, -12.0f, // LUFS
                0.15f // sub stereo ratio threshold
            };

        case GenreProfileType::EDMClub:
            return {
                GenreProfileType::EDMClub,
                "EDM / Club",
                {{
                    {  2.5f, -2.5f,  3.5f }, // Sub (boosted sub)
                    {  1.5f, -2.5f,  3.0f }, // Bass
                    { -1.5f, -2.5f,  1.5f }, // LowMid (strict mud suppression)
                    { -0.5f, -2.5f,  2.0f }, // Mid
                    {  1.0f, -2.0f,  2.5f }, // UpperMid (bright punch)
                    {  1.5f, -2.5f,  3.0f }  // Air (extended sizzle)
                }},
                6.0f, 11.0f,
                -11.0f, -7.0f,
                0.08f // very strict mono low-end for club sound systems
            };

        case GenreProfileType::HipHopTrap:
            return {
                GenreProfileType::HipHopTrap,
                "Hip-Hop / Trap",
                {{
                    {  3.5f, -2.5f,  4.0f }, // Sub (heavy 808 focus)
                    {  1.0f, -3.0f,  2.5f }, // Bass
                    { -2.0f, -2.5f,  1.5f }, // LowMid (scooped for vocal + 808 clarity)
                    { -0.5f, -2.5f,  2.0f }, // Mid
                    {  0.5f, -2.5f,  2.5f }, // UpperMid
                    {  0.5f, -2.5f,  3.0f }  // Air (crisp hi-hats)
                }},
                6.5f, 12.0f,
                -12.0f, -8.0f,
                0.10f
            };

        case GenreProfileType::RockAcoustic:
            return {
                GenreProfileType::RockAcoustic,
                "Rock / Acoustic",
                {{
                    { -1.5f, -3.5f,  2.5f }, // Sub (clean controlled sub)
                    {  0.5f, -2.5f,  2.5f }, // Bass (punchy kick/bass guitar)
                    {  0.0f, -2.5f,  2.5f }, // LowMid (warm body)
                    {  1.0f, -2.0f,  3.0f }, // Mid (guitar & vocal focus)
                    {  0.5f, -2.5f,  2.5f }, // UpperMid
                    { -0.5f, -3.5f,  2.5f }  // Air (natural, not over-hyped)
                }},
                9.0f, 16.0f,
                -16.0f, -12.0f,
                0.20f
            };

        case GenreProfileType::VocalLead:
            return {
                GenreProfileType::VocalLead,
                "Vocal / Lead Bus",
                {{
                    { -6.0f, -12.0f, 1.0f }, // Sub (should be high-passed!)
                    { -3.0f,  -6.0f, 1.5f }, // Bass
                    { -1.0f,  -3.0f, 2.0f }, // LowMid
                    {  1.5f,  -2.0f, 3.0f }, // Mid (intelligibility)
                    {  2.0f,  -2.0f, 3.5f }, // UpperMid (presence & air)
                    {  1.0f,  -2.5f, 3.0f }  // Air
                }},
                10.0f, 18.0f,
                -18.0f, -14.0f,
                0.35f
            };

        case GenreProfileType::Count:
        default:
            return getGenreProfile(GenreProfileType::ModernPop);
    }
}

} // namespace MixSense

