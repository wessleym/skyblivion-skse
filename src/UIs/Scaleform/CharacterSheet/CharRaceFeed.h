#pragma once

#include <string>

namespace CharacterSheet {

	//Hands the character sheet the race menu's data file, parsed here.
	//
	//The sheet sends SKYBCharRaceRequest, strArg "menu|function". The function is called, on the main thread,
	//with the "races" array of Data/Interface/Skyblivion/ConfigFiles/Races/SKYBRaceDetails.json as Scaleform
	//objects (every field as the file has it), or an empty array when the file is missing or not valid JSON.
	//The file is read on every request, so an edit shows the next time the sheet opens. Parsing it in
	//ActionScript cost the sheet a visible stutter.
	class CharRaceFeed {
	public:
		//Call once, at kDataLoaded.
		static void Register();

	private:
		static void PushRaces(const std::string& request);

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;
	};

}
