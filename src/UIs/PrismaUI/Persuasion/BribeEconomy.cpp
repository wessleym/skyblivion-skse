#include "BribeEconomy.h"

#include <algorithm>
#include <cmath>

namespace Persuasion {

	namespace {
		//Oblivion GMST settings for bribery (from cs.uesp.net/wiki/Bribery):
		constexpr float kBribePriceCurve = 1.50f;//price curve exponent on combined speech
		constexpr float kBribeCurve = 0.75f;//gain curve exponent on combined speech
		constexpr float kBribeNpcLevelMult = 0.10f;//weight of NPC level vs. player level
		constexpr float kBribeScale = 1.00f;//global price multiplier
		constexpr float kBribeSpeechcraftMult = 0.03f;//weight of speechcraft vs. levels in price
		//These constants are just guesses since I couldn't find them in game data:
		constexpr float kBribePriceScale = 5.00f;//overall price multiplier on top of fBribeScale
		constexpr float kBribeGainScale = 1.00f;//overall gain multiplier on the speech-curve term
		constexpr float kBribeDispositionPriceDivisor = 50.0f;//dispositionMult = 1 + disposition / divisor (smaller = steeper)
	}

	BribeEconomy::Outcome BribeEconomy::Calculate(float disposition, float playerSpeech, float npcSpeech, int playerLevel, int npcLevel) {
		const float combinedSpeech = playerSpeech + npcSpeech;
		const float levelFactor = static_cast<float>(playerLevel) + static_cast<float>(npcLevel) * kBribeNpcLevelMult;
		const float speechCurveCost = std::pow(combinedSpeech, kBribePriceCurve) * kBribeSpeechcraftMult;
		const float dispositionMult = 1.0f + disposition / kBribeDispositionPriceDivisor;
		float costF = kBribePriceScale * kBribeScale * dispositionMult * (levelFactor + speechCurveCost);
		if (playerSpeech >= 100.0f) {//Master Speechcraft halves the result (Speechcraft 100 mastery perk).
			costF *= 0.5f;
		}
		const int price = std::max(1, static_cast<int>(costF + 0.5f));

		const float dispositionFalloff = std::max(0.05f, 1.0f - disposition / kDispositionMax);
		const float gainF = std::pow(combinedSpeech, kBribeCurve) * kBribeGainScale * dispositionFalloff;
		const int   gain = std::max(1, static_cast<int>(gainF + 0.5f));

		return { price, gain };
	}

}
