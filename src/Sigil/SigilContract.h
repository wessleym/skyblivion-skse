#pragma once

//SKYBSigilEnchant.psc's requests for the inventory changes Papyrus cannot express.
//Papyrus keeps the game logic: it picks the enchantment and charge, and consumes the stone on success.
//FormIDs travel as signed 32-bit decimal (see ModEvents::ParseFormID).
namespace Sigil::SigilContract {
	//Papyrus -> C++:
	namespace ModEvent {
		//strArg: "gemsFormListID^tier". Removes one filled soul gem of the tier (1 petty to 5 grand).
		//Answered with GemConsumed.
		inline constexpr const char* ConsumeGem = "SKYBSigilGemConsume";
		//strArg: "gearID^enchantmentID^charge^customName". The name may be empty.
		//Enchants one carried copy of the gear. Answered with Applied.
		inline constexpr const char* ApplyEnchant = "SKYBSigilApplyEnchant";
	}

	//C++ -> Papyrus:
	namespace Answer {
		//strArg: "1" when a gem was removed, "0" when none matched.
		inline constexpr const char* GemConsumed = "SKYBSigilGemConsumed";
		//strArg: one of the ApplyResult values below.
		inline constexpr const char* Applied = "SKYBSigilApplied";
	}

	namespace ApplyResult {
		inline constexpr const char* Enchanted = "1";
		//Every carried copy is already enchanted.
		inline constexpr const char* AlreadyEnchanted = "already";
		//The gear is not carried, a form did not resolve, or the payload was malformed.
		inline constexpr const char* Missing = "missing";
	}
}
