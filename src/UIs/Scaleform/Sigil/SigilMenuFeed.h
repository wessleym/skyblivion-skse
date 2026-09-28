#pragma once

#include <string>

namespace Sigil {

	//Builds the sigil stone menu's gear and stone lists natively. See SigilMenuContract.h.
	class SigilMenuFeed {
	public:
		//Call once, at kDataLoaded.
		static void Register();

	private:
		//Unenchanted non-staff weapons and unenchanted apparel, equipped included.
		//Quest items and MagicDisallowEnchanting gear are excluded. Base-form checks only, as the Papyrus builder did.
		static void PushGear();

		//Carried stones in list-index order, each with its weapon and armor enchantments read live, and its tier's gem count.
		static void PushStones(const std::string& payload);

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;
	};

}
