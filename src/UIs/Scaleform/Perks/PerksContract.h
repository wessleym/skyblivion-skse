#pragma once

//The SKYBPerksView perk menu (Scaleform), ported from SKYBPerkService.
//The menu's event contract and JSON schemas are defined by the SKYBPerksView design documents.
namespace PerksView::PerksContract {
	//SKYBPerksView.swf -> C++:
	namespace ModEvent {
		//strArg: "menu,target[,mode]". Pushes the tree definitions (PerksFeed).
		inline constexpr const char* RequestTrees = "SKYBRequestPerkTrees";
		//strArg: "menu,target[,mode]". Pushes the player's live state (PerksFeed).
		inline constexpr const char* RequestState = "SKYBRequestPerkState";
		//strArg: "treeId^perkId^HEX|Plugin^currencyType[^globalEditorId]". numArg: the rank record's form ID.
		//Papyrus adds the perk; C++ deducts its cost (PerkCurrency).
		inline constexpr const char* Acquire = "SKYBUIPerkAcquire";
		//strArg: a menu tab. The existing menu-switching event; Papyrus opens the menu it names.
		inline constexpr const char* OpenCustomMenu = "SKYBUIOpenCustomMenu";
	}

	namespace Menu {
		//OpenCustomMenu's strArg for this menu.
		inline constexpr const char* Tab = "PerkMenu";
		//Relative to Data, for checking the menu is installed, loose or in an archive.
		inline constexpr const char* SwfFile = "Interface/Skyblivion/SKYBPerksView.swf";
	}

	//Relative to the game folder. Each subfolder holding a tree.json is one tree; its other *.json files are its perks.
	inline constexpr const char* DataRoot = "Data/Interface/Skyblivion/ConfigFiles/Perks";
}
