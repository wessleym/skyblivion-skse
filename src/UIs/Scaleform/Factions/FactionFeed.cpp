#include "FactionFeed.h"
#include "FactionData.h"
#include "FactionsContract.h"
#include "Papyrus/ModEvents.h"
#include "Platform/MainThread.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <chrono>

namespace Factions {

	void FactionFeed::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("FactionFeed: No mod-event source. Faction page requests will not be heard.");
			return;
		}
		Log::INFO("FactionFeed: Registered.");
	}

	void FactionFeed::PushFactions(const std::string& request) {
		const auto start = std::chrono::steady_clock::now();

		const auto feed = FeedRequest::Open(request, FeedRequest::Mode::Ignored, "FactionFeed");
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!feed || !player) {
			return;
		}
		const auto standings = FactionData::ReadAll(player, "FactionFeed");
		if (!standings) {
			return;
		}
		auto* movie = feed->movie.get();

		RE::GFxValue rows;
		movie->CreateArray(&rows);
		int memberships = 0;
		for (const auto& standing : *standings) {
			if (standing.member) {
				++memberships;
			}

			RE::GFxValue row;
			movie->CreateObject(&row);
			row.SetMember("isInFaction", RE::GFxValue(standing.member));
			row.SetMember("isExpelled", RE::GFxValue(standing.expelled));
			row.SetMember("factionName", RE::GFxValue(standing.name));
			row.SetMember("rankName", RE::GFxValue(standing.rankTitle));
			rows.PushBack(row);
		}

		feed->Push(rows);

		Log::INFO("FactionFeed: {} faction(s), {} membership(s) -> {} in {} us.",
			standings->size(), memberships, feed->target, ScaleformUtility::MicrosecondsSince(start));
	}

	RE::BSEventNotifyControl FactionFeed::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (a_event && a_event->eventName == FactionsContract::ModEvent::RequestFactions) {
			MainThread::Post([request = ModEvents::StrArg(*a_event)] { PushFactions(request); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
