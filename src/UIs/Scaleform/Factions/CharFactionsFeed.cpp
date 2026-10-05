#include "CharFactionsFeed.h"
#include "CharFactionsContract.h"
#include "FactionData.h"
#include "Papyrus/ModEvents.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <chrono>

namespace Factions {

	void CharFactionsFeed::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("CharFactionsFeed: No mod-event source. Character sheet faction requests will not be heard.");
			return;
		}
		Log::INFO("CharFactionsFeed: Registered.");
	}

	void CharFactionsFeed::PushFactions(const std::string& request) {
		const auto start = std::chrono::steady_clock::now();

		const auto callback = MenuCallback::Open(request, "CharFactionsFeed");
		if (!callback) {
			return;
		}
		auto* movie = callback->movie.get();

		RE::GFxValue rows;
		movie->CreateArray(&rows);
		auto* player = RE::PlayerCharacter::GetSingleton();
		const auto standings = player ? FactionData::ReadAll(player, "CharFactionsFeed") : std::nullopt;
		if (standings) {
			for (const auto& standing : *standings) {
				if (!standing.member) {
					continue;
				}
				RE::GFxValue row;
				movie->CreateObject(&row);
				row.SetMember("name", RE::GFxValue(standing.name));
				row.SetMember("rank", RE::GFxValue(standing.rankTitle));
				row.SetMember("expelled", RE::GFxValue(standing.expelled));
				rows.PushBack(row);
			}
		}
		callback->Invoke(rows);

		Log::INFO("CharFactionsFeed: {} membership(s) -> {} in {} us.", rows.GetArraySize(), callback->target, ScaleformUtility::MicrosecondsSince(start));
	}

	RE::BSEventNotifyControl CharFactionsFeed::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		//A UI task, not AddTask: the sheet is a custom menu (see SkillSheetFeed).
		if (a_event && a_event->eventName == CharFactionsContract::ModEvent::RequestFactions) {
			SKSE::GetTaskInterface()->AddUITask([request = ModEvents::StrArg(*a_event)] { PushFactions(request); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
