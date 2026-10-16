#pragma once

namespace RE { class Actor; }

namespace Persuasion {
	// Skyrim closes the dialogue menu when idle.
	// This keeps it open while the user is in the persuasion game.
	class DialogueSession {
	public:
		// Holds the NPC in conversation and keeps that hold alive
		static void Start(RE::Actor* target);
		// Closes and reopens the dialogue so topics refresh. The handle form is the one to use: it stays
		// valid across the frames the reopen takes and keeps the NPC loaded; the form-ID overload is for
		// callers that still hold the target that way.
		static void End(RE::ActorHandle handle);
		static void End(RE::FormID formID);
	};

}
