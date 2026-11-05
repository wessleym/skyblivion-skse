#include "CharRaceFeed.h"
#include "Papyrus/ModEvents.h"
#include "Platform/MainThread.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <chrono>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace CharacterSheet {

	namespace {
		constexpr const char* kRequest = "SKYBCharRaceRequest";
		constexpr const char* kFile = "Data/Interface/Skyblivion/ConfigFiles/Races/SKYBRaceDetails.json";

		//A JSON value as a Scaleform value: objects and arrays built in the movie, strings, numbers, booleans,
		//null as null.
		RE::GFxValue ToGFx(RE::GFxMovieView& movie, const nlohmann::json& j) {
			RE::GFxValue out;
			if (j.is_object()) {
				movie.CreateObject(&out);
				for (const auto& [key, value] : j.items()) {
					out.SetMember(key.c_str(), ToGFx(movie, value));
				}
			} else if (j.is_array()) {
				movie.CreateArray(&out);
				for (const auto& value : j) {
					out.PushBack(ToGFx(movie, value));
				}
			} else if (j.is_string()) {
				//the movie keeps its own copy of the text
				movie.CreateString(&out, j.get_ref<const std::string&>().c_str());
			} else if (j.is_boolean()) {
				out.SetBoolean(j.get<bool>());
			} else if (j.is_number()) {
				out.SetNumber(j.get<double>());
			} else {
				out.SetNull();
			}
			return out;
		}
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
		std::ifstream file(kFile, std::ios::binary);
		if (!file) {
			Log::WARN("CharRaceFeed: {} not found.", kFile);
			movie->CreateArray(&races);
		} else {
			std::stringstream text;
			text << file.rdbuf();
			const auto doc = nlohmann::json::parse(text.str(), nullptr, false);
			if (doc.is_discarded() || !doc.is_object() || !doc.contains("races") || !doc["races"].is_array()) {
				Log::WARN("CharRaceFeed: {} is not valid JSON with a \"races\" array.", kFile);
				movie->CreateArray(&races);
			} else {
				races = ToGFx(*movie, doc["races"]);
			}
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
