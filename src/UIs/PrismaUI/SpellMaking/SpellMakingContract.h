#pragma once

namespace SpellMaking::SpellMakingContract {
	// JS -> C++:
	namespace Listener {
		inline constexpr const char* SpellMakingBuy = "spellMakingBuy";
		inline constexpr const char* SpellMakingClose = "spellMakingClose";
	}

	// C++ -> JS:
	namespace JsFunc {
		inline constexpr const char* SpellMakingReset = "SpellMakingBridges.reset";
		inline constexpr const char* SpellMakingVerifyBridges = "SpellMakingBridges.verify";
	}

	namespace CastingType {
		//Anything that isn't "Concentration" is fire-and-forget.
		inline constexpr const char* Concentration = "Concentration";
	}
	namespace Delivery {
		//Anything that isn't "Self" or "Touch" is aimed.
		inline constexpr const char* Self = "Self";
		inline constexpr const char* Touch = "Touch";
	}
}
