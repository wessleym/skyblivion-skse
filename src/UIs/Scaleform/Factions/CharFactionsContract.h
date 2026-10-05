#pragma once

//The character sheet's faction feed: the Character menu of the new menus.
//Replaces FactionsContract's event when the old character menu retires.
namespace Factions::CharFactionsContract {
	//Character sheet -> C++:
	namespace ModEvent {
		//strArg: "menu|target", target the absolute path of one of the sheet's functions.
		//Invokes target with one Array of {name, rank, expelled}: the factions the player belongs to,
		//sorted by name. rank is the player's rank title, "" when expelled or untitled.
		//Always answers, with an empty Array when there are none or the faction list is missing.
		inline constexpr const char* RequestFactions = "SKYBCharFactionsRequest";
	}
}
