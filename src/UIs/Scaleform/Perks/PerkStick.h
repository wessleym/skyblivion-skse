#pragma once

#include <mutex>
#include <string>

namespace PerksView {

	//The right thumbstick, for the perk menu's zoom. Scaleform never sees the sticks; this hands the right one to the menu.
	//
	//The menu asks with PerksContract::ModEvent::WatchStick, strArg "menu|function". From then on every
	//right-stick change is called into that function as (x, y), each -1..1, y forward positive, on the
	//main thread (MainThread::Post), the newest value only. A call that does not land (the menu closed,
	//another movie in the same menu) ends the watch.
	class PerkStick : public RE::BSTEventSink<RE::InputEvent*>,
					  public RE::BSTEventSink<SKSE::ModCallbackEvent> {
	public:
		//Call once, at kDataLoaded.
		static void Register();

		RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
			RE::BSTEventSource<RE::InputEvent*>* a_source) override;
		RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
			RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;

	private:
		void Deliver();

		std::mutex  _lock;
		std::string _menu;
		std::string _function;
		float       _x = 0.0f;
		float       _y = 0.0f;
		bool        _posted = false;
	};

}
