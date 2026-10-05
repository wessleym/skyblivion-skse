#pragma once

#include <string>

namespace StartMenu {

	//The native half of the title screen. See StartMenuContract.h for the events and payloads.
	//
	//Options: difficulty (the engine setting, which a new game reads on start), achievement toasts and
	//achievement scope (SKYBSettings.ini through SettingsService). Any other option is recorded under [NewGame].
	//
	//Events are handled on the sender's thread: skse.SendModEvent from a movie dispatches on the thread
	//advancing it, so invoking the movie from the handler is the ordinary pattern (as in LoadingScreenService).
	class StartMenuService {
	public:
		//Call once, at kDataLoaded, after SettingsService.
		static void Register();

		//At kPostLoadGame, and inside the kNewGame task: logs the UI's state as the game starts and clears
		//UI::modal when it is set with no modal menu left on the stack (seen after the title screen's
		//Creations entry, whose login menu can close without clearing it: only the console then took input).
		static void RepairInputAfterMenus();

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

		//Observes the menus that open and close over the title screen, by name, with the UI's state.
		class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;
		};
		static inline MenuSink s_menuSink;
	};

}
