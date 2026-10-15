#include "Resources.h"


void Resources::Install()
{
	auto data_handler = RE::TESDataHandler::GetSingleton();

	assert(data_handler);

	gold = RE::TESForm::LookupByID<RE::TESObjectMISC>(0xF);
	perkTreeList = data_handler->LookupForm<RE::BGSListForm>(0x1254F2, pluginName);
	leftHandSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x00013F42);
	rightHandSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x00013F43);
	eitherHandSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(0x00013F44);
	audioCategoryUI = RE::TESForm::LookupByID<RE::BGSSoundCategory>(0x064451);
	
	assert(gold);
	assert(perkTreeList);
	assert(leftHandSlot);
	assert(rightHandSlot);
	assert(eitherHandSlot);
	assert(audioCategoryUI);
}
