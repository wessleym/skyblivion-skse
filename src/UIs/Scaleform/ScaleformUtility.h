#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json_fwd.hpp>

class ScaleformUtility {
public:
	//The movie of an open menu, or null when the menu is closed.
	//Every feed pushes through this: a menu can close between a request and the task that answers it.
	[[nodiscard]] static RE::GPtr<RE::GFxMovieView> OpenMenuMovie(std::string_view menuName);

	//The plugin's own menus: an IMenu around Interface\<a_file>.swf whose callbacks come through GameDelegate.
	//For its constructor: loads the movie with an FxDelegate and sets what these menus share (menu-mode input,
	//modal, topmost, the cursor unless a gamepad is in use) at a_depth, pausing the game when a_pausesGame.
	//A movie that does not load is closed at once with a_close: a menu without its movie would sit invisible and modal.
	static void LoadMenuMovie(RE::IMenu& a_menu, std::string_view a_file, std::int8_t a_depth, bool a_pausesGame, void (*a_close)());
	//Whether the menu can be shown: the UI is up, the menu is not open already, and Interface\<a_file>.swf
	//exists, loose or in an archive (a missing movie is logged).
	[[nodiscard]] static bool CanShowMenu(std::string_view a_menu, std::string_view a_file);
	//Queue the menu's show / hide message.
	static void ShowMenu(std::string_view a_menu);
	static void HideMenu(std::string_view a_menu);

	//A JSON value built in a_movie: objects and arrays as Scaleform objects and arrays, strings owned by the movie,
	//numbers, booleans, anything else as null.
	[[nodiscard]] static RE::GFxValue ToGFx(RE::GFxMovieView& a_movie, const nlohmann::json& a_json);

	//A gamepad is the active input device.
	[[nodiscard]] static bool GamepadActive();

	//For the feeds' timing log lines.
	[[nodiscard]] static long long MicrosecondsSince(std::chrono::steady_clock::time_point start);

	//Runs attempt on the main thread (MainThread::Post) until it returns true, at most attempts times,
	//waiting interval between tries. onGiveUp then runs there too if every attempt returned false.
	//The wait is a sleeping thread, so that the interval is real time, not a count of frames.
	static void RetryOnMainThread(int attempts, std::chrono::milliseconds interval,
		std::function<bool()> attempt, std::function<void()> onGiveUp);
};

//A menu's data request, "menu,target[,mode]": the open menu's movie, the path the answer goes to,
//and how it goes there.
struct FeedRequest {
	//Whether a feed reads the request's third field.
	enum class Mode {
		//"menu,target". Any further field is ignored and the target is always invoked.
		Ignored,
		//"menu,target[,mode]". "function" (also when left out) invokes the target, "variable" assigns it,
		//anything else pushes nothing.
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

//A menu's data request, "menu|target": the open menu, and the absolute path of the function the answer is passed to as its one argument.
struct MenuCallback {
	RE::GPtr<RE::GFxMovieView> movie;
	std::string target;

	//Null when the request is malformed, which is logged under source,
	//or when its menu is closed, which is silent (see ScaleformUtility::OpenMenuMovie).
	[[nodiscard]] static std::optional<MenuCallback> Open(const std::string& request, std::string_view source);

	void Invoke(const RE::GFxValue& value) const;
};
