#include "Platform/ScriptClock.h"

#include <atomic>
#include <chrono>

namespace ScriptClock
{
	namespace
	{
		using Clock = std::chrono::steady_clock;
		std::atomic<double> g_seconds{ 0.0 };
		Clock::time_point g_lastFrame{};
	}

	void OnFrame()
	{
		const Clock::time_point now = Clock::now();
		if (g_lastFrame != Clock::time_point{} && !RE::UI::GetSingleton()->GameIsPaused()) {
			g_seconds.store(g_seconds.load() + std::chrono::duration<double>(now - g_lastFrame).count());
		}
		g_lastFrame = now;
	}

	double Seconds()
	{
		return g_seconds.load();
	}
}
