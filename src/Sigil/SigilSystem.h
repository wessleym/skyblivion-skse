#pragma once

#include <string>

namespace Sigil {

	//Answers SKYBSigilEnchant.psc's gem-consume and enchant requests. See SigilContract.h.
	//The sigil menu's data feeds are UI code, in UIs/Scaleform/Sigil.
	class SigilSystem {
	public:
		//Call once, at kDataLoaded.
		static void Register();

	private:
		static void ConsumeGem(const std::string& payload);
		static void ApplyEnchant(const std::string& payload);

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;
	};

}
