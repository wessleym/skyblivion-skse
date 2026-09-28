#include "PerkDataStore.h"
#include "PerksContract.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using nlohmann::json;

namespace PerksView {

	namespace {
		constexpr const char* kTreeFileName = "tree.json";  // reserved filename per tree folder
		constexpr const char* kDefaultPerkPointsLabel = "$SKYB_perk_points_available";
		constexpr double kDefaultPriority = 100.0;

		const std::regex kIdRe{R"(^[A-Za-z][A-Za-z0-9_]*$)"};
		const std::regex kTransKeyRe{R"(^\$\S+$)"};
		const std::regex kThemeColorRe{R"(^#[0-9A-Fa-f]{6}$)"};
		const std::regex kFormRefRe{R"(^[0-9A-Fa-f]{1,8}\|\S.*$)"};

		std::string Lower(std::string a_text) {
			std::transform(a_text.begin(), a_text.end(), a_text.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_text;
		}

		bool IsId(const std::string& a_text) { return std::regex_match(a_text, kIdRe); }
		bool IsTransKey(const std::string& a_text) { return std::regex_match(a_text, kTransKeyRe); }

		// Optional-string fetch: false only when present-but-wrong-type.
		bool GetStringField(const json& a_json, const char* a_key, std::string& a_out, bool& a_present) {
			auto it = a_json.find(a_key);
			a_present = it != a_json.end();
			if (!a_present) {
				return true;
			}
			if (!it->is_string()) {
				return false;
			}
			a_out = it->get<std::string>();
			return true;
		}

		std::optional<std::string> RequiredString(const json& a_json, const char* a_key) {
			auto it = a_json.find(a_key);
			if (it == a_json.end() || !it->is_string()) {
				return std::nullopt;
			}
			return it->get<std::string>();
		}

		std::optional<double> RequiredNumber(const json& a_json, const char* a_key) {
			auto it = a_json.find(a_key);
			if (it == a_json.end() || !it->is_number()) {
				return std::nullopt;
			}
			return it->get<double>();
		}

		std::optional<FormRef> ParseFormRef(const std::string& a_raw) {
			if (!std::regex_match(a_raw, kFormRefRe)) {
				return std::nullopt;
			}
			FormRef ref;
			ref.raw = a_raw;
			const auto separator = a_raw.find('|');
			ref.plugin = a_raw.substr(separator + 1);
			ref.formID = static_cast<std::uint32_t>(std::stoul(a_raw.substr(0, separator), nullptr, 16));
			return ref;
		}

		// Shared optional-field handler for visibilityGlobal (trees and perks):
		// non-string or empty -> field dropped + logged.
		void ParseVisibilityGlobal(const json& a_json, const std::string& a_file, std::string& a_out) {
			bool present = false;
			std::string value;
			if (!GetStringField(a_json, "visibilityGlobal", value, present)) {
				Log::WARN("PerkDataStore: {}: 'visibilityGlobal' must be a string - field dropped", a_file);
			} else if (present) {
				if (!value.empty()) {
					a_out = value;
				} else {
					Log::WARN("PerkDataStore: {}: 'visibilityGlobal' is empty - field dropped", a_file);
				}
			}
		}

		// ---- requirement --------------------------------------------------
		// Known type with malformed fields = a required-field failure -> the
		// caller skips the WHOLE perk (SCHEMAS: "fails any required-field rule
		// -> whole perk skipped"). Unknown type string = kept, flagged
		// kUnknown, rawType preserved (pushed as-is; SWF fails closed).
		// Returns nullopt on the perk-skipping failures.
		std::optional<Requirement> ParseRequirement(const json& a_json, const std::string& a_file) {
			if (!a_json.is_object()) {
				Log::WARN("PerkDataStore: {}: requirement is not an object", a_file);
				return std::nullopt;
			}
			const auto type = RequiredString(a_json, "type");
			if (!type) {
				Log::WARN("PerkDataStore: {}: requirement missing string 'type'", a_file);
				return std::nullopt;
			}

			Requirement requirement;
			requirement.rawType = *type;

			if (*type == "level" || *type == "pointsInTree") {
				requirement.type = (*type == "level") ? RequirementType::kLevel : RequirementType::kPointsInTree;
				const auto value = RequiredNumber(a_json, "value");
				if (!value || *value < 1.0 || *value != std::floor(*value)) {
					Log::WARN("PerkDataStore: {}: '{}' requirement needs integer 'value' >= 1", a_file, *type);
					return std::nullopt;
				}
				requirement.value = *value;
			} else if (*type == "actorValue") {
				requirement.type = RequirementType::kActorValue;
				const auto name = RequiredString(a_json, "name");
				const auto value = RequiredNumber(a_json, "value");
				if (!name || name->empty() || !value) {
					Log::WARN("PerkDataStore: {}: 'actorValue' requirement needs 'name' (string) and 'value' (number)", a_file);
					return std::nullopt;
				}
				requirement.name = *name;
				requirement.value = *value;
			} else if (*type == "perk") {
				requirement.type = RequirementType::kPerk;
				const auto perkId = RequiredString(a_json, "perkId");
				if (!perkId || !IsId(*perkId)) {
					Log::WARN("PerkDataStore: {}: 'perk' requirement needs a valid 'perkId'", a_file);
					return std::nullopt;
				}
				requirement.perkId = *perkId;
				requirement.rank = 1;
				if (auto it = a_json.find("rank"); it != a_json.end()) {
					if (!it->is_number_integer() || it->get<int>() < 1) {
						Log::WARN("PerkDataStore: {}: 'perk' requirement 'rank' must be an integer >= 1", a_file);
						return std::nullopt;
					}
					requirement.rank = it->get<int>();
				}
			} else {
				// Fail-closed: kept but unmet-undisplayable; the type string is
				// passed to the SWF as-is.
				requirement.type = RequirementType::kUnknown;
				Log::WARN("PerkDataStore: {}: unknown requirement type '{}' - kept, treated unmet (fail-closed)",
							 a_file, *type);
			}
			return requirement;
		}

		// ---- perk file ----------------------------------------------------
		std::optional<Perk> ParsePerkFile(const fs::path& a_path) {
			const std::string file = a_path.generic_string();

			std::ifstream in(a_path);
			if (!in) {
				Log::WARN("PerkDataStore: {}: cannot open - perk skipped", file);
				return std::nullopt;
			}
			json document;
			try {
				document = json::parse(in, nullptr, true, true);  // comments allowed
			} catch (const std::exception& e) {
				Log::WARN("PerkDataStore: {}: invalid JSON ({}) - perk skipped", file, e.what());
				return std::nullopt;
			}
			if (!document.is_object()) {
				Log::WARN("PerkDataStore: {}: root is not an object - perk skipped", file);
				return std::nullopt;
			}

			Perk perk;

			const auto id = RequiredString(document, "id");
			if (!id || !IsId(*id)) {
				Log::WARN("PerkDataStore: {}: missing/invalid 'id' - perk skipped", file);
				return std::nullopt;
			}
			perk.id = *id;

			const auto name = RequiredString(document, "name");
			if (!name || !IsTransKey(*name)) {
				Log::WARN("PerkDataStore: {}: 'name' must be a translation key ($...) - perk skipped", file);
				return std::nullopt;
			}
			perk.name = *name;

			const auto icon = RequiredString(document, "icon");
			if (!icon || icon->empty()) {
				Log::WARN("PerkDataStore: {}: missing 'icon' - perk skipped", file);
				return std::nullopt;
			}
			perk.icon = *icon;

			const auto x = RequiredNumber(document, "x");
			const auto y = RequiredNumber(document, "y");
			if (!x || !y) {
				Log::WARN("PerkDataStore: {}: 'x'/'y' must be numbers - perk skipped", file);
				return std::nullopt;
			}
			perk.x = *x;
			perk.y = *y;

			if (auto it = document.find("prerequisites"); it != document.end()) {
				if (!it->is_array()) {
					Log::WARN("PerkDataStore: {}: 'prerequisites' must be an array - perk skipped", file);
					return std::nullopt;
				}
				for (const auto& entry : *it) {
					if (!entry.is_string() || !IsId(entry.get<std::string>())) {
						Log::WARN("PerkDataStore: {}: 'prerequisites' entries must be perk ids - perk skipped", file);
						return std::nullopt;
					}
					perk.prerequisites.push_back(entry.get<std::string>());
				}
			}

			bool present = false;
			std::string flavour;
			if (!GetStringField(document, "flavourText", flavour, present)) {
				Log::WARN("PerkDataStore: {}: 'flavourText' must be a string - field dropped", file);
			} else if (present) {
				if (IsTransKey(flavour)) {
					perk.flavourText = flavour;
				} else {
					Log::WARN("PerkDataStore: {}: 'flavourText' is not a translation key - field dropped", file);
				}
			}

			ParseVisibilityGlobal(document, file, perk.visibilityGlobal);

			const auto ranksIt = document.find("ranks");
			if (ranksIt == document.end() || !ranksIt->is_array() || ranksIt->empty()) {
				Log::WARN("PerkDataStore: {}: 'ranks' must be a non-empty array - perk skipped", file);
				return std::nullopt;
			}
			std::unordered_set<std::string> localRankIds;
			for (const auto& rankJson : *ranksIt) {
				if (!rankJson.is_object()) {
					Log::WARN("PerkDataStore: {}: rank entry is not an object - perk skipped", file);
					return std::nullopt;
				}
				Rank rank;
				const auto rankId = RequiredString(rankJson, "rankId");
				if (!rankId) {
					Log::WARN("PerkDataStore: {}: rank missing 'rankId' - perk skipped", file);
					return std::nullopt;
				}
				auto ref = ParseFormRef(*rankId);
				if (!ref) {
					Log::WARN("PerkDataStore: {}: rankId '{}' is not HEX|Plugin - perk skipped", file, *rankId);
					return std::nullopt;
				}
				if (!localRankIds.insert(Lower(ref->raw)).second) {
					Log::WARN("PerkDataStore: {}: duplicate rankId '{}' within perk - perk skipped", file, ref->raw);
					return std::nullopt;
				}
				rank.rankId = std::move(*ref);

				const auto description = RequiredString(rankJson, "description");
				if (!description || !IsTransKey(*description)) {
					Log::WARN("PerkDataStore: {}: rank 'description' must be a translation key - perk skipped", file);
					return std::nullopt;
				}
				rank.description = *description;

				if (auto requirementsIt = rankJson.find("requirements"); requirementsIt != rankJson.end()) {
					if (!requirementsIt->is_array()) {
						Log::WARN("PerkDataStore: {}: 'requirements' must be an array - perk skipped", file);
						return std::nullopt;
					}
					for (const auto& requirementJson : *requirementsIt) {
						auto requirement = ParseRequirement(requirementJson, file);
						if (!requirement) {
							return std::nullopt;  // reason already logged
						}
						rank.requirements.push_back(std::move(*requirement));
					}
				}
				perk.ranks.push_back(std::move(rank));
			}

			return perk;
		}

		// ---- tree.json ----------------------------------------------------
		// One tree definition per folder (decentralised discovery — no master
		// list). Validated independently: a bad tree.json rejects ONLY this
		// tree (fail-soft per tree).
		std::optional<Tree> ParseTreeJson(const fs::path& a_path, const std::string& a_folderName) {
			const std::string file = a_path.generic_string();

			std::ifstream in(a_path);
			if (!in) {
				Log::WARN("PerkDataStore: {}: cannot open - tree skipped", file);
				return std::nullopt;
			}
			json document;
			try {
				document = json::parse(in, nullptr, true, true);  // comments allowed
			} catch (const std::exception& e) {
				Log::WARN("PerkDataStore: {}: invalid JSON ({}) - tree skipped", file, e.what());
				return std::nullopt;
			}
			if (!document.is_object()) {
				Log::WARN("PerkDataStore: {}: root is not an object - tree skipped", file);
				return std::nullopt;
			}

			const auto version = document.find("schemaVersion");
			if (version == document.end() || !version->is_number_integer() || version->get<int>() != 1) {
				Log::WARN("PerkDataStore: {}: schemaVersion must be the integer 1 - tree skipped", file);
				return std::nullopt;
			}

			Tree tree;

			const auto id = RequiredString(document, "id");
			if (!id || !IsId(*id)) {
				Log::WARN("PerkDataStore: {}: missing/invalid 'id' - tree skipped", file);
				return std::nullopt;
			}
			tree.id = *id;
			if (Lower(tree.id) != Lower(a_folderName)) {
				Log::WARN("PerkDataStore: {}: id '{}' does not match folder name '{}' - tree skipped",
							 file, tree.id, a_folderName);
				return std::nullopt;
			}

			// priority: optional number, default 100 (invalid -> default + log).
			tree.priority = kDefaultPriority;
			if (auto it = document.find("priority"); it != document.end()) {
				if (it->is_number()) {
					tree.priority = it->get<double>();
				} else {
					Log::WARN("PerkDataStore: {}: 'priority' must be a number - default {} used", file, kDefaultPriority);
				}
			}

			const auto displayName = RequiredString(document, "displayName");
			if (!displayName || !IsTransKey(*displayName)) {
				Log::WARN("PerkDataStore: {}: 'displayName' must be a translation key - tree skipped", file);
				return std::nullopt;
			}
			tree.displayName = *displayName;

			const auto icon = RequiredString(document, "icon");
			const auto backgroundArt = RequiredString(document, "backgroundArt");
			if (!icon || icon->empty() || !backgroundArt || backgroundArt->empty()) {
				Log::WARN("PerkDataStore: {}: 'icon' and 'backgroundArt' are required - tree skipped", file);
				return std::nullopt;
			}
			tree.icon = *icon;
			tree.backgroundArt = *backgroundArt;

			bool present = false;
			std::string themeColor;
			if (!GetStringField(document, "themeColor", themeColor, present)) {
				Log::WARN("PerkDataStore: {}: 'themeColor' must be a string - field dropped", file);
			} else if (present) {
				if (std::regex_match(themeColor, kThemeColorRe)) {
					tree.themeColor = themeColor;
				} else {
					Log::WARN("PerkDataStore: {}: themeColor '{}' is not #RRGGBB - field dropped (default gold)",
								 file, themeColor);
				}
			}

			std::string attribute;
			if (!GetStringField(document, "attribute", attribute, present)) {
				Log::WARN("PerkDataStore: {}: 'attribute' must be a string - field dropped", file);
			} else if (present) {
				tree.attribute = attribute;  // AV names go verbatim to GetActorValue
			}

			ParseVisibilityGlobal(document, file, tree.visibilityGlobal);

			// Currency: omitted == { "type": "perkPoints" }.
			tree.currencyType = CurrencyType::kPerkPoints;
			tree.currencyLabel = kDefaultPerkPointsLabel;
			if (auto it = document.find("currency"); it != document.end()) {
				if (!it->is_object()) {
					Log::WARN("PerkDataStore: {}: 'currency' must be an object - tree skipped", file);
					return std::nullopt;
				}
				const auto type = RequiredString(*it, "type");
				if (!type || (*type != "perkPoints" && *type != "globalVariable")) {
					Log::WARN("PerkDataStore: {}: currency 'type' must be perkPoints|globalVariable - tree skipped", file);
					return std::nullopt;
				}
				std::string editorId;
				bool hasEditorId = false;
				if (!GetStringField(*it, "editorId", editorId, hasEditorId)) {
					Log::WARN("PerkDataStore: {}: currency 'editorId' must be a string - tree skipped", file);
					return std::nullopt;
				}
				std::string label;
				bool hasLabel = false;
				if (!GetStringField(*it, "label", label, hasLabel)) {
					Log::WARN("PerkDataStore: {}: currency 'label' must be a string - tree skipped", file);
					return std::nullopt;
				}
				if (hasLabel && !IsTransKey(label)) {
					Log::WARN("PerkDataStore: {}: currency 'label' must be a translation key - tree skipped", file);
					return std::nullopt;
				}

				if (*type == "globalVariable") {
					tree.currencyType = CurrencyType::kGlobalVariable;
					if (!hasEditorId || editorId.empty()) {
						Log::WARN("PerkDataStore: {}: globalVariable currency requires 'editorId' - tree skipped", file);
						return std::nullopt;
					}
					if (!hasLabel) {
						Log::WARN("PerkDataStore: {}: globalVariable currency requires 'label' - tree skipped", file);
						return std::nullopt;
					}
					tree.globalEditorId = editorId;
					tree.currencyLabel = label;
				} else {  // perkPoints
					if (hasEditorId) {
						Log::WARN("PerkDataStore: {}: 'editorId' is forbidden for perkPoints currency - tree skipped", file);
						return std::nullopt;
					}
					if (hasLabel) {
						tree.currencyLabel = label;
					}
				}
			}

			return tree;
		}
	}

