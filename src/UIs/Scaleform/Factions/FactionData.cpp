#include "FactionData.h"
#include "FactionsContract.h"

#include <algorithm>
#include <cstring>

namespace Factions::FactionData {

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

	std::optional<std::vector<Standing>> ReadAll(RE::PlayerCharacter* player, std::string_view source) {
		auto* list = RE::TESForm::LookupByEditorID<RE::BGSListForm>(FactionsContract::FactionListEditorID);
		if (!list) {
			Log::WARN("{}: {} not resolved. Is po3 Tweaks installed?", source, FactionsContract::FactionListEditorID);
			return std::nullopt;
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

		std::vector<Standing> standings;
		standings.reserve(factions.size());
		for (auto* faction : factions) {
			const bool member = player->IsInFaction(faction);
			const bool expelled = member && (faction->data.flags & RE::FACTION_DATA::Flag::kPlayerIsExpelled) != 0;
			standings.push_back(Standing{
				faction,
				NameOf(faction),
				member,
				expelled,
				(member && !expelled) ? RankTitle(faction, PlayerFactionRank(player, faction), female) : ""
			});
		}
		return standings;
	}

}
