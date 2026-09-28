#include "SigilEnchanting.h"

namespace Sigil {

	namespace {
		//How far under the player the instance is dropped: far enough that its 3D is never seen.
		constexpr float kDropDepth = 2000.0f;

		//The total carried, from the merged base-container and changes view.
		//Items untouched since arriving through a base container (starting gear never equipped)
		//have no InventoryEntryData in the changes at all.
		std::int32_t CarriedCount(RE::PlayerCharacter* player, RE::TESBoundObject* gear) {
			auto inventory = player->GetInventory([gear](RE::TESBoundObject& object) { return &object == gear; });
			const auto found = inventory.find(gear);
			return found != inventory.end() ? found->second.first : 0;
		}

		//The live changes entry, when one exists. Needed only for the special-instance census.
		RE::InventoryEntryData* LiveEntry(RE::PlayerCharacter* player, RE::TESBoundObject* gear) {
			auto* changes = player->GetInventoryChanges();
			if (!changes || !changes->entryList) {
				return nullptr;
			}
			for (auto* entry : *changes->entryList) {
				if (entry && entry->object == gear) {
					return entry;
				}
			}
			return nullptr;
		}

		struct Census {
			//Copies with no extra list, including pure base-container copies with no entry.
			std::int32_t plainCount = 0;
			//An unenchanted special instance, preferring one that is not worn. Null when there is none.
			RE::ExtraDataList* clean = nullptr;
		};

		Census TakeCensus(RE::InventoryEntryData* entry, std::int32_t total) {
			Census census;
			std::int32_t specialCount = 0;
			bool cleanWorn = false;
			if (entry && entry->extraLists) {
				for (auto* extraList : *entry->extraLists) {
					if (!extraList) {
						continue;
					}
					specialCount += extraList->GetCount();
					if (extraList->HasType(RE::ExtraDataType::kEnchantment)) {
						continue;
					}
					const bool worn = extraList->HasType(RE::ExtraDataType::kWorn) || extraList->HasType(RE::ExtraDataType::kWornLeft);
					if (!census.clean || (cleanWorn && !worn)) {
						census.clean = extraList;
						cleanWorn = worn;
					}
				}
			}
			census.plainCount = total - specialCount;
			return census;
		}
	}

	SigilEnchanting::Result SigilEnchanting::Apply(RE::PlayerCharacter* player, RE::FormID gearID, RE::FormID enchantmentID, std::uint16_t charge, const std::string& name) {
		auto* gear = RE::TESForm::LookupByID<RE::TESBoundObject>(gearID);
		auto* enchantment = RE::TESForm::LookupByID<RE::EnchantmentItem>(enchantmentID);
		if (!player || !gear || !enchantment) {
			Log::WARN("SigilEnchanting: Gear {:08X} or enchantment {:08X} did not resolve.", gearID, enchantmentID);
			return Result::Missing;
		}

		const std::int32_t total = CarriedCount(player, gear);
		if (total <= 0) {
			Log::WARN("SigilEnchanting: Gear {:08X} not found in the inventory.", gearID);
			return Result::Missing;
		}

		const auto census = TakeCensus(LiveEntry(player, gear), total);
		if (census.plainCount <= 0 && !census.clean) {
			Log::INFO("SigilEnchanting: Every copy of {:08X} is already enchanted.", gearID);
			return Result::AlreadyEnchanted;
		}

		//A null list lets the engine take a plain copy first, as the vanilla stack rule does.
		RE::ExtraDataList* dropList = census.plainCount > 0 ? nullptr : census.clean;
		RE::NiPoint3 dropLocation = player->GetPosition();
		dropLocation.z -= kDropDepth;
		auto handle = player->RemoveItem(gear, 1, RE::ITEM_REMOVE_REASON::kDropping, dropList, nullptr, &dropLocation);
		auto reference = handle.get();
		if (!reference) {
			Log::WARN("SigilEnchanting: The drop of {:08X} returned no reference.", gearID);
			return Result::Missing;
		}

		//Guard: the census chose an unenchanted instance, but the dropped reference carries an enchantment. It is picked back up.
		if (reference->extraList.HasType(RE::ExtraDataType::kEnchantment)) {
			player->PickUpObject(reference.get(), 1, false, false);
			Log::WARN("SigilEnchanting: The dropped instance of {:08X} was already enchanted. Returned.", gearID);
			return Result::AlreadyEnchanted;
		}

		reference->extraList.Add(new RE::ExtraEnchantment(enchantment, charge));
		if (!name.empty()) {
			if (auto* textDisplayData = reference->extraList.GetByType<RE::ExtraTextDisplayData>()) {
				textDisplayData->SetName(name.c_str());
			}
			else {
				reference->extraList.Add(new RE::ExtraTextDisplayData(name.c_str()));
			}
		}
		player->PickUpObject(reference.get(), 1, false, false);
		Log::INFO("SigilEnchanting: Enchanted {:08X} through a drop round trip ({} instance, charge {}).",
			gearID, dropList ? "special" : "plain", charge);
		return Result::Enchanted;
	}

}
