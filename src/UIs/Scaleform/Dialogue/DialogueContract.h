#pragma once

//The Skyblivion dialogue menu's disposition panel and persuasion hotkey.
namespace Dialogue::DialogueContract {
	//Dialogue menu -> C++:
	namespace ModEvent {
		//strArg: "menu,target". Invokes target with a one-element array holding
		//{hasSpeaker, noPersuade, canPersuade, disposition, persuadeRow}:
		//	disposition   The speaker's disposition, 0 to 100.
		//	noPersuade    The speaker is in SKYBNoPersuadeFaction. The menu hides the whole panel.
		//	canPersuade   The persuasion topic's own conditions hold: not in that faction, not in combat.
		//	              The menu shows the hotkey and face icon whenever this is true.
		//	persuadeRow   The persuasion topic's row in the list the game most recently gave the menu, or -1.
		//	              When present, the menu removes the row and clicks it for the hotkey,
		//	              so the topic's own fragment runs. Resolved by form, so it works in every language.
		//	services      [{id, row, available, icon}], the service buttons (see DialogueServices.h).
		//	              As with persuadeRow, the menu removes a present row and clicks it.
		inline constexpr const char* RequestDialogueData = "SKYBRequestDialogueData";
		//The hotkey's fallback when persuadeRow was -1: opens persuasion with the current speaker.
		inline constexpr const char* OpenPersuasion = "SKYBOpenPersuasion";
		//strArg: a service id. A service button's fallback when its row was -1: opens the service with the current speaker.
		inline constexpr const char* OpenDialogueService = "SKYBOpenDialogueService";
	}

	//Optional extension of the service table, relative to the game folder (see DialogueServices.h).
	inline constexpr const char* ServicesIniFile = "Data/SKSE/Plugins/SKYBDialogueServices.ini";

	//Resolved by editor ID (po3 Tweaks), then by form key when editor IDs are unavailable.
	namespace Forms {
		//The "Persuade" topic, whose TopicInfo carries SKYB_TIF_UIPersuasion.
		inline constexpr const char* PersuasionTopicEditorID = "SKYBUIPersuasionTopic";
		inline constexpr RE::FormID PersuasionTopicFormID = 0x2891A0;
		//Membership disables persuasion. It is the GetInFaction condition on the persuasion TopicInfo.
		inline constexpr const char* NoPersuadeFactionEditorID = "SKYBNoPersuadeFaction";
		inline constexpr RE::FormID NoPersuadeFactionFormID = 0x0849E4;
		inline constexpr const char* Plugin = "Skyblivion.esm";
	}
}
