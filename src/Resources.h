#pragma once

class Resources {
public:
	static constexpr std::string_view pluginName = "Skyblivion.esm";

	static inline RE::TESObjectMISC* gold = nullptr;
	static inline RE::BGSListForm* perkTreeList = nullptr;
	static inline RE::BGSEquipSlot* leftHandSlot = nullptr;
	static inline RE::BGSEquipSlot* rightHandSlot = nullptr;
	static inline RE::BGSEquipSlot* eitherHandSlot = nullptr;
	// Arcane Blacksmith (Skyrim.esm 05218E): Degradation's repair menu lets its holder repair enchanted gear.
	static inline RE::BGSPerk* arcaneBlacksmith = nullptr;
	// SKYBMastery<Heavy|Light>Armor<Apprentice|Journeyman|Expert|Master> (Skyblivion.esm), granted by
	// SKYBMasteryPerkQuest at skill 25 / 50 / 75 / 100: Degradation slows wear on that armor class per rank held.
	// Optional: a missing record counts as a rank not held.
	static inline std::array<RE::BGSPerk*, 4> heavyArmorMastery{};
	static inline std::array<RE::BGSPerk*, 4> lightArmorMastery{};

	static void Install();

	// The player's gold, as an item count of Gold001 (0000000F). Not NG's Actor::GetGoldAmount(): it finds gold
	// through BGSDefaultObjectManager, whose "initialized" flags NG reads at +0xB80 while AE 1.6.1170 keeps them at
	// +0xB90 (366 default objects, not 364), so on 1.6.1170 it can see gold as unset and dereference null.
	[[nodiscard]] static std::int32_t PlayerGold();
};
