#pragma once

#include <string>

namespace Factions {

	//Answers the character menu's faction page request natively.
	//See FactionsContract.h for the event and payload shape.
	class FactionFeed {
	public:
		//Call once, at kDataLoaded.
		static void Register();

	private:
		static void PushFactions(const std::string& request);

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;
	};

}
