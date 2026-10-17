#pragma once

class Resources {
public:
	static constexpr std::string_view pluginName = "Skyblivion.esm";

	static inline RE::TESObjectMISC* gold = nullptr;
	static inline RE::BGSListForm* perkTreeList = nullptr;
	static inline RE::BGSEquipSlot* leftHandSlot = nullptr;
	static inline RE::BGSEquipSlot* rightHandSlot = nullptr;
	static inline RE::BGSEquipSlot* eitherHandSlot = nullptr;

	static void Install();
};
