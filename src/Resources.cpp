#include "Resources.h"

void Resources::Install() {
	auto* dataHandler = RE::TESDataHandler::GetSingleton();
	assert(dataHandler);

	gold = RE::TESForm::LookupByID<RE::TESObjectMISC>(0xF);
	//SKYBPerkTrees, the master FormID list naming the eight SKYBPerks<Attribute> lists.
	perkTreeList = dataHandler->LookupForm<RE::BGSListForm>(0x1254F2, pluginName);//Optional. PerkListValidator handles its absence.
	rightHandSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x00013F42);//RightHand
	leftHandSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x00013F43);//LeftHand
	eitherHandSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x00013F44);//EitherHand

	assert(gold);
	assert(leftHandSlot);
	assert(rightHandSlot);
	assert(eitherHandSlot);
}
