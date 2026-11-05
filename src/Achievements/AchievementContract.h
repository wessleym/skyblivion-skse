#pragma once

#include <cstdint>

//Achievements: defined as JSON, unlocked from Papyrus by id, kept per save in the co-save,
//shown in the journal's Achievements tab (Interface\Skyblivion\SKYBAchievementsMenu.swf) and announced by the toast.
namespace Achievements::AchievementContract {

	//Relative to the game folder. Each subfolder holding a category.json is one category;
	//its other *.json files are its achievements, one per file:
	//	category.json   { "id", "priority", "displayName" }
	//	<id>.json       { "id", "priority", "name", "description",
	//	                  "lockedName"?, "lockedDescription"?, "hidden"?, "icon"?,
	//	                  "progress"?: { "countGlobal", "total" | "totalGlobal" } }
	//Text fields are $keys (Interface\Skyblivion\Strings) or plain text. "icon" names a file in Interface\Skyblivion\Art\Awards.
	inline constexpr const char* DataRoot = "Data/Interface/Skyblivion/ConfigFiles/Achievements";
	inline constexpr const char* CategoryFileName = "category.json";

	//Papyrus: the native script (Source\Scripts\SKYBAchievements.psc).
	inline constexpr const char* ScriptName = "SKYBAchievements";

	//Mod events.
	namespace ModEvent {
		//Menu -> C++. strArg "menu|target": the target function gets the whole list as one object.
		inline constexpr const char* Request = "SKYBRequestAchievements";
		//Menu -> C++. numArg: the journal tab to show when the journal opens next (0 quests: the journal hands on to the quest menu; 1 general stats; 2 system).
		inline constexpr const char* JournalTab = "SKYBJournalTab";
		//C++ -> Papyrus, after every new unlock. strArg: the id.
		inline constexpr const char* Earned = "SKYBAchievementEarned";
	}

	inline constexpr const char* JournalMenuName = "Journal Menu";
	//The achievements menu's container (SKYBUIUtility opens it with UI.OpenCustomMenu).
	inline constexpr const char* CustomMenuName = "CustomMenu";
	//The journal's page switch, added by our quest_journal.swf.
	inline constexpr const char* JournalShowTab = "_root.QuestJournalFader.Menu_mc.skybShowTab";

	//Quest Journal Overhaul's quest menu (the journal's QUESTS page). Its tab row is the Tabs Addon's (QJTabs_mc, depth
	//1000); the ACHIEVEMENTS item is added beside it by loading SKYBQuestTabItem.swf into a clip of our own.
	namespace QuestMenu {
		inline constexpr const char* Name = "QuestMenu";
		inline constexpr const char* Holder = "_root.QuestJournal_mc";
		inline constexpr const char* CreateClip = "_root.QuestJournal_mc.createEmptyMovieClip";
		inline constexpr const char* ClipName = "SKYBAchTab_mc";
		inline constexpr double ClipDepth = 1001;
		inline constexpr const char* LoadMovie = "_root.QuestJournal_mc.SKYBAchTab_mc.loadMovie";
		inline constexpr const char* Movie = "Skyblivion/SKYBQuestTabItem.swf";
		//Native functions set on the holder for the item: the hand-over (the quest menu has no skse object) and a log line.
		inline constexpr const char* HandOverMember = "skybAchievementsHandOver";
		inline constexpr const char* LogMember = "skybAchievementsLog";
		//What the hand-over sends: the dispatcher's menu switch (SKYBUIUtility) to the achievements menu.
		inline constexpr const char* MenuSwitch = "SKYBMenuSwitch";
		inline constexpr const char* MenuSwitchTab = "AchievementsMenu";
	}

	//The toast's header line.
	inline constexpr const char* ToastHeader = "$SKYB_ach_unlocked_header";

	//In the SKSE log folder: Documents\My Games\<game>\SKSE\. Every achievement any character on this machine earned.
	inline constexpr const char* RecordFileName = "SKYBAchievements.json";

	//Co-save record: this save's unlocks.
	inline constexpr std::uint32_t RecordType = 'SKAC';
	inline constexpr std::uint32_t RecordVersion = 1;
}
