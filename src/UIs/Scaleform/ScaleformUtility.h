#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

class ScaleformUtility {
public:
	//The movie of an open menu, or null when the menu is closed.
	//Every feed pushes through this: a menu can close between a request and the task that answers it.
	[[nodiscard]] static RE::GPtr<RE::GFxMovieView> OpenMenuMovie(std::string_view menuName);

	//For the feeds' timing log lines.
	[[nodiscard]] static long long MicrosecondsSince(std::chrono::steady_clock::time_point start);

	//Runs attempt as a UI task until it returns true, at most attempts times, waiting interval between tries.
	//onGiveUp then runs as a UI task if every attempt returned false.
	//The wait is a sleeping thread, not a task that queues itself again:
	//SKSE drains its queues in a loop, so a re-queued task runs in the same pass, not a later frame.
	static void RetryOnUIThread(int attempts, std::chrono::milliseconds interval,
		std::function<bool()> attempt, std::function<void()> onGiveUp);
};

//A menu's data request in the retired SkyblivionUI.dll's shape, "menu,target[,mode]",
//with the menu's movie looked up.
struct FeedRequest {
	//Whether a feed reads the request's third field.
	enum class Mode {
		//"menu,target". Any further field is ignored and the target is always invoked.
		Ignored,
		//"menu,target,mode". "function" invokes the target, "variable" assigns it, anything else pushes nothing.
		Required,
		//As Required, but the mode may be left out, meaning "function".
		Optional
	};

	RE::GPtr<RE::GFxMovieView> movie;
	std::string target;
	//"function" when the feed ignores the field or it was left out.
	std::string mode;

	//Null when the request is malformed, which is logged under source,
	//or when its menu is closed, which is silent (see ScaleformUtility::OpenMenuMovie).
	[[nodiscard]] static std::optional<FeedRequest> Open(const std::string& request, Mode modeField, std::string_view source);

	//Pushes the value to the target as the mode says. False, having pushed nothing, for an unknown mode.
	bool Push(const RE::GFxValue& value) const;
};
