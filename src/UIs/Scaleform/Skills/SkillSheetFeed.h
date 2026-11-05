#pragma once

#include <string>

namespace Skills {

	//Answers the skill sheet's data requests natively.
	//See SkillSheetContract.h for the events and payload shapes.
	class SkillSheetFeed {
	public:
		//Call once, at kDataLoaded.
		static void Register();

	private:
		static void PushSheet(const std::string& request);
		static void PushMastery(const std::string& request, int actorValue);

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;
	};

}
