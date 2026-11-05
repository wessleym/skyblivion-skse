#pragma once

//The level-up screen's music (Skyblivion/SKYBAdvancement.swf, a CustomMenu opened by SKYBUILevelUp).
//
//The movie announces itself with the SKYBAdvancementReady mod event and hands in the player's choice with
//SKYBAdvancementChosen. On Ready the music starts: the ESM's SKYBUILevelUpSD (Sound\UI\SKYBUILevelUpMenu.xwm,
//UI category, so it plays while the game is paused), which no sound marker points at, played here from its
//descriptor. It fades out when the choice comes in (the movie plays the level-up sting itself) or when the
//CustomMenu closes. Closed without a choice (Esc: the level is kept for later) it also plays the cancel sound.
//The movie's own clicks (steps, focus, confirm) are its PlaySound calls.
namespace Advancement {

	class AdvancementSound {
	public:
		//Call once, at kDataLoaded.
		static void Register();

	private:
		static void Start();
		static void Stop(bool cancelled);

		class Sink :
			public RE::BSTEventSink<SKSE::ModCallbackEvent>,
			public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;
		};
		static inline Sink s_sink;
	};

}
