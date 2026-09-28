#pragma once

#include <string_view>

//SKYBSettings.ini, in the SKSE log folder beside the plugin logs:
//	[NewGame]
//	difficulty=2            index into the difficulty ladder (engine setting)
//	[Achievements]
//	toastNotifications=1    0 = no HUD toast on unlock (the journal row is unaffected)
//	shared=0                1 = the journal tab shows every character's unlocks (SKYBAchievements.json)
namespace Settings::SettingsContract {
	inline constexpr const char* FileName = "SKYBSettings.ini";

	inline constexpr std::string_view NewGameSection = "NewGame";
	inline constexpr std::string_view AchievementsSection = "Achievements";
	inline constexpr std::string_view ToastKey = "toastNotifications";
	inline constexpr std::string_view SharedKey = "shared";

	//Mirrors Achievements/toastNotifications. SKYBAchievementFeed.psc checks it before asking for a toast.
	inline constexpr const char* ToastGlobalEditorID = "SKYBAchieveNotifications";

	//Papyrus -> C++:
	namespace ModEvent {
		//The journal achievements panel's toggle row. numArg: the new state.
		inline constexpr const char* AchievementNotifyToggle = "SKYBAchievementNotifyToggle";
	}
}
