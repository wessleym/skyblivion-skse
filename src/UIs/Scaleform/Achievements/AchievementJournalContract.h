#pragma once

//The journal's achievements tab, pushed natively for SKYBAchievementFeed.psc (its 2026-09-26 revision).
namespace Achievements::AchievementJournalContract {
	//Papyrus -> C++, on Journal Menu open:
	namespace ModEvent {
		//strArg: the feed CSV "key|0or1[|icon];...". numArg: the HUD-notification flag, 0 or 1.
		inline constexpr const char* Show = "SKYBUIAchievementShow";
	}

	inline constexpr const char* JournalMenuName = "Journal Menu";

	//C++ -> ActionScript:
	namespace AsPath {
		//([header, csv, notify]) one array argument, the shape the panel expects.
		inline constexpr const char* AddCategory = "_root.QuestJournalFader.Menu_mc.StatsFader.Page_mc.AddAchievementCategory";
	}

	//The category header: a translation key.
	inline constexpr const char* Header = "$SKYB_stats_achievements";
}
