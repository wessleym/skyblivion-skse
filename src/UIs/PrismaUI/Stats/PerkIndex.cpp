#include "PerkIndex.h"

#include <nlohmann/json.hpp>

#include <charconv>
#include <utility>

namespace Stats {

	namespace {
		std::vector<PerkIndex::Tree> g_trees;

		//Plugin-relative hex as the exports write it: "13E136"
		std::optional<RE::FormID> ParseFormId(std::string_view a_text) {
			if (a_text.empty()) {
				return std::nullopt;
			}
			RE::FormID  formID = 0;
			const auto* first = a_text.data();
			const auto* last = first + a_text.size();
			if (std::from_chars(first, last, formID, 16).ptr != last) {
				return std::nullopt;
			}
			return formID;
		}
	}

	std::string PerkReference::Describe() const {
		return plugin + " " + std::format("{:06X}", formID);
	}

	std::optional<PerkReference> PerkIndex::ReadReference(const nlohmann::json& a_json) {
		if (!a_json.is_object()) {
			return std::nullopt;
		}
		const auto plugin = a_json.value("plugin", std::string{});
		const auto formId = ParseFormId(a_json.value("formId", std::string{}));
		if (plugin.empty() || !formId) {
			return std::nullopt;
		}
		return PerkReference{ plugin, *formId };
	}

	bool PerkIndex::Set(const char* a_json) {
		if (!a_json) {
			Log::WARN("PerkIndex: Null payload.");
			return false;
		}
		const auto payload = nlohmann::json::parse(a_json, nullptr, false);
		if (payload.is_discarded() || !payload.contains("trees") || !payload["trees"].is_array()) {
			Log::WARN("PerkIndex: Unusable payload.");
			return false;
		}

		std::vector<Tree> trees;
		std::size_t perkCount = 0;
		std::size_t unreadable = 0;
		for (const auto& treeJson : payload["trees"]) {
			Tree tree;
			tree.id = treeJson.value("id", std::string{});
			if (treeJson.contains("nodes")) {
				for (const auto& nodeJson : treeJson["nodes"]) {
					Node node;
					node.id = nodeJson.value("id", std::string{});
					if (nodeJson.contains("perks")) {
						for (const auto& perkJson : nodeJson["perks"]) {
							if (auto perk = ReadReference(perkJson)) {
								node.perks.push_back(std::move(*perk));
								++perkCount;
							}
							else {
								++unreadable;
							}
						}
					}
					tree.nodes.push_back(std::move(node));
				}
			}
			trees.push_back(std::move(tree));
		}

		g_trees = std::move(trees);
		Log::INFO("PerkIndex: Registered {} tree(s) and {} perk(s).", g_trees.size(), perkCount);
		if (unreadable > 0) {
			Log::WARN("PerkIndex: {} rank(s) named no usable plugin and form id.", unreadable);
		}
		return true;
	}

	bool PerkIndex::IsEmpty() {
		return g_trees.empty();
	}

	const std::vector<PerkIndex::Tree>& PerkIndex::Trees() {
		return g_trees;
	}

	RE::BGSPerk* PerkIndex::Resolve(const PerkReference& a_perk) {
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		return dataHandler ? dataHandler->LookupForm<RE::BGSPerk>(a_perk.formID, a_perk.plugin) : nullptr;
	}
}
