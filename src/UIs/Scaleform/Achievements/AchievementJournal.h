#pragma once

#include <string>

namespace Achievements {

	//Pushes the journal's achievements tab. See AchievementJournalContract.h for the event and payload.
	//
	//With SKYBSettings.ini [Achievements] shared=1, the cross-save record (AchievementStore) is folded in first:
	//every key it holds is flipped to unlocked and moved into the unlocked block,
	//so the tab shows the union across characters. Nothing here writes the record.
	class AchievementJournal {
	public:
		//Call once, at kDataLoaded, after SettingsService.
		static void Register();

	private:
		//As a UI task: the journal movie and the store are main-thread state.
		static void Push(const std::string& csv, bool notify);

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;
	};

}
