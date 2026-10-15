#pragma once

#include <string>
#include <string_view>

namespace Achievements {

	//The achievements of the loaded save, and everything that changes or shows them.
	//
	//	Papyrus     SKYBAchievements.Unlock(id) / IsUnlocked / CheckProgress / GetUnlockedCount / GetTotalCount / Exists.
	//	            Triggering is Papyrus's business; the definitions are JSON (AchievementData).
	//	Co-save     this save's unlocks: id, local date and time, the game's date string, the character.
	//	On unlock   the toast (unless toastNotifications=0 in SKYBSettings.ini), the machine-wide record
	//	            (AchievementStore), and the mod event SKYBAchievementEarned for any listener.
	//	Menu        SKYBRequestAchievements "menu|target": the whole list, answered on the UI queue.
	//	Journal     SKYBJournalTab n: the next time the journal opens, it is switched to page n.
	class AchievementService {
	public:
		//At plugin load, before Storage::CoSave::Install.
		static void InstallSerialization();

		//At kDataLoaded: reads the definitions, takes the mod-event and menu sinks.
		static void Register();

		//From SKSEScriptRegistrar.
		static void RegisterPapyrus(RE::BSScript::Internal::VirtualMachine* vm);

		//True when this call unlocked it. Unknown ids and ones already earned are logged and return false.
		static bool Unlock(std::string_view id);
		[[nodiscard]] static bool IsUnlocked(std::string_view id);
		//Unlocks a counted achievement once its count global reaches its total. True when it is (now) unlocked.
		static bool CheckProgress(std::string_view id);
		[[nodiscard]] static int UnlockedCount();
	};

}
