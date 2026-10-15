#pragma once

#include <optional>
#include <string_view>
#include <vector>

//The player's standing in the factions the UI lists, read once for every menu that shows them:
//the character menu's faction page (FactionFeed) and the character sheet's (CharFactionsFeed).
namespace Factions::FactionData {
	struct Standing {
		RE::TESFaction* faction;
		//The faction's display name, "" when it has none.
		const char* name;
		bool member;
		//Only ever true for a member.
		bool expelled;
		//The player's rank title for their sex; "" when not a member, expelled, or the rank has no title.
		const char* rankTitle;
	};

	//Every faction in SKYBUIFactionslist_FactionUIFL, sorted by display name.
	//Empty, logged under source, when the list is missing.
	[[nodiscard]] std::optional<std::vector<Standing>> ReadAll(RE::PlayerCharacter* player, std::string_view source);
}
