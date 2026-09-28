#pragma once

namespace Translation::TranslationContract {
	//Papyrus or Scaleform -> C++:
	namespace ModEvent {
		//Re-reads every strings file and re-applies it. Menus show the new text the next time they open.
		inline constexpr const char* ReloadStrings = "SKYBUIReloadStrings";
	}

	//Relative to the game folder. Files live in <StringsRoot>/<language>/*.txt.
	inline constexpr const char* StringsRoot = "Data/Interface/Skyblivion/Strings";
	//Used when sLanguage:General is unset, and when the language has no folder.
	inline constexpr const char* FallbackLanguage = "english";
}
