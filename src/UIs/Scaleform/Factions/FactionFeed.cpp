#include "FactionFeed.h"
#include "FactionsContract.h"
#include "Papyrus/ModEvents.h"
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
		auto* movie = feed->movie.get();

		auto* list = RE::TESForm::LookupByEditorID<RE::BGSListForm>(FactionsContract::FactionListEditorID);
		if (!list) {
			Log::WARN("FactionFeed: {} not resolved. Is po3 Tweaks installed?", FactionsContract::FactionListEditorID);
			return;
		}

		const auto* base = player->GetActorBase();
		const bool female = base && base->GetSex() == RE::SEX::kFemale;

		std::vector<RE::TESFaction*> factions;
		for (auto* form : list->forms) {
			if (auto* faction = form ? form->As<RE::TESFaction>() : nullptr) {
				factions.push_back(faction);
			}
		}
		std::sort(factions.begin(), factions.end(), [](const RE::TESFaction* a, const RE::TESFaction* b) {
			return std::strcmp(NameOf(a), NameOf(b)) < 0;
		});

		RE::GFxValue rows;
		movie->CreateArray(&rows);
		int memberships = 0;
		for (auto* faction : factions) {
			const bool inFaction = player->IsInFaction(faction);
			const bool expelled = inFaction && (faction->data.flags & RE::FACTION_DATA::Flag::kPlayerIsExpelled) != 0;
			const char* rankName = (inFaction && !expelled)
				? RankTitle(faction, PlayerFactionRank(player, faction), female)
				: "";
			if (inFaction) {
				++memberships;
			}

			RE::GFxValue row;
			movie->CreateObject(&row);
			row.SetMember("isInFaction", RE::GFxValue(inFaction));
			row.SetMember("isExpelled", RE::GFxValue(expelled));
			row.SetMember("factionName", RE::GFxValue(NameOf(faction)));
			row.SetMember("rankName", RE::GFxValue(rankName));
			rows.PushBack(row);
		}

		feed->Push(rows);

		Log::INFO("FactionFeed: {} faction(s), {} membership(s) -> {} in {} us.",
			factions.size(), memberships, feed->target, ScaleformUtility::MicrosecondsSince(start));
	}

	RE::BSEventNotifyControl FactionFeed::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (a_event && a_event->eventName == FactionsContract::ModEvent::RequestFactions) {
			SKSE::GetTaskInterface()->AddTask([request = ModEvents::StrArg(*a_event)] { PushFactions(request); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
