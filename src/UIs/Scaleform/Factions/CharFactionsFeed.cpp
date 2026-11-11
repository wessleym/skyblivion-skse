#include "CharFactionsFeed.h"
#include "CharFactionsContract.h"
#include "FactionsContract.h"
#include "Papyrus/ModEvents.h"
#include "Platform/FormCache.h"
#include "Platform/MainThread.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

namespace Factions {

	namespace {
		const char* NameOf(const RE::TESFaction* faction) {
			const char* name = faction->GetFullName();
			return name ? name : "";
		}

		//The player's rank: runtime changes first (joins and promotions live in ExtraFactionChanges),
		//then the actor base record. -1 when neither lists the faction.
		int PlayerFactionRank(RE::PlayerCharacter* player, const RE::TESFaction* faction) {
			if (auto* changes = player->extraList.GetByType<RE::ExtraFactionChanges>()) {
				for (const auto& change : changes->factionChanges) {
					if (change.faction == faction) {
						return change.rank;
					}
				}
			}
			if (auto* base = player->GetActorBase()) {
				for (const auto& membership : base->factions) {
					if (membership.faction == faction) {
						return membership.rank;
					}
				}
			}
			return -1;
		}

		//Read live from the FACT rank block, so upstream faction edits show without a rebuild.
		const char* RankTitle(RE::TESFaction* faction, int rank, bool female) {
			if (rank < 0) {
				return "";
			}
			int index = 0;
			for (auto* rankData : faction->rankData) {
				if (index++ == rank && rankData) {
					const auto& title = female ? rankData->femaleRankTitle : rankData->maleRankTitle;
					return title.c_str() ? title.c_str() : "";
				}
			}
			return "";
		}
	}

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

		//The factions of SKYBUIFactionslist_FactionUIFL the player belongs to, sorted by display name.
		std::vector<RE::TESFaction*> factions;
		auto* player = RE::PlayerCharacter::GetSingleton();
		//Resolved once, at data load (the form table logs a miss once).
		auto* list = FormCache::Get<RE::BGSListForm>(FactionsContract::FactionListEditorID, "CharFactionsFeed");
		if (player && list) {
			for (auto* form : list->forms) {
				auto* faction = form ? form->As<RE::TESFaction>() : nullptr;
				if (faction && player->IsInFaction(faction)) {
					factions.push_back(faction);
				}
			}
		}
		std::sort(factions.begin(), factions.end(), [](const RE::TESFaction* a, const RE::TESFaction* b) {
			return std::strcmp(NameOf(a), NameOf(b)) < 0;
		});

		const auto* base = player ? player->GetActorBase() : nullptr;
		const bool  female = base && base->GetSex() == RE::SEX::kFemale;
		RE::GFxValue rows;
		movie->CreateArray(&rows);
		for (auto* faction : factions) {
			const bool expelled = (faction->data.flags & RE::FACTION_DATA::Flag::kPlayerIsExpelled) != 0;
			RE::GFxValue row;
			movie->CreateObject(&row);
			row.SetMember("name", RE::GFxValue(NameOf(faction)));
			row.SetMember("rank", RE::GFxValue(expelled ? "" : RankTitle(faction, PlayerFactionRank(player, faction), female)));
			row.SetMember("expelled", RE::GFxValue(expelled));
			rows.PushBack(row);
		}
		callback->Invoke(rows);

		Log::INFO("CharFactionsFeed: {} membership(s) -> {} in {} us.", rows.GetArraySize(), callback->target, ScaleformUtility::MicrosecondsSince(start));
	}

	RE::BSEventNotifyControl CharFactionsFeed::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		//On the main thread, next frame (see SkillSheetFeed).
		if (a_event && a_event->eventName == CharFactionsContract::ModEvent::RequestFactions) {
			MainThread::Post([request = ModEvents::StrArg(*a_event)] { PushFactions(request); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
