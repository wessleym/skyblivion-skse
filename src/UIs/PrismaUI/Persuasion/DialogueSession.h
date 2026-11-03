#pragma once

namespace RE { class Actor; }

namespace Persuasion {
	// Skyrim closes the dialogue menu when idle.
	// This keeps it open while the user is in the persuasion game.
	class DialogueSession {
	public:
		// Holds the NPC in conversation and keeps that hold alive
		static void Start(RE::Actor* target);
		// Closes and reopens the dialogue so topics refresh
		static void End(RE::FormID formID);
	};

}
