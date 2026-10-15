#pragma once

struct Resources
{
	constexpr static std::string_view pluginName = "Skyblivion.esm";

	//<NEB> As a general rule one should A, always cache a form, and B, prefer using forms over form ids.
	// form ids take time to search even if it's minor
	inline static RE::TESObjectMISC*	gold = nullptr;
	inline static RE::BGSListForm*		perkTreeList = nullptr;
	inline static RE::BGSEquipSlot*		leftHandSlot = nullptr;
	inline static RE::BGSEquipSlot*		rightHandSlot = nullptr;
	inline static RE::BGSEquipSlot*		eitherHandSlot = nullptr;
	inline static RE::BGSSoundCategory* audioCategoryUI = nullptr;


	static void Install();
};
