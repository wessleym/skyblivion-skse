#pragma once

//Replaces the vanilla StatsMenu with a perk menu: its kShow opens the replacement, and the menu is then dismissed.
//The replacement is the SKYBPerksView perk tree (Scaleform) when it is installed,
//else the Stats PrismaUI view, else nothing, which leaves the vanilla screen.
//
//StatsMenu carries kCustomRendering, so while it is on the stack it owns scene drawing,
//and a suppressed instance draws nothing behind the replacement.
//Once it is dismissed, the replacement holds the pause:
//the Stats view focuses with pauseGame=true before the dismissal is queued.
//The perk tree pauses on its own, but Papyrus opens it,
//so the game can run for a few frames between the dismissal and its arrival.
//
//Both hooked slots are identical on SE, AE and VR:
//the VR-only IMenu virtuals are appended at 0x09/0x0A.
//No StatsMenu member is dereferenced, and those offsets do differ between runtimes.
class StatsMenuHook {
public:
	//Idempotent. Installed whether or not PrismaUI is present.
	static void Initialize();

private:
	static RE::UI_MESSAGE_RESULTS ProcessMessage(RE::StatsMenu* a_this, RE::UIMessage& a_message);
	static void PostDisplay(RE::StatsMenu* a_this);

	//Opens the first available replacement. False when none is, so the vanilla screen should open.
	static bool OpenReplacement();

	//Closes StatsMenu, and the Tween menu when StatsMenu was opened from it:
	//a Tween menu left open holds its own pause and blur over the replacement.
	static void DismissHostMenus();

	static inline REL::Relocation<decltype(ProcessMessage)> s_processMessage;
	static inline REL::Relocation<decltype(PostDisplay)> s_postDisplay;

	//Prevents a second install from wrapping the already-installed hook.
	static inline bool s_installed{ false };

	//Set between queuing StatsMenu's dismissal and its arrival,
	//to tell that kHide from the player closing the replacement.
	static inline bool s_dismissing{ false };

	//Set while StatsMenu runs its own close behind the replacement.
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