	const std::vector<Tree>& PerkDataStore::Trees() {
		return s_trees;
	}

	const Tree* PerkDataStore::FindTree(std::string_view id) {
		for (const auto& tree : s_trees) {
			if (tree.id == id) {
				return &tree;
			}
		}
		return nullptr;
	}

	void PerkDataStore::LoadOnce() {
		if (s_loaded) {
			return;
		}
		s_loaded = true;  // one parse per session, success or not

		const fs::path root{ PerksContract::DataRoot };
		std::error_code ec;
		if (!fs::is_directory(root, ec)) {
			Log::ERROR("PerkDataStore: {} not found - no perk trees", root.generic_string());
			return;
		}

		// Legacy master list: ignored since the decentralised-discovery round.
		if (fs::exists(root / "trees.json", ec)) {
			Log::WARN("PerkDataStore: {}/trees.json is DEPRECATED and ignored - trees are discovered per-folder "
						 "(<TreeFolder>/tree.json)", root.generic_string());
		}

		// Discovery: every subfolder with a valid tree.json registers a tree.
		struct Candidate {
			Tree tree;
			fs::path folder;
			std::string folderNameLower;
		};
		std::vector<Candidate> candidates;
		for (const auto& entry : fs::directory_iterator(root, ec)) {
			if (!entry.is_directory(ec)) {
				continue;
			}
			const std::string folderName = entry.path().filename().string();
			// Windows filenames are case-insensitive; construct the reserved
			// name directly (any casing on disk resolves to it).
			const fs::path treeFile = entry.path() / kTreeFileName;
			if (!fs::is_regular_file(treeFile, ec)) {
				Log::INFO("PerkDataStore: folder {} has no {} - skipped", entry.path().generic_string(), kTreeFileName);
				continue;
			}
			auto tree = ParseTreeJson(treeFile, folderName);
			if (!tree) {
				continue;  // reason already logged (fail-soft per tree)
			}
			candidates.push_back(Candidate{std::move(*tree), entry.path(), Lower(folderName)});
		}
		if (candidates.empty()) {
			Log::ERROR("PerkDataStore: no valid tree folders under {}", root.generic_string());
			return;
		}

		// Duplicate ids across folders: the higher-priority tree wins (lower
		// number sorts/lists first == higher priority), ties broken by
		// alphabetical folder name. Losers rejected + logged.
		auto beats = [](const Candidate& a, const Candidate& b) {
			if (a.tree.priority != b.tree.priority) {
				return a.tree.priority < b.tree.priority;
			}
			return a.folderNameLower < b.folderNameLower;
		};
		std::unordered_map<std::string, std::size_t> winnerById;  // lowercased id -> candidates index
		for (std::size_t i = 0; i < candidates.size(); ++i) {
			const auto key = Lower(candidates[i].tree.id);
			auto [it, inserted] = winnerById.try_emplace(key, i);
			if (inserted) {
				continue;
			}
			auto& winner = it->second;
			const auto loser = beats(candidates[i], candidates[winner]) ? std::exchange(winner, i) : i;
			Log::WARN("PerkDataStore: duplicate tree id '{}': folder {} rejected (priority {} vs winning {} in {})",
						 candidates[loser].tree.id, candidates[loser].folder.generic_string(),
						 candidates[loser].tree.priority, candidates[winnerById[key]].tree.priority,
						 candidates[winnerById[key]].folder.generic_string());
		}
		std::vector<Candidate*> accepted;
		for (std::size_t i = 0; i < candidates.size(); ++i) {
			if (winnerById[Lower(candidates[i].tree.id)] == i) {
				accepted.push_back(&candidates[i]);
			}
		}

		// Selector/cycle order: priority ascending, ties alphabetical by id.
		std::sort(accepted.begin(), accepted.end(), [](const Candidate* a, const Candidate* b) {
			if (a->tree.priority != b->tree.priority) {
				return a->tree.priority < b->tree.priority;
			}
			const auto lowerA = Lower(a->tree.id);
			const auto lowerB = Lower(b->tree.id);
			return lowerA != lowerB ? lowerA < lowerB : a->tree.id < b->tree.id;
		});

		// Perk files: everything *.json in the tree's folder EXCEPT tree.json.
		// Globally-unique perk ids and rankIds; alphabetical scan order makes
		// "first wins" deterministic (trees in final priority order, files
		// sorted case-insensitively within each folder).
		std::unordered_set<std::string> perkIds;        // exact-case (bare ids are referenced verbatim)
		std::unordered_set<std::string> globalRankIds;  // lowercased raw form refs
		std::size_t perkCount = 0;
		std::size_t rankCount = 0;

		for (auto* candidate : accepted) {
			Tree tree = std::move(candidate->tree);

			std::vector<fs::path> files;
			for (const auto& entry : fs::directory_iterator(candidate->folder, ec)) {
				if (!entry.is_regular_file(ec)) {
					continue;
				}
				const auto nameLower = Lower(entry.path().filename().string());
				if (nameLower == kTreeFileName) {
					continue;  // the tree definition itself
				}
				if (Lower(entry.path().extension().string()) == ".json") {
					files.push_back(entry.path());
				}
			}
			std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
				const auto lowerA = Lower(a.filename().string());
				const auto lowerB = Lower(b.filename().string());
				return lowerA != lowerB ? lowerA < lowerB : a.filename().string() < b.filename().string();
			});

