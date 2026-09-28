#include "SoulGems.h"

namespace Sigil {

	namespace {
		bool IsValidTier(int tier) {
			return tier >= static_cast<int>(RE::SOUL_LEVEL::kPetty) && tier <= static_cast<int>(RE::SOUL_LEVEL::kGrand);
		}

		bool IsEligibleBase(const RE::TESSoulGem* gem, RE::SOUL_LEVEL level) {
			return gem
				&& gem->GetMaximumCapacity() == level
				&& !gem->CanHoldNPCSoul()
				&& !gem->HasKeywordString("ReusableSoulGem");
		}

		//A record-filled base counts its whole stack. An empty base counts only the instances holding the soul.
		int CountEntryGems(const RE::TESSoulGem* gem, std::int32_t count, const RE::InventoryEntryData* entry, RE::SOUL_LEVEL level) {
			if (gem->GetContainedSoul() == level) {
				return count;
			}
			if (gem->GetContainedSoul() != RE::SOUL_LEVEL::kNone || !entry || !entry->extraLists) {
				return 0;
			}
			int instances = 0;
			for (auto* extraList : *entry->extraLists) {
				if (extraList && extraList->GetSoulLevel() == level) {
					instances += extraList->GetCount();
				}
			}
			return instances;
		}

		RE::TESObjectREFR::InventoryItemMap CarriedSoulGems(RE::PlayerCharacter* player) {
			return player->GetInventory([](RE::TESBoundObject& object) { return object.IsSoulGem(); });
		}
	}

	int SoulGems::CountTierGems(RE::PlayerCharacter* player, int tier) {
		if (!player || !IsValidTier(tier)) {
			return 0;
		}
		const auto level = static_cast<RE::SOUL_LEVEL>(tier);
		int total = 0;
		for (const auto& [object, countAndEntry] : CarriedSoulGems(player)) {
			const auto* gem = object->As<RE::TESSoulGem>();
			if (countAndEntry.first > 0 && IsEligibleBase(gem, level)) {
				total += CountEntryGems(gem, countAndEntry.first, countAndEntry.second.get(), level);
			}
		}
		return total;
	}

	bool SoulGems::ConsumeTierGem(RE::PlayerCharacter* player, int tier) {
		if (!player || !IsValidTier(tier)) {
			return false;
		}
		const auto level = static_cast<RE::SOUL_LEVEL>(tier);
		const auto inventory = CarriedSoulGems(player);

		for (const auto& [object, countAndEntry] : inventory) {
			const auto* gem = object->As<RE::TESSoulGem>();
			if (countAndEntry.first > 0 && IsEligibleBase(gem, level) && gem->GetContainedSoul() == level) {
				player->RemoveItem(object, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
				return true;
			}
		}

		//The extra data list comes from a GetInventory entry copy, which is assumed to share the live list pointers.
		//Player-filled gems are consumed correctly in game, but InventoryEntryData's copy semantics are not confirmed.
		for (const auto& [object, countAndEntry] : inventory) {
			const auto* gem = object->As<RE::TESSoulGem>();
			if (countAndEntry.first < 1 || !IsEligibleBase(gem, level) || gem->GetContainedSoul() != RE::SOUL_LEVEL::kNone ||
				!countAndEntry.second || !countAndEntry.second->extraLists) {
				continue;
			}
			for (auto* extraList : *countAndEntry.second->extraLists) {
				if (extraList && extraList->GetSoulLevel() == level) {
					player->RemoveItem(object, 1, RE::ITEM_REMOVE_REASON::kRemove, extraList, nullptr);
					return true;
				}
			}
		}
		return false;
	}

}
