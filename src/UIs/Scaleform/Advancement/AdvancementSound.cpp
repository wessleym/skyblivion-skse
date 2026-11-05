#include "AdvancementSound.h"
#include "Platform/FormCache.h"
#include "Papyrus/ModEvents.h"

#include <mutex>

namespace Advancement {

	namespace {
		//The level-up screen's music (Skyblivion.esm SKYBUILevelUpSD) and the menu cancel sound (Skyrim.esm
		//UIMenuCancelSD). Descriptors, not markers: the game keeps no editor IDs for descriptors, so by form ID.
		constexpr RE::FormID kMusic = 0x210F20;
		constexpr RE::FormID kCancel = 0x03C752;
		constexpr std::uint16_t kFadeMs = 600;

		//Set on Ready, cleared on the close. The events come from the movie's thread and the UI's.
		std::mutex g_lock;
		RE::BSSoundHandle g_music;
		bool g_open = false;
		bool g_chosen = false;

		void Play(RE::BSSoundHandle& handle, RE::FormID localID, std::string_view plugin, std::string_view what) {
			auto* audio = RE::BSAudioManager::GetSingleton();
			auto* form = FormCache::GetLocal<RE::BGSSoundDescriptorForm>(localID, plugin, "AdvancementSound");
			if (!audio || !form) {
				return;
			}
			if (!audio->GetSoundHandle(handle, static_cast<RE::BSISoundDescriptor*>(form)) || !handle.IsValid()) {
				Log::INFO("AdvancementSound: {} {:08X} could not be started", what, form->GetFormID());
				return;
			}
			handle.Play();
		}
	}

	void AdvancementSound::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("AdvancementSound: No mod-event source. The level-up screen will have no music.");
			return;
		}
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(&s_sink);
		}
		Log::INFO("AdvancementSound: Registered.");
	}

	void AdvancementSound::Start() {
		std::scoped_lock lock(g_lock);
		g_open = true;
		g_chosen = false;
		if (g_music.IsValid() && g_music.IsPlaying()) {
			return;
		}
		Play(g_music, kMusic, "Skyblivion.esm", "music");
	}

	void AdvancementSound::Stop(bool cancelled) {
		std::scoped_lock lock(g_lock);
		if (g_music.IsValid()) {
			g_music.FadeOutAndRelease(kFadeMs);
		}
		g_music = RE::BSSoundHandle{};
		if (cancelled) {
			RE::BSSoundHandle cancel;
			Play(cancel, kCancel, "Skyrim.esm", "cancel sound");
		}
	}

	RE::BSEventNotifyControl AdvancementSound::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		if (a_event->eventName == "SKYBAdvancementReady") {
			SKSE::GetTaskInterface()->AddTask([] { Start(); });
		} else if (a_event->eventName == "SKYBAdvancementChosen") {
			{
				std::scoped_lock lock(g_lock);
				g_chosen = true;
			}
			SKSE::GetTaskInterface()->AddTask([] { Stop(false); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl AdvancementSound::Sink::ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
		RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
		//Every custom menu is "CustomMenu"; only the one that sent Ready (g_open) is ours.
		if (!a_event || a_event->opening || a_event->menuName != "CustomMenu") {
			return RE::BSEventNotifyControl::kContinue;
		}
		bool cancelled = false;
		{
			std::scoped_lock lock(g_lock);
			if (!g_open) {
				return RE::BSEventNotifyControl::kContinue;
			}
			g_open = false;
			cancelled = !g_chosen;
		}
		SKSE::GetTaskInterface()->AddTask([cancelled] { Stop(cancelled); });
		return RE::BSEventNotifyControl::kContinue;
	}

}
