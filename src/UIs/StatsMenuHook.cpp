#include "StatsMenuHook.h"
#include "PrismaUI/Stats/StatsView.h"
#include "Scaleform/Perks/PerksMenu.h"

void StatsMenuHook::Initialize() {
	if (s_installed) {
		return;
	}
	s_installed = true;
	REL::Relocation<std::uintptr_t> vtable{ RE::VTABLE_StatsMenu[0] };
	s_processMessage = vtable.write_vfunc(kProcessMessageIdx, ProcessMessage);
	s_postDisplay = vtable.write_vfunc(kPostDisplayIdx, PostDisplay);
	Log::INFO("StatsMenuHook: StatsMenu vtable hooked. A perk menu replaces the vanilla screen when one is available.");
	if (auto* ui = RE::UI::GetSingleton()) {
		ui->AddEventSink<RE::MenuOpenCloseEvent>(&s_closeSink);
	}
}

RE::BSEventNotifyControl StatsMenuHook::StatsMenuCloseSink::ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
	RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
	if (a_event && !a_event->opening && a_event->menuName == RE::StatsMenu::MENU_NAME) {
		s_statsMenuClosing = false;
	}
	return RE::BSEventNotifyControl::kContinue;
}

bool StatsMenuHook::OpenReplacement() {
	if (PerksView::PerksMenu::IsAvailable()) {
		PerksView::PerksMenu::Open();
		return true;
	}
	if (Stats::StatsView::IsAvailable()) {
		Stats::StatsView::Open();
		return true;
	}
	return false;
}

void StatsMenuHook::DismissHostMenus() {
	auto* ui = RE::UI::GetSingleton();
	auto* queue = RE::UIMessageQueue::GetSingleton();
	if (!ui || !queue) {
		Log::WARN("StatsMenuHook: UI unavailable. StatsMenu stays on the stack and suppresses scene rendering.");
		return;
	}

	//kHide, not kForceHide: kForceHide skips the teardown and leaves the screen black.
	queue->AddMessage(RE::StatsMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);

	const bool tweenOpen = ui->IsMenuOpen(RE::TweenMenu::MENU_NAME);
	if (tweenOpen) {
		queue->AddMessage(RE::TweenMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
	}
	Log::INFO("StatsMenuHook: Dismissing StatsMenu{}.", tweenOpen ? " and the Tween menu" : "");
}

RE::UI_MESSAGE_RESULTS StatsMenuHook::ProcessMessage(RE::StatsMenu* a_this, RE::UIMessage& a_message) {
	switch (a_message.type.get()) {
	case RE::UI_MESSAGE_TYPE::kShow:
		//Opened before the dismissal is queued, so the Stats view takes the pause with it.
		//With no replacement, the menu is left untouched so the vanilla screen opens.
		if (!OpenReplacement()) {
			Log::WARN("StatsMenuHook: No perk menu available. Opening the vanilla StatsMenu.");
			return s_processMessage(a_this, a_message);
		}
		s_statsMenuClosing = false;
		s_dismissing = true;
		DismissHostMenus();
		return RE::UI_MESSAGE_RESULTS::kHandled;

	case RE::UI_MESSAGE_TYPE::kHide:
	case RE::UI_MESSAGE_TYPE::kForceHide:
		if (s_dismissing) {
			//Forwarded: the original's kHide handler is what hands scene rendering back.
			//Reporting it handled unwinds the stack without teardown, leaving the screen black.
			//It also starts StatsMenu's closing transition,
			//which the messages forwarded below carry through while the replacement is up.
			s_dismissing = false;
			s_statsMenuClosing = true;
			return s_processMessage(a_this, a_message);
		}
		if (s_statsMenuClosing) {
			//StatsMenu finishing the close it began at the dismissal. The replacement stays up.
			return s_processMessage(a_this, a_message);
		}
		//Any other close, such as the engine closing all menus, also hides the Stats view.
		//A no-op when the vanilla screen is the one open. The perk tree is an engine menu that the engine closes itself.
		Stats::StatsView::Hide();
		return s_processMessage(a_this, a_message);

	default:
		//Before the dismissal arrives, the instance has no initialized 3D or movie state,
		//so update and input messages are consumed. Afterwards they drive its close.
		return s_dismissing
			? RE::UI_MESSAGE_RESULTS::kHandled
			: s_processMessage(a_this, a_message);
	}
}

void StatsMenuHook::PostDisplay(RE::StatsMenu* a_this) {
	//Before the dismissal, the skydome, stars and camera nodes this pass renders do not exist.
	if (s_dismissing) {
		return;
	}
	s_postDisplay(a_this);
}
