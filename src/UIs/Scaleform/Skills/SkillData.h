#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

//The player's skill data, read once for every menu that shows skills:
//the skills menu's feed (SkillsFeed) and the skill sheet's (SkillSheetFeed).
namespace Skills::SkillData {
	//Oblivion's skill categories, alphabetical within each.
	//Matches the order of SKYBMasteryPerkList's inner lists.
	inline constexpr std::array<RE::ActorValue, 18> kSkillOrder{
		RE::ActorValue::kArchery,     RE::ActorValue::kBlock,       RE::ActorValue::kHeavyArmor,
		RE::ActorValue::kOneHanded,   RE::ActorValue::kSmithing,    RE::ActorValue::kTwoHanded,
		RE::ActorValue::kAlteration,  RE::ActorValue::kConjuration, RE::ActorValue::kDestruction,
		RE::ActorValue::kEnchanting,  RE::ActorValue::kIllusion,    RE::ActorValue::kRestoration,
		RE::ActorValue::kAlchemy,     RE::ActorValue::kLightArmor,  RE::ActorValue::kLockpicking,
		RE::ActorValue::kPickpocket,  RE::ActorValue::kSneak,       RE::ActorValue::kSpeech,
	};

	//A skill's place in kSkillOrder; empty for an actor value that is not one of the 18.
	[[nodiscard]] std::optional<std::size_t> IndexOf(RE::ActorValue actorValue);

	struct Reading {
		RE::ActorValue actorValue;
		//With fortify and damage effects.
		float current;
		float base;
		float permanent;
		float xp;
		float levelThreshold;
		//xp / levelThreshold as a percentage; 0 when the threshold is not positive.
		float progress;
	};

	//All 18 skills in kSkillOrder. Empty, logged under source, when the player's data is unavailable.
	[[nodiscard]] std::optional<std::array<Reading, 18>> ReadAll(std::string_view source);

	struct MasteryPerk {
		RE::BGSPerk* perk;
		std::string name;
		std::string description;
		bool owned;
	};

	//The mastery perks of the skill at index in kSkillOrder, in their list's order (lowest rank first).
	//Empty, logged under source, when SKYBMasteryPerkList is missing or has no list for index.
	[[nodiscard]] std::optional<std::vector<MasteryPerk>> MasteryPerks(std::size_t index, std::string_view source);
}
