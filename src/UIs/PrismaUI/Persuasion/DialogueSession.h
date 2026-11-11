#pragma once

namespace RE { class Actor; }

namespace Persuasion {
	// Skyrim closes the dialogue menu when idle.
	// This keeps it open while the user is in the persuasion game.
	class DialogueSession {
	public:
		// kDataLoaded: watches deaths, so the wheel closes when its NPC dies.
		static void Register();
		// Holds the NPC in conversation and keeps that hold alive. a_closeWheel closes the wheel: it runs on the main
		// thread if the NPC dies while the wheel is up.
		static void Start(RE::Actor* target, void (*a_closeWheel)());
		// Closes and reopens the dialogue so topics refresh (not with a dead NPC)
		static void End(RE::ActorHandle handle);
		// The game closed the wheel (a load, a cell change; no End will come): stops the hold and a reopen still
		// pending from an End, shows the dialogue menu
		static void Abort();
	};

}
