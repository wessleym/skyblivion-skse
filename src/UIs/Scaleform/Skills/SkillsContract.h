#pragma once

//The editor ID of the mastery perk FormList that SkillData::MasteryPerks reads.
namespace Skills::SkillsContract {
	//FormList of 18 per-skill FormLists, four mastery perks each, in SkillData::kSkillOrder.
	//Resolved by editor ID, which needs po3 Tweaks.
	inline constexpr const char* MasteryPerkListEditorID = "SKYBMasteryPerkList";
}
