#pragma once

namespace Sigil {

	//The soul gems that pay a sigil stone's tier cost.
	//Counting and consuming share one rule, so the menu never shows a gem the cost will not take:
	//	The gem's capacity equals the tier.
	//	Its soul equals the tier: either the record carries it (vanilla *Filled and prefilled bases),
	//	or the instance does (a player-filled gem: an empty base plus ExtraSoul).
	//	Excluded: reusable gems (ReusableSoulGem keyword, e.g. Azura's Star, whose consumption would destroy an artifact)
	//	and black-soul-capable gems.
	//Tiers run from 1 (petty) to 5 (grand); any other tier counts and consumes nothing.
	class SoulGems {
	public:
		[[nodiscard]] static int CountTierGems(RE::PlayerCharacter* player, int tier);

		//Removes one matching gem: a record-filled base first, then a player-filled instance.
		//The instance is targeted by its extra data list; Papyrus would take an arbitrary member of the base's stack,
		//possibly an empty one. True when a gem was removed.
		static bool ConsumeTierGem(RE::PlayerCharacter* player, int tier);
	};

}
