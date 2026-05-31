#pragma once

namespace RE { class Actor; }
class UISystem {
public:
	static void OnDataLoaded();
	static void OpenPersuasion(RE::Actor* target);
	static void OpenSpellMaking();
	static void Initialize();
};
