#pragma once

//The re-authored title screen (Interface\startmenu.swf, the SKYBStartMenu class). The SWF works without this;
//with it, the save details gain a location row and the new-game settings column becomes live.
namespace StartMenu::StartMenuContract {
	//Title screen -> C++ (skse.SendModEvent from the Main Menu movie):
	namespace ModEvent {
		//The menu is up: answered with AsPath::SetOptions.
		inline constexpr const char* RequestOptions = "SKYBStartRequest";
		//A save list has been filled: answered with AsPath::SetSaveLocations.
		inline constexpr const char* RequestLocations = "SKYBStartLocations";
		//strArg: the option key. numArg: the chosen index.
		inline constexpr const char* SetOption = "SKYBStartOption";
	}

	//C++ -> ActionScript:
	namespace AsPath {
		//([{key, label, values, index, defaultIndex, desc, groups}]) groups: a comma list of "newgame" / "settings",
		//where the row shows.
		inline constexpr const char* SetOptions = "_root.MenuHolder.Menu_mc.SetOptions";
		//([{name, playTime, location, file}]) one per entry of the engine's save list, in its order.
		inline constexpr const char* SetSaveLocations = "_root.MenuHolder.Menu_mc.SetSaveLocations";
	}

	//Option keys.
	namespace Option {
		//iDifficulty:GamePlay in the prefs collection, written through to SkyrimPrefs.ini as the settings menu does.
		inline constexpr const char* Difficulty = "difficulty";
		//Achievements/toastNotifications in SKYBSettings.ini, applied to the toast global at once.
		inline constexpr const char* ToastNotifications = "toastNotifications";
		//Achievements/shared in SKYBSettings.ini: 0 per character, 1 every character's unlocks.
		inline constexpr const char* AchievementScope = "achievementScope";
	}
}
