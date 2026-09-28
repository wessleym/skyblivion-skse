#pragma once

namespace Toast::ToastContract {
	//Papyrus or Scaleform -> C++:
	namespace ModEvent {
		//strArg: "header|title|desc|icon". Trailing fields are optional.
		//$keys translate inside the movie; an empty icon draws the star fallback.
		inline constexpr const char* ShowToast = "SKYBUIShowToast";
	}

	//The toast movie, loaded into the HUD movie as a child clip.
	namespace Movie {
		inline constexpr const char* Path = "Skyblivion/SKYBAchievementToast.swf";
		inline constexpr const char* ClipName = "SKYBToast";
		//Far above HUDMovieBaseInstance (depth 1) and its overlay (depth 195).
		inline constexpr double ClipDepth = 5533;
	}

	//C++ -> ActionScript:
	namespace AsPath {
		inline constexpr const char* CreateClip = "_root.createEmptyMovieClip";
		inline constexpr const char* LoadMovie = "_root.SKYBToast.loadMovie";
		//Set true by the toast movie once it can take showAchievement calls.
		inline constexpr const char* ReadyVariable = "_root.SKYBToast.SKYBReady";
		//(header, title, desc, icon). The movie queues and displays toasts one at a time.
		inline constexpr const char* ShowAchievement = "_root.SKYBToast.showAchievement";
	}
}
