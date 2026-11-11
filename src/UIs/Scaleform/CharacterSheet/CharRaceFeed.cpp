#include "CharRaceFeed.h"
#include "Papyrus/ModEvents.h"
#include "Platform/JsonFile.h"
#include "Platform/MainThread.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <chrono>

namespace CharacterSheet {

	namespace {
		constexpr const char* kRequest = "SKYBCharRaceRequest";
		constexpr const char* kFile = "Data/Interface/Skyblivion/ConfigFiles/Races/SKYBRaceDetails.json";
	}

	void CharRaceFeed::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("CharRaceFeed: No mod-event source. The character sheet's race page will not get its data.");
			return;
		}
		Log::INFO("CharRaceFeed: Registered.");
	}

	void CharRaceFeed::PushRaces(const std::string& request) {
		const auto start = std::chrono::steady_clock::now();
		const auto callback = MenuCallback::Open(request, "CharRaceFeed");
		if (!callback) {
			return;
		}
		auto* movie = callback->movie.get();
		RE::GFxValue races;
		const auto doc = JsonFile::ReadObject(kFile, "CharRaceFeed");
		const auto list = doc ? doc->find("races") : nlohmann::json::const_iterator{};
		if (doc && list != doc->end() && list->is_array()) {
			races = ScaleformUtility::ToGFx(*movie, *list);
		} else {
			if (doc) {
				Log::WARN("CharRaceFeed: {} has no \"races\" array.", kFile);
			}
			movie->CreateArray(&races);
		}
		callback->Invoke(races);
		Log::INFO("CharRaceFeed: {} race(s) -> {} in {} us.", races.GetArraySize(), callback->target,
			ScaleformUtility::MicrosecondsSince(start));
	}

	RE::BSEventNotifyControl CharRaceFeed::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		//On the main thread, next frame (as CharFactionsFeed).
		if (a_event && a_event->eventName == kRequest) {
			MainThread::Post([request = ModEvents::StrArg(*a_event)] { PushRaces(request); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
