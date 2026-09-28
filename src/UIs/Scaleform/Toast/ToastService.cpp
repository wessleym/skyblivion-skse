#include "ToastService.h"
#include "ToastContract.h"
#include "Papyrus/ModEvents.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <string_view>

namespace Toast {

	namespace {
		//The HUD menu is not checked with IsMenuOpen: it stays registered while hidden,
		//and the missing movie is what marks the main menu and load screens.
		RE::GPtr<RE::GFxMovieView> HudMovie() {
			auto* ui = RE::UI::GetSingleton();
			if (!ui) {
				return nullptr;
			}
			auto menu = ui->GetMenu(RE::HUDMenu::MENU_NAME);
			return menu ? menu->uiMovie : nullptr;
		}

		bool IsClipReady(RE::GFxMovieView& movie) {
			RE::GFxValue ready;
			return movie.GetVariable(&ready, ToastContract::AsPath::ReadyVariable) && ready.IsBool() && ready.GetBool();
		}

		//Creating the clip again replaces it, so a repeated injection is harmless.
		void InjectClip(RE::GFxMovieView& movie) {
			RE::GFxValue createArgs[2];
			createArgs[0] = ToastContract::Movie::ClipName;
			createArgs[1] = ToastContract::Movie::ClipDepth;
			movie.Invoke(ToastContract::AsPath::CreateClip, nullptr, createArgs, 2);
			RE::GFxValue path(ToastContract::Movie::Path);
			movie.Invoke(ToastContract::AsPath::LoadMovie, nullptr, &path, 1);
			Log::INFO("ToastService: Toast clip injected into the HUD movie.");
		}
	}

	void ToastService::Register() {
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(&s_sink);
		}
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("ToastService: No mod-event source. Toast requests will not be heard.");
			return;
		}
		Log::INFO("ToastService: Registered.");
	}

	void ToastService::Show(std::string header, std::string title, std::string desc, std::string icon) {
		{
			std::scoped_lock lock(s_lock);
			s_pending.push_back({ std::move(header), std::move(title), std::move(desc), std::move(icon) });
		}
		if (TryBeginFlushChain()) {
			EnsureInjected();
			FlushWhenReady();
		}
	}

	bool ToastService::TryBeginFlushChain() {
		std::scoped_lock lock(s_lock);
		if (s_pending.empty() || s_flushChainActive) {
			return false;
		}
		s_flushChainActive = true;
		return true;
	}

	void ToastService::EnsureInjected() {
		SKSE::GetTaskInterface()->AddUITask([] {
			auto movie = HudMovie();
			if (movie && !IsClipReady(*movie)) {
				InjectClip(*movie);
			}
		});
	}

	void ToastService::FlushWhenReady() {
		ScaleformUtility::RetryOnUIThread(kReadyAttempts, kReadyInterval, DeliverIfReady, [] {
			std::scoped_lock lock(s_lock);
			Log::WARN("ToastService: The toast movie never reported ready. Dropping {} held toast(s).", s_pending.size());
			s_pending.clear();
			s_flushChainActive = false;
		});
	}

	bool ToastService::DeliverIfReady() {
		auto movie = HudMovie();
		if (!movie) {
			std::scoped_lock lock(s_lock);
			s_flushChainActive = false;
			return true;
		}
		if (!IsClipReady(*movie)) {
			return false;
		}

		std::vector<ToastFields> batch;
		{
			std::scoped_lock lock(s_lock);
			batch.swap(s_pending);
			s_flushChainActive = false;
		}
		for (const auto& toast : batch) {
			RE::GFxValue args[4];
			for (std::size_t i = 0; i < toast.size(); ++i) {
				args[i] = toast[i].c_str();
			}
			movie->Invoke(ToastContract::AsPath::ShowAchievement, nullptr, args, 4);
		}
		if (!batch.empty()) {
			Log::INFO("ToastService: Delivered {} toast(s).", batch.size());
		}
		return true;
	}

	RE::BSEventNotifyControl ToastService::Sink::ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
		RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		//The HUD opening covers game start. A load screen closing covers the HUD movie losing its clips.
		const bool hudOpened = a_event->opening && a_event->menuName == RE::HUDMenu::MENU_NAME;
		const bool loadingClosed = !a_event->opening && a_event->menuName == RE::LoadingMenu::MENU_NAME;
		if (hudOpened || loadingClosed) {
			EnsureInjected();
			if (TryBeginFlushChain()) {
				FlushWhenReady();
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl ToastService::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (!a_event || a_event->eventName != ToastContract::ModEvent::ShowToast) {
			return RE::BSEventNotifyControl::kContinue;
		}
		ToastFields fields;
		const auto parts = ModEvents::Split(ModEvents::StrArg(*a_event), '|');
		for (std::size_t i = 0; i < fields.size() && i < parts.size(); ++i) {
			fields[i] = parts[i];
		}
		Log::INFO("ToastService: {}: '{}' / '{}'", ToastContract::ModEvent::ShowToast, fields[0], fields[1]);
		Show(std::move(fields[0]), std::move(fields[1]), std::move(fields[2]), std::move(fields[3]));
		return RE::BSEventNotifyControl::kContinue;
	}

}
