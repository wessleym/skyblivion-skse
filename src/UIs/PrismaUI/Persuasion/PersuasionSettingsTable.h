// Generated from Settings.csv by gen_settings.py - do not edit by hand.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>

namespace Persuasion
{
    enum class P : std::uint32_t
    {
        GainMult,
        LossMult,
        MaxDisposition,
        CloseOnMax,
        MaxRounds,
        UsePrismaView,
        LoveFactor,
        LikeFactor,
        DislikeFactor,
        HateFactor,
        LoveMult,
        LikeMult,
        DislikeMult,
        HateMult,
        LoveAdd,
        LikeAdd,
        DislikeAdd,
        HateAdd,
        AdmireMult,
        BoastMult,
        JokeMult,
        CoerceMult,
        AdmireAdd,
        BoastAdd,
        JokeAdd,
        CoerceAdd,
        DecayWhileOpen,
        DecayPerSecond,
        DecayPerRotation,
        DecayRateMult,
        DecayStartDelay,
        DecayFloor,
        PenaltyEnabled,
        PenaltyStart,
        PenaltyStrength,
        PenaltyCurve,
        PenaltySpeechRelief,
        PenaltyFloor,
        PenaltyOnBribe,
        BribeEnabled,
        BribeCostMult,
        BribeGainMult,
        BribePriceScale,
        BribeGainScale,
        BribeDispositionPriceDivisor,
        BribePriceCurve,
        BribeGainCurve,
        BribeNpcLevelMult,
        BribeScale,
        BribeSpeechcraftMult,
        BribeMinPrice,
        BribeMinGain,
        kCount
    };

    struct SettingDef
    {
        const char* section;
        const char* key;
        float       fallback;
    };

    inline constexpr SettingDef kSettingDefs[] = {
        { "General", "GainMult", 1.0f },
        { "General", "LossMult", 1.0f },
        { "General", "MaxDisposition", 100.0f },
        { "General", "CloseOnMax", 1.0f },
        { "General", "MaxRounds", 0.0f },
        { "General", "UsePrismaView", 0.0f },
        { "Response", "LoveFactor", 2.0f },
        { "Response", "LikeFactor", 1.0f },
        { "Response", "DislikeFactor", -1.0f },
        { "Response", "HateFactor", -3.0f },
        { "Response", "LoveMult", 1.0f },
        { "Response", "LikeMult", 1.0f },
        { "Response", "DislikeMult", 1.0f },
        { "Response", "HateMult", 1.0f },
        { "Response", "LoveAdd", 0.0f },
        { "Response", "LikeAdd", 0.0f },
        { "Response", "DislikeAdd", 0.0f },
        { "Response", "HateAdd", 0.0f },
        { "Approach", "AdmireMult", 1.0f },
        { "Approach", "BoastMult", 1.0f },
        { "Approach", "JokeMult", 1.0f },
        { "Approach", "CoerceMult", 1.0f },
        { "Approach", "AdmireAdd", 0.0f },
        { "Approach", "BoastAdd", 0.0f },
        { "Approach", "JokeAdd", 0.0f },
        { "Approach", "CoerceAdd", 0.0f },
        { "Decay", "WhileOpen", 1.0f },
        { "Decay", "PerSecond", 0.1f },
        { "Decay", "PerRotation", 0.0f },
        { "Decay", "RateMult", 1.0f },
        { "Decay", "StartDelay", 5.0f },
        { "Decay", "Floor", 0.0f },
        { "Penalty", "Enabled", 1.0f },
        { "Penalty", "Start", 0.0f },
        { "Penalty", "Strength", 0.35f },
        { "Penalty", "Curve", 2.0f },
        { "Penalty", "SpeechRelief", 0.5f },
        { "Penalty", "Floor", 0.1f },
        { "Penalty", "OnBribe", 0.0f },
        { "Bribe", "Enabled", 1.0f },
        { "Bribe", "CostMult", 1.0f },
        { "Bribe", "GainMult", 1.0f },
        { "Bribe", "PriceScale", 5.0f },
        { "Bribe", "GainScale", 1.0f },
        { "Bribe", "DispositionPriceDivisor", 50.0f },
        { "Bribe", "PriceCurve", 1.5f },
        { "Bribe", "GainCurve", 0.75f },
        { "Bribe", "NpcLevelMult", 0.1f },
        { "Bribe", "Scale", 1.0f },
        { "Bribe", "SpeechcraftMult", 0.03f },
        { "Bribe", "MinPrice", 1.0f },
        { "Bribe", "MinGain", 1.0f },
    };
    static_assert(std::size(kSettingDefs) == static_cast<std::size_t>(P::kCount));
}
