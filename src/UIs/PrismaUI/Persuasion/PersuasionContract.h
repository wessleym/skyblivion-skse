#pragma once

namespace Persuasion::PersuasionContract {
	//JS -> C++
	namespace Listener {
		inline constexpr const char* PersuasionWedgeHover = "persuasionWedgeHover";
		inline constexpr const char* PersuasionClose = "persuasionClose";
		inline constexpr const char* PersuasionDispositionChanged = "persuasionDispositionChanged";
		inline constexpr const char* PersuasionBribe = "persuasionBribe";
	}

	//C++ -> JS
	namespace JsFunc {
		inline constexpr const char* PersuasionInit = "PersuasionBridges.init";
		inline constexpr const char* PersuasionBribeResult = "PersuasionBridges.bribeResult";
		inline constexpr const char* PersuasionVerifyBridges = "PersuasionBridges.verify";
	}
}
