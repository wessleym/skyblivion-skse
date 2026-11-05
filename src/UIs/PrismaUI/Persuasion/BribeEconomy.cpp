#include "BribeEconomy.h"

#include <algorithm>
#include <cmath>

namespace Persuasion {

	BribeEconomy::Outcome BribeEconomy::Calculate(const Params& p, float disposition, float playerSpeech, float npcSpeech, int playerLevel, int npcLevel) {
		const float combinedSpeech = std::max(0.0f, playerSpeech + npcSpeech);
		const float levelFactor = static_cast<float>(playerLevel) + static_cast<float>(npcLevel) * p.npcLevelMult;
		const float speechCurveCost = std::pow(combinedSpeech, p.priceCurve) * p.speechcraftMult;
		const float divisor = p.dispositionPriceDivisor > 0.0f ? p.dispositionPriceDivisor : 50.0f;
		const float dispositionMult = 1.0f + disposition / divisor;
		const float costF = p.priceScale * p.scale * dispositionMult * (levelFactor + speechCurveCost) * std::max(0.0f, p.costMult);
		const int price = std::max(p.minPrice, static_cast<int>(costF + 0.5f));

		const float maxDisposition = p.maxDisposition > 0.0f ? p.maxDisposition : 100.0f;
		const float dispositionFalloff = std::max(0.05f, 1.0f - disposition / maxDisposition);
		const float gainF = std::pow(combinedSpeech, p.gainCurve) * p.gainScale * dispositionFalloff * std::max(0.0f, p.gainMult);
		const int   gain = std::max(p.minGain, static_cast<int>(gainF + 0.5f));

		return { price, gain, gainF };
	}

}
