#pragma once

#include <cstdint>
#include <string>
#include <vector>

//The perk tree definitions as loaded from the JSON files (schemaVersion 1).
//Icon and art references stay raw strings: the SWF resolves them.
//Form references keep both the parsed pieces, for TESDataHandler::LookupForm,
//and the raw "HEX|Plugin" string, which is pushed to the SWF verbatim as rankId.
namespace PerksView {

	enum class CurrencyType {
		//The engine's perk-point pool.
		kPerkPoints,
		//A named GlobalVariable.
		kGlobalVariable
	};

	enum class RequirementType {
		//Player level >= value.
		kLevel,
		//GetActorValue(name) >= value.
		kActorValue,
		//Own at least rank ranks of perkId, in any tree.
		kPerk,
		//Ranks owned across this tree >= value.
		kPointsInTree,
		//An unrecognised type string: never met. rawType is pushed as-is so the SWF fails closed too.
		kUnknown
	};

	struct FormRef {
		//Plugin-relative, without the load-order prefix.
		std::uint32_t formID{ 0 };
		//Plugin file name, such as "Skyblivion.esm".
		std::string plugin;
		//The original "HEX|Plugin" string.
		std::string raw;
	};

	struct Requirement {
		RequirementType type{ RequirementType::kUnknown };
		//The authored type string; the canonical name for known types.
		std::string rawType;
		//Level, actor value threshold or points; 0 when not applicable.
		double value{ 0.0 };
		//The actor value name for kActorValue; "" otherwise.
		std::string name;
		//For kPerk; "" otherwise.
		std::string perkId;
		//For kPerk (>= 1, default 1); 0 when not applicable.
		int rank{ 0 };
	};

	struct Rank {
		//The perk record this rank adds.
		FormRef rankId;
		//Translation key.
		std::string description;
		//All must be met; may be empty.
		std::vector<Requirement> requirements;
	};

	struct Perk {
		//Unique across all trees.
		std::string id;
		//Translation key.
		std::string name;
		std::string icon;
		//Node centre in canvas-local pixels.
		double x{ 0.0 };
		double y{ 0.0 };
		//Translation key or "".
		std::string flavourText;
		//GlobalVariable editor ID or "". The node shows only while the global is non-zero (presentation only).
		std::string visibilityGlobal;
		//Any one suffices; same tree only. Empty makes the perk a root.
		std::vector<std::string> prerequisites;
		//At least one, in rank order.
		std::vector<Rank> ranks;
	};

	struct Tree {
		//Matches its folder's name, case-insensitively.
		std::string id;
		//Sort key, ascending; ties sort alphabetically by id.
		double priority{ 100.0 };
		//Translation key.
		std::string displayName;
		std::string icon;
		std::string backgroundArt;
		//"#RRGGBB", or "" for the menu's default gold.
		std::string themeColor;
		//Actor value name or "".
		std::string attribute;
		//GlobalVariable editor ID or "". The tree is listed only while the global is non-zero.
		std::string visibilityGlobal;
		CurrencyType currencyType{ CurrencyType::kPerkPoints };
		//Translation key, defaulted for perk points.
		std::string currencyLabel;
		//"" for perk points.
		std::string globalEditorId;
		std::vector<Perk> perks;
	};

}
