#pragma once
// The persuasion wheel's rules for one conversation.
//
// Resolve() takes the settings (INI + settings menu), lays the perk and skill modifiers over
// them for the player as they are now, and reads the player's Speechcraft. The result is what
// the view sends to JavaScript as "rules" (the wheel computes wedge results from it) and what
// C++ uses for bribes and the disposition penalty.
#include "BribeEconomy.h"
#include "PersuasionSettings.h"

#include <nlohmann/json.hpp>

namespace RE { class Actor; }

namespace Persuasion
{
	struct Effective
	{
		Settings::Values v{};
		float            playerSpeech{ 0.0f };

		[[nodiscard]] float Get(P a_id) const { return v[static_cast<std::size_t>(a_id)]; }
		[[nodiscard]] bool  On(P a_id) const { return Get(a_id) >= 0.5f; }
	};

	class Rules
	{
	public:
		// Reloads the INIs if they changed, then settings -> perks -> this player's values.
		[[nodiscard]] static Effective Resolve();

		// What a gain at a_disposition is multiplied by: 1 below Penalty.Start, down to Penalty.Floor
		// at MaxDisposition, relieved by the player's Speechcraft. 1 when the penalty is off.
		[[nodiscard]] static float PenaltyMultiplier(const Effective& a_eff, float a_disposition);

		// The "rules" object of the view's init payload (see script.ts, PersuasionRules).
		[[nodiscard]] static nlohmann::json ToJson(const Effective& a_eff);

		[[nodiscard]] static BribeEconomy::Params BribeParams(const Effective& a_eff);
	};
}
