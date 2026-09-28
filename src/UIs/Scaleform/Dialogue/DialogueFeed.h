#pragma once

#include <string>

namespace Dialogue {

	//Answers the dialogue menu's disposition panel and service buttons request,
	//and opens persuasion or a service when the menu finds no topic row to click (services: DialogueServices).
	//See DialogueContract.h for the events and payload shape.
	class DialogueFeed {
	public:
		//Call once, at kDataLoaded.
		static void Register();

	private:
		static void PushDialogueData(const std::string& request);

		//Re-checks the persuasion topic's conditions, so this path never opens more than the topic would.
		static void OpenPersuasion();

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;
	};

}
