#pragma once

namespace LoadingScreen {

	//Native feeds for the Skyblivion loading screen (SKYBLoadingMenu).
	//
	//LoadingMenu holds the location the game is loading towards,
	//but its GameDelegate exposes only RequestLoadingText and RequestPlayerInfo to ActionScript.
	//The SWF asks for the rest by mod event, and this answers.
	//
	//The pushes are also driven by the menu opening, not only by the SWF's requests.
	//The first loading screen of a session is up before kDataLoaded,
	//so sinks are taken as early as kPostLoad, at the first message where each source exists.
	//
	//Q/E and the shoulder buttons are also read here from raw input:
	//the Loading Menu's input context is kNone, and GFx may not hand it key events.
	//
	//Nothing here goes through SKSE's task interface. Menu events, input events,
	//and Scaleform's skse.SendModEvent all arrive on the main thread,
	//and the task queue is not drained while a load runs:
	//a deferred push once arrived six seconds late, after the loading menu had closed.
	class LoadingScreenService {
	public:
		//Takes the mod-event and menu sinks. Idempotent: call at kPostLoad, kInputLoaded and kDataLoaded.
		static void Register();

		//Takes the raw input sink. Call at kInputLoaded, when the input manager first exists.
		static void RegisterInput();

		//kPreLoadGame or kNewGame: a game is starting even if the title screen never showed.
		static void EndBootPhase();

	private:
		static void PushLocation();
		static void PushGamepad();
		static void PushChromeVisible(bool visible);
		static void PushStep(const char* asPath);
		static void LogOpenMenus();

		class Sink :
			public RE::BSTEventSink<SKSE::ModCallbackEvent>,
			public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
			public RE::BSTEventSink<RE::InputEvent*> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
				RE::BSTEventSource<RE::InputEvent*>* a_source) override;
		};
		static inline Sink s_sink;

		static inline bool s_modEventSinkAdded = false;
		static inline bool s_menuSinkAdded = false;
		static inline bool s_inputSinkAdded = false;

		//False until the title screen has come and gone, or a game starts without it.
		//The boot load, before the title screen, is not one the player asked for, so its frame stays hidden.
		static inline bool s_bootDone = false;
	};

}
