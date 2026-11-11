#pragma once

//The journal's System > Help page (quest_journal.swf, SystemPage), fed from a JSON tree instead of the engine's
//single HelpManualPC form list.
namespace Help::HelpContract {
	//quest_journal.swf -> C++:
	namespace ModEvent {
		//strArg: "menu,target[,mode]". Pushes the help tree (HelpFeed).
		inline constexpr const char* RequestHelp = "SKYBRequestHelp";
	}

	//Relative to the game folder. Each subfolder holding a category.json is one category; its other *.json files
	//are its topics, one per file:
	//	category.json   { "id", "displayName", "priority", "importFormList": "HEX|Plugin", "visibilityGlobal": "EditorID" }
	//	<topic>.json    { "id", "title", "priority", "body": "text" | ["paragraph", ...], "visibilityGlobal": "EditorID" }
	//importFormList adds every message of a form list (title = the message's name, text = its description) as a
	//topic. visibilityGlobal: shown only while that global is non-zero.
	//Styles: <root>/styles.json { "styles": { "<tag>": { "size", "color", "face", "bold", "italic", "underline",
	//	"block" } } } turns <tag>...</tag> in topic bodies into font markup ("block": a line break after it).
	inline constexpr const char* DataRoot = "Data/Interface/Skyblivion/ConfigFiles/Help";
	inline constexpr const char* CategoryFileName = "category.json";
	inline constexpr const char* StylesFileName = "styles.json";
}
