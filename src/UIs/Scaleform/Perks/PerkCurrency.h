#pragma once

#include <string>

namespace PerksView {

	//Pays for a perk rank bought in the perk menu. See PerksContract::ModEvent::Acquire.
	//
	//The event reaches both sides: Papyrus adds the rank's perk record, and this deducts its cost of one.
	//perkPoints spend from the engine's perk-point pool (Papyrus Game.AddPerkPoints(-1) did not move it on this runtime);
	//globalVariable spends from the tree's GlobalVariable. Both stop at 0.
	class PerkCurrency {
	public:
		//Call once, at kDataLoaded.
		static void Register();

	private:
		static void Deduct(const std::string& payload);

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;
	};

}
