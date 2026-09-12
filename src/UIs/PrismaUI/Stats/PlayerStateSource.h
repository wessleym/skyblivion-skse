#pragma once

#include "PlayerStateData.h"

#include <optional>
#include <string_view>

namespace Stats {

	//Reads the player's standing against the graph registered with PerkIndex.
	class PlayerStateSource {
	public:
		//Empty state, not a partial one, when the player is unavailable.
		static PlayerStateData Capture();

		//Actor value holding an attribute's level.
		//Null when the tree is not one of the eight Oblivion attributes.
		static std::optional<RE::ActorValue> ActorValueForTree(std::string_view a_treeId);
	};

}
