#pragma once

namespace RE { class Actor; }
class UISystem {
public:
	//SKSE message handlers, called from Plugin.cpp's listener.
	static void OnPostLoad();
	static void OnInputLoaded();
	//kPreLoadGame or kNewGame.
	static void OnGameStarting();
	//kPreLoadGame, with the name of the save about to load.
	static void OnPreLoadGame(std::string_view saveName);
	static void OnDataLoaded();

	static void OpenPersuasion(RE::Actor* target);
	static void OpenSpellMaking();
	static void Initialize();
};
