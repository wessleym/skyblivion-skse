#pragma once

#include <string>

namespace Skills {

	//Answers the skills menu's data requests natively.
	//See SkillsContract.h for the events and payload shapes.
	class SkillsFeed {
	public:
		//Call once, at kDataLoaded.
		static void Register();

	private:
		static void PushSkillData(const std::string& request);
		static void PushMasteryPerks(const std::string& request, int skillIndex);

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;
	};

}
