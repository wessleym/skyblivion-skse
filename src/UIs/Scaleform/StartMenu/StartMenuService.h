#pragma once

#include <string>
#include <vector>

namespace StartMenu {

	//The native half of the title screen. See StartMenuContract.h for the events and payloads.
	//
	//Options: the settings menu's presets for the new game, one row per category with a newgame label (applied by
	//OnNewGame). The achievement rows are in the settings menu; a choice sent for them is still written
	//(SettingsService). Any other option is recorded under [NewGame]. The vanilla difficulty is not offered: every
	//level is x1 (CombatService).
	//
	//Events are handled on the sender's thread: skse.SendModEvent from a movie dispatches on the thread
	//advancing it, so invoking the movie from the handler is the ordinary pattern (as in LoadingScreenService).
	class StartMenuService {
	public:
		//Call once, at kDataLoaded, after SettingsService.
		static void Register();

		//At kPostLoadGame and in the kNewGame task: clears UI::modal when no modal menu is left on the stack
		//(the title screen's Creations login menu can close without clearing it, blocking all input but the console).
		static void RepairInputAfterMenus();

		//In the kNewGame task, after the settings store has filled the new character's values: the presets
		//chosen on the new-game panel, if any, replace its Combat / Progression rows. Once per choice.
		static void OnNewGame();

	private:
		static void PushOptions();
		static void PushLocations();
		static void ApplyOption(const std::string& key, int index);

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;
		//per settings-menu category (kCategories order): the preset the new game takes, -1 none (its current
		//values); and whether its row starts with "Current Settings". Reset each time the options are sent.
		static inline std::vector<int>  s_pending;
		static inline std::vector<bool> s_hasCurrent;
	};

}
