#include "PerkAcquisition.h"
#include "PerkIndex.h"

#include <string>

namespace Stats {

	bool PerkAcquisition::Acquire(const PerkReference& a_perk) {
		const auto described = a_perk.Describe();

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			Log::WARN("PerkAcquisition: Player unavailable.");
			return false;
		}

		auto* perk = PerkIndex::Resolve(a_perk);
		if (!perk) {
			Log::WARN("PerkAcquisition: {} does not name a perk in any loaded plugin. Not granted.", described);
			return false;
		}
		if (player->HasPerk(perk)) {
			Log::INFO("PerkAcquisition: The player already has {}. Not granted.", described);
			return false;
		}

		auto& gameStats = REBridge::GameStats(player);
		if (gameStats.perkCount == 0) {
			Log::INFO("PerkAcquisition: No perk points available. {} not granted.", described);
			return false;
		}

		player->AddPerk(perk);
		--gameStats.perkCount;
		Log::INFO("PerkAcquisition: Granted {}, {} perk point(s) left.", described, gameStats.perkCount);
		return true;
	}
}
