#pragma once

#include <string>

namespace Help {

	//Answers the journal help page's request. See HelpContract.h for the event and the data files.
	//
	//The push: an array of categories in display order, each { id, name, topics: [ { id, title, body } ] }.
	//A topic or category whose visibilityGlobal is zero, or does not resolve, is left out (read on every request);
	//a category left with no topic is left out too. An empty array means the page keeps the game's own list.
	class HelpFeed {
	public:
		//Call once, at kDataLoaded.
		static void Register();

	private:
		static void PushHelp(const std::string& request);

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;
	};

}
