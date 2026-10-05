#pragma once

//The skill sheet's feeds: the Skills menu of the new menus, keyed by actor value.
//Replaces SkillsContract's events when the old skills menu retires.
namespace Skills::SkillSheetContract {
	//Skill sheet -> C++:
	namespace ModEvent {
		//strArg: "menu|target", target the absolute path of one of the menu's functions.
		//Invokes target with {gamepad, skills}: skills holds 18 {av, effective, base, percent}.
		inline constexpr const char* RequestSheet = "SKYBRequestSkillSheet";
		//strArg: "menu|target". numArg: the skill's actor value.
		//Invokes target with {av, ranks}: ranks holds the skill's mastery perks, lowest rank first,
		//as {name, text, owned}. Always answers; ranks is empty when the perks cannot be read.
		inline constexpr const char* RequestMastery = "SKYBRequestSkillMastery";
	}

	//percent is below 100: a skill shows as full only once it has gone up a level.
	inline constexpr float MaxPercent = 99.99f;
}
