#pragma once
// One bribe, for either host of the persuasion wheel (the PrismaUI view and the Scaleform menu):
// price and gain from BribeEconomy under the conversation's rules, the gold taken, the
// disposition set.
#include "PersuasionRules.h"

namespace RE { class Actor; }

namespace Persuasion
{
	struct BribeAttempt
	{
		enum class Reason
		{
			kNone,
			kDisabled,   // [Bribe] Enabled = 0
			kNoGold,     // the player cannot pay
			kTooSmall,   // it would buy less than one point
			kError       // an engine lookup failed (logged)
		};

		bool   success{ false };
		Reason reason{ Reason::kNone };
		int    price{ 0 };
		int    gain{ 0 };
		float  disposition{ 0.0f };  // after the bribe (or unchanged)
	};

	class Bribery
	{
	public:
		// a_disposition: the value the view shows right now (decay included). Sets the actor's
		// disposition on success.
		static BribeAttempt Attempt(RE::Actor* a_actor, float a_disposition, const Effective& a_eff);

		// What a bribe at a_disposition would cost and buy, nothing taken (the price on the wheel's
		// Bribe button). reason kNoGold: the player cannot pay.
		static BribeAttempt Quote(RE::Actor* a_actor, float a_disposition, const Effective& a_eff);
	};
}