			for (const auto& file : files) {
				auto perk = ParsePerkFile(file);
				if (!perk) {
					continue;  // reason already logged
				}
				if (perkIds.contains(perk->id)) {
					Log::WARN("PerkDataStore: {}: duplicate perk id '{}' - first file (alphabetical) wins, this one skipped",
								 file.generic_string(), perk->id);
					continue;
				}
				bool rankClash = false;
				for (const auto& rank : perk->ranks) {
					if (globalRankIds.contains(Lower(rank.rankId.raw))) {
						Log::WARN("PerkDataStore: {}: rankId '{}' already used by another perk - perk skipped",
									 file.generic_string(), rank.rankId.raw);
						rankClash = true;
						break;
					}
				}
				if (rankClash) {
					continue;
				}
				perkIds.insert(perk->id);
				for (const auto& rank : perk->ranks) {
					globalRankIds.insert(Lower(rank.rankId.raw));
				}
				rankCount += perk->ranks.size();
				++perkCount;
				tree.perks.push_back(std::move(*perk));
			}

			if (tree.perks.empty()) {
				Log::WARN("PerkDataStore: tree '{}': no valid perks - tree listed with empty canvas", tree.id);
			}
			s_trees.push_back(std::move(tree));
		}

		// Post-pass 1: prerequisites must name perks in the SAME tree.
		// Dangling entries drop (logged); if all drop, the perk becomes a root.
		for (auto& tree : s_trees) {
			std::unordered_set<std::string> perkIdsInTree;
			for (const auto& perk : tree.perks) {
				perkIdsInTree.insert(perk.id);
			}
			for (auto& perk : tree.perks) {
				if (perk.prerequisites.empty()) {
					continue;
				}
				const auto before = perk.prerequisites.size();
				std::erase_if(perk.prerequisites, [&](const std::string& prerequisite) {
					if (!perkIdsInTree.contains(prerequisite)) {
						Log::WARN("PerkDataStore: perk '{}': prerequisite '{}' not found in tree '{}' - entry dropped",
									 perk.id, prerequisite, tree.id);
						return true;
					}
					return false;
				});
				if (before > 0 && perk.prerequisites.empty()) {
					Log::WARN("PerkDataStore: perk '{}': all prerequisites dropped - perk becomes a root", perk.id);
				}
			}
		}

		// Post-pass 2: "perk" requirements may reference any tree, but an
		// unknown id can never be met (fail-closed via the state feed: no
		// perkRanks entry -> 0 owned). Logged here so the bad reference is visible.
		for (const auto& tree : s_trees) {
			for (const auto& perk : tree.perks) {
				for (const auto& rank : perk.ranks) {
					for (const auto& requirement : rank.requirements) {
						if (requirement.type == RequirementType::kPerk && !perkIds.contains(requirement.perkId)) {
							Log::WARN(
								"PerkDataStore: perk '{}': requirement references unknown perk '{}' - permanently unmet (fail-closed)",
								perk.id, requirement.perkId);
						}
					}
				}
			}
		}

		Log::INFO("PerkDataStore: loaded {} tree(s), {} perk(s), {} rank(s) from {} (priority order)",
					 s_trees.size(), perkCount, rankCount, root.generic_string());
	}

}
