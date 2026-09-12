#pragma once

namespace Stats::StatsContract {
	// JS -> C++:
	namespace Listener {
		inline constexpr const char* StatsClose = "statsClose";
		//Payload: {"trees":[{"id","nodes":[{"id","perks":[{"plugin","formId"}]}]}]}.
		//Sent once the view has loaded its tree document.
		inline constexpr const char* StatsSetPerkIds = "statsSetPerkIds";
		//Payload: {"plugin","formId"}. Spends one perk point on that perk.
		inline constexpr const char* StatsAcquire = "statsAcquire";
		//Payload: PerkTreeDocumentJson, written out as perk-trees.js.
		inline constexpr const char* StatsSaveTrees = "statsSaveTrees";
	}

	// C++ -> JS:
	namespace JsFunc {
		inline constexpr const char* StatsVerifyBridges = "StatsBridges.verify";
		//Requests the perk ids.
		//The page cannot send them unprompted: its scripts run before these listeners exist.
		inline constexpr const char* StatsSendPerkIds = "StatsBridges.sendPerkIds";
		//Payload: PlayerStateJson. Sent on registration and on every open.
		inline constexpr const char* StatsSetPlayerState = "StatsBridges.setPlayerState";
		//true on open, false on close.
		//The page keeps running while hidden,
		//and navigator.getGamepads() reports the stick whichever view holds focus.
		inline constexpr const char* StatsSetVisible = "StatsBridges.setVisible";
	}
}
