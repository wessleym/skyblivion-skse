#pragma once

// Seconds of unpaused play, measured with the system's high-resolution clock.
namespace ScriptClock
{
	void OnFrame();

	// The total, which doesn't change while the game is paused.
	[[nodiscard]] double Seconds();
}
