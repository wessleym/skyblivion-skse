#pragma once

//SKYBAchievementFeed.psc and future menus <-> the cross-save achievement store.
namespace Achievements::AchievementContract {
	//Papyrus -> C++:
	namespace ModEvent {
		//strArg: the feed cache, "key|0or1[|icon];...". Sent from rebuildAchievementCache(),
		//which runs at every save load, unlock, journal open and panel toggle.
		inline constexpr const char* Sync = "SKYBUIAchievementSync";
		//Answered with FileData.
		inline constexpr const char* RequestFileData = "SKYBUIAchievementFileRequest";
	}

	//C++ -> Papyrus:
	namespace Answer {
		//strArg: comma-separated keys unlocked by any character. numArg: the key count.
		inline constexpr const char* FileData = "SKYBUIAchievementFileData";
	}

	//In the SKSE log folder: Documents\My Games\<game>\SKSE\.
	inline constexpr const char* FileName = "SKYBAchievements.json";
}
