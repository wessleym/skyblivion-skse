#pragma once

#include <string>

namespace Dialogue {

	//Oblivion's service buttons in the dialogue menu: barter and training, with repair and recharge reserved.
	//
	//Each service has topics (the prompts whose fragments open it) and factions (who offers it).
	//The built-in table can be extended or overridden by DialogueContract::ServicesIniFile:
	//	[barter]                       ; section = service id
	//	topics=EditorID|FormKey|icon   ; comma-separated; FormKey ("210285:Skyblivion.esm") and icon optional
	//	factions=EditorID|FormKey      ; comma-separated
	//	fallback=1                     ; offer and open with no row (default 0)
	//	open=Class.Function            ; Papyrus static call, (Actor speaker)
	//	clear=1                        ; drop the built-in topics and factions first
	//A service id the SWF does not know is carried in the feed and ignored there.
	//
	//All access is on the main thread (SKSE tasks), so the table needs no lock.
	class DialogueServices {
	public:
		//Adds "services": [{id, row, available, icon}] to the dialogue feed's record, one per service.
		//	row        where the first of the service's topics sits in the list the game most recently gave the menu, or -1.
		//	available  row >= 0, or, when the service allows the fallback, the speaker is in one of its factions
		//	           (and, for barter, has a vendor faction).
		//	icon       the matched topic's icon, or "".
		//Returns a summary for the feed's log line.
		static std::string AddToRecord(RE::GFxMovieView& movie, RE::GFxValue& record, RE::Actor* speaker);

		//The fallback when a service button fires and its topic was not in the list to click.
		//Re-checks the table and the speaker's factions, so it never opens more than the feed offered,
		//then makes the call the topic's fragment would: Actor.ShowBarterMenu, Game.ShowTrainingMenu,
		//or the table's open=Class.Function, each with the speaker.
		static void Open(const std::string& serviceID, RE::Actor* speaker);
	};

}
