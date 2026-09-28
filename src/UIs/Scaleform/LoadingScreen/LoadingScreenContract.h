#pragma once

namespace LoadingScreen::LoadingScreenContract {
	//SKYBLoadingMenu.as -> C++, both fired from FadeInMenu:
	namespace ModEvent {
		//Answered with AsPath::SetLocationText.
		inline constexpr const char* RequestLocation = "SKYBLoadingLocation";
		//Answered with AsPath::SetGamepad.
		inline constexpr const char* RequestPlatform = "SKYBLoadingPlatform";
	}

	//C++ -> ActionScript:
	namespace AsPath {
		//(name) The destination's name, or "" to hide the cartouche.
		inline constexpr const char* SetLocationText = "_root.Menu_mc.SetLocationText";
		//(bool) True to show LB/RB in the hint bar instead of Q/E.
		inline constexpr const char* SetGamepad = "_root.Menu_mc.SetGamepad";
		//(bool) Shows or hides the carved frame.
		inline constexpr const char* SetChromeVisible = "_root.Menu_mc.SetChromeVisible";
		//() Steps the tips. Each ignores a second call within 150 ms,
		//so a press arriving through both the SWF's listener and the native input sink advances once.
		inline constexpr const char* NextTip = "_root.Menu_mc.nextTip";
		inline constexpr const char* PreviousTip = "_root.Menu_mc.prevTip";
	}
}
