#pragma once

//The skills menu's feeds, rebuilt to the retired SkyblivionUI.dll's Scaleform contract,
//so the menu's original callbacks receive exactly what they were written for.
namespace Skills::SkillsContract {
	//Skills menu -> C++:
	namespace ModEvent {
		//strArg: "menu,target,mode". mode "function" invokes target, "variable" assigns it, anything else pushes nothing.
		//Pushes 18 {current, base, maximum, xp, levelThreshold, progress} in the menu's fixed order.
		inline constexpr const char* RequestSkillData = "SKYBRequestSkillData";
		//strArg: "menu,target". numArg: the skill's index in the menu's order.
		//Invokes target with {fullName, description, hasPerk} for each perk in that skill's mastery list.
		inline constexpr const char* RequestMasteryPerks = "SKYBGetMasteryPerkInfo";
	}

	//FormList of 18 per-skill FormLists, four mastery perks each, in the menu's order.
	//Resolved by editor ID, which needs po3 Tweaks.
	inline constexpr const char* MasteryPerkListEditorID = "SKYBMasteryPerkList";
}
