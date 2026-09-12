#pragma once

namespace Stats {

	//Redirects StatsMenu to the Stats PrismaUI view:
	//its kShow opens the view, and the menu is then dismissed.
	//
	//StatsMenu carries kCustomRendering, so while it is on the stack it owns scene drawing,
	//and a suppressed instance draws nothing behind the view.
	//Dismissed, PrismaUI holds the pause instead (StatsView::Open focuses with pauseGame=true).
	//
	//Both hooked slots are identical on SE, AE and VR:
	//the VR-only IMenu virtuals are appended at 0x09/0x0A.
	//No StatsMenu member is dereferenced, and those offsets do differ between runtimes.
	class StatsMenuHook {
	public:
		static void Initialize();

	private:
		static RE::UI_MESSAGE_RESULTS ProcessMessage(RE::StatsMenu* a_this, RE::UIMessage& a_message);
		static void PostDisplay(RE::StatsMenu* a_this);

		//Closes StatsMenu, and the Tween menu when the player came that way:
		//a Tween menu left open holds its own pause and blur over the view.
		static void DismissHostMenus();

		static inline REL::Relocation<decltype(ProcessMessage)> s_processMessage;
		static inline REL::Relocation<decltype(PostDisplay)> s_postDisplay;

		//Set between queuing StatsMenu's dismissal and its arrival,
		//to tell that kHide from the player closing the view.
		static inline bool s_dismissing{ false };

		//Set while StatsMenu runs its own close behind the view.
		//It is sent kHide on every frame of that close,
		//so those messages must not be read as the player leaving.
		static inline bool s_statsMenuClosing{ false };

		//Clears s_statsMenuClosing once StatsMenu has left the stack,
		//which no single message marks.
		class StatsMenuCloseSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;
		};
		static inline StatsMenuCloseSink s_closeSink;

		//IMenu vtable slots; see RE/I/IMenu.h.
		static constexpr std::size_t kProcessMessageIdx{ 0x04 };
		static constexpr std::size_t kPostDisplayIdx{ 0x06 };
	};

}
