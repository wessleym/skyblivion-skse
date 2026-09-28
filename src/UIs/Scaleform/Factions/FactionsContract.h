#pragma once

//The character menu's faction page feed, rebuilt to the retired SkyblivionUI.dll's Scaleform contract.
namespace Factions::FactionsContract {
	//Character menu -> C++:
	namespace ModEvent {
		//strArg: "menu,target". Invokes target with {isInFaction, isExpelled, factionName, rankName}
		//for every faction in the list, sorted by display name. The menu renders member rows only.
		inline constexpr const char* RequestFactions = "SKYBGetFactionInfo";
	}

	//Resolved by editor ID, which needs po3 Tweaks.
	inline constexpr const char* FactionListEditorID = "SKYBUIFactionslist_FactionUIFL";
}
