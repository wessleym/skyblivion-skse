#pragma once

#include <string_view>

//SKYBSettings.ini, in the SKSE log folder beside the plugin logs:
//	[NewGame]
//	difficulty=2            index into the difficulty ladder (engine setting)
//	[Achievements]
//	toastNotifications=1    0 = no HUD toast on unlock (the unlock itself is unaffected)
//	shared=0                1 = the achievements menu opens on every character's unlocks (SKYBAchievements.json)
namespace Settings::SettingsContract {
	inline constexpr const char* FileName = "SKYBSettings.ini";

	inline constexpr std::string_view NewGameSection = "NewGame";
	inline constexpr std::string_view AchievementsSection = "Achievements";
	inline constexpr std::string_view ToastKey = "toastNotifications";
	inline constexpr std::string_view SharedKey = "shared";
}
