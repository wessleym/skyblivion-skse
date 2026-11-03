#include "PerkListValidator.h"
#include "PerkIndex.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Stats {

	namespace {
		//SKYBPerkTrees, the master FormID list naming the eight SKYBPerks<Attribute> lists.
		constexpr RE::FormID  kPerkTreesList = 0x1254F2;
		constexpr const char* kPlugin = "Skyblivion.esm";

		using PerkSet = std::unordered_set<RE::FormID>;

		//One attribute's list. Matched to a tree by content overlap, not by position or editor id:
		//Skyrim discards editor ids at run time, and either side may be reordered.
		struct AttributeList {
			std::size_t index;
			PerkSet     perks;
		};

		std::vector<AttributeList> ReadLists() {
			std::vector<AttributeList> lists;

			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler) {
				return lists;
			}
			auto* master = dataHandler->LookupForm<RE::BGSListForm>(kPerkTreesList, kPlugin);
			if (!master) {
				Log::INFO("PerkListValidator: {} does not supply SKYBPerkTrees. Skipping the check.", kPlugin);
				return lists;
			}

			for (auto* entry : master->forms) {
				if (!entry || !entry->Is(RE::FormType::FormList)) {
					continue;
				}
				AttributeList list;
				list.index = lists.size();
				for (auto* member : static_cast<RE::BGSListForm*>(entry)->forms) {
					if (member && member->Is(RE::FormType::Perk)) {
						list.perks.insert(member->GetFormID());
					}
				}
				lists.push_back(std::move(list));
			}
			return lists;
		}

		//A registered tree's perks as run-time form ids: every rank in all, first ranks in first.
		//The attribute lists name one perk per node, so later ranks are absent by design.
		struct TreePerks {
			PerkSet all;
			PerkSet first;
		};

		TreePerks PerksOf(const PerkIndex::Tree& a_tree, std::size_t& a_unresolved) {
			TreePerks perks;
			for (const auto& node : a_tree.nodes) {
				bool isFirstRank = true;
				for (const auto& reference : node.perks) {
					if (auto* perk = PerkIndex::Resolve(reference)) {
						perks.all.insert(perk->GetFormID());
						if (isFirstRank) {
							perks.first.insert(perk->GetFormID());
						}
					}
					else {
						++a_unresolved;
						Log::WARN("PerkListValidator: Tree \"{}\" uses {}|{:06X}, which no loaded plugin supplies.",
							a_tree.id, reference.plugin, reference.formID);
					}
					isFirstRank = false;
				}
			}
			return perks;
		}

		std::size_t Overlap(const PerkSet& a_left, const PerkSet& a_right) {
			std::size_t shared = 0;
			for (const auto formID : a_left) {
				if (a_right.count(formID) > 0) {
					++shared;
				}
			}
			return shared;
		}
	}

	void PerkListValidator::Validate() {
		const auto lists = ReadLists();
		if (lists.empty()) {
			return;
		}

		const auto& trees = PerkIndex::Trees();
		std::size_t unresolved = 0;
		std::vector<TreePerks> treePerks;
		treePerks.reserve(trees.size());
		for (const auto& tree : trees) {
			treePerks.push_back(PerksOf(tree, unresolved));
		}

		//Every perk any list names, to tell "in no list" apart from "in another attribute's list".
		PerkSet listed;
		for (const auto& list : lists) {
			listed.insert(list.perks.begin(), list.perks.end());
		}

		std::size_t missingFromTree = 0;
		std::size_t missingFromList = 0;
		std::size_t unmatched = 0;

		for (std::size_t treeIndex = 0; treeIndex < trees.size(); ++treeIndex) {
			const auto& tree = trees[treeIndex];
			const auto& perks = treePerks[treeIndex];

			const AttributeList* match = nullptr;
			std::size_t best = 0;
			for (const auto& list : lists) {
				if (const auto shared = Overlap(perks.all, list.perks); shared > best) {
					best = shared;
					match = &list;
				}
			}
			if (!match) {
				++unmatched;
				Log::WARN("PerkListValidator: Tree \"{}\" shares no perk with any attribute list.", tree.id);
				continue;
			}

			for (const auto formID : match->perks) {
				if (perks.all.count(formID) == 0) {
					++missingFromTree;
					Log::WARN("PerkListValidator: {:08X} is in tree \"{}\"'s attribute list but no node uses it.",
						formID, tree.id);
				}
			}
			//First ranks only, for the reason given on TreePerks.
			for (const auto formID : perks.first) {
				if (match->perks.count(formID) == 0) {
					++missingFromList;
					Log::WARN("PerkListValidator: {:08X} is used by tree \"{}\" but {}.", formID, tree.id,
						listed.count(formID) > 0 ? "belongs to a different attribute's list" : "is in no attribute list");
				}
			}
		}

		Log::INFO("PerkListValidator: {} attribute list(s) against {} tree(s). "
			"In the list but unused: {}. Used but not listed: {}. Trees matching no list: {}. Unresolved perk ids: {}.",
			lists.size(), trees.size(), missingFromTree, missingFromList, unmatched, unresolved);
	}

}
