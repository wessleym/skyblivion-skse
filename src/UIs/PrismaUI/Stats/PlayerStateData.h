#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace Stats {

	//The player's standing against the registered graph, keyed by the view's ids, not by form.
	//Mirrored by PlayerStateJson in Data/PrismaUI/views/Stats/script.ts.

	struct PlayerPerkData {
		std::string id;
		//Ranks owned, 0 to the node's rank count.
		int ownedRanks;
	};

	struct PlayerAttributeData {
		std::string                 id;
		//Name from the Actor Value Information record, empty when unavailable.
		//The view prefers it over the name in its own document.
		std::string                 name;
		//Null when no actor value backs the tree.
		std::optional<int>          attributeLevel;
		std::vector<PlayerPerkData> perks;
	};

	struct PlayerStateData {
		int                              perkPoints = 0;
		std::vector<PlayerAttributeData> attributes;
	};

	inline void to_json(nlohmann::json& a_json, const PlayerPerkData& a_perk) {
		a_json = nlohmann::json{
			{ "id", a_perk.id },
			{ "ownedRanks", a_perk.ownedRanks }
		};
	}

	inline void to_json(nlohmann::json& a_json, const PlayerAttributeData& a_attribute) {
		a_json = nlohmann::json{
			{ "id", a_attribute.id },
			{ "name", a_attribute.name },
			{ "attributeLevel", a_attribute.attributeLevel ? nlohmann::json(*a_attribute.attributeLevel) : nlohmann::json(nullptr) },
			{ "perks", a_attribute.perks }
		};
	}

	inline void to_json(nlohmann::json& a_json, const PlayerStateData& a_state) {
		a_json = nlohmann::json{
			{ "perkPoints", a_state.perkPoints },
			{ "attributes", a_state.attributes }
		};
	}

}
