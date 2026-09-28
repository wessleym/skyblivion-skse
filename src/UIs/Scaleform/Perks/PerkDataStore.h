#pragma once

#include "PerkData.h"

#include <string_view>
#include <vector>

namespace PerksView {

	//Loads and caches the perk tree definitions from PerksContract::DataRoot.
	//
	//Every subfolder holding a valid tree.json registers a tree; its other *.json files are that tree's perks,
	//read in alphabetical order so duplicate handling is deterministic. There is no master list:
	//a legacy top-level trees.json is ignored with a deprecation warning.
	//Trees sort by priority (ascending), then id. A duplicate tree id keeps the higher-priority tree,
	//then the alphabetically first folder.
	//
	//Loading is fail-soft per tree and per perk: a bad file logs one line with its path and reason and is skipped.
	//Unknown requirement types are kept but never met. Perk ids and rank ids must be unique across all trees
	//(the first wins). Prerequisites naming a perk outside the tree are dropped; a perk left with none becomes a root.
	//
	//All access is on the main thread (kDataLoaded, then SKSE tasks), so the store needs no lock.
	class PerkDataStore {
	public:
		//Parses and caches the definitions. Later calls do nothing, whether the first succeeded or not.
		static void LoadOnce();

		[[nodiscard]] static const std::vector<Tree>& Trees();

		//The tree with this id, or null.
		[[nodiscard]] static const Tree* FindTree(std::string_view id);

	private:
		static inline bool s_loaded = false;
		static inline std::vector<Tree> s_trees;
	};

}
