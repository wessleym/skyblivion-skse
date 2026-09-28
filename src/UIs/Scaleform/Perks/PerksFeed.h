#pragma once

#include <string>

namespace PerksView {

	//Answers the perk menu's two data requests. See PerksContract.h for the events.
	//
	//Trees: one object per tree, pre-sorted by priority, which the SWF renders in received order:
	//	{ id, displayName, icon, backgroundArt, themeColor, attribute, currencyType, currencyLabel, globalEditorId,
	//	  perks: [ { id, name, icon, x, y, flavourText, prerequisites: [id],
	//	             ranks: [ { rankId, description, requirements: [ { type, value, name, perkId, rank } ] } ] } ] }
	//	Absent strings are "", numbers that do not apply are 0.
	//
	//State: { level, currency: { treeId: balance }, avs: { name: value }, perkRanks: { perkId: owned },
	//	pointsInTree: { treeId: n }, treeVisible: { treeId: 0|1 }, perkVisible: { perkId: 0|1 } }
	//	Visibility entries exist only for items with a visibilityGlobal, read live on every request;
	//	a global that does not resolve reads as hidden. The owned rank is the highest contiguous rank held.
	class PerksFeed {
	public:
		//Call once, at kDataLoaded.
		static void Register();

	private:
		static void PushTrees(const std::string& request);
		static void PushState(const std::string& request);

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;
	};

}
