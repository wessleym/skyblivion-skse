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
	arcaneBlacksmith = RE::TESForm::LookupByID<RE::BGSPerk>(0x0005218E);//ArcaneBlacksmith (Skyrim.esm). Optional: without it the Smithing gate alone applies.
	constexpr std::array<RE::FormID, 4> heavyMasteryIds{ 0x13E122, 0x20BA00, 0x20BA44, 0x20BA76 };
	constexpr std::array<RE::FormID, 4> lightMasteryIds{ 0x13E123, 0x20BA01, 0x20BA4B, 0x20BA77 };
	for (std::size_t i = 0; i < heavyMasteryIds.size(); ++i) {
		heavyArmorMastery[i] = dataHandler->LookupForm<RE::BGSPerk>(heavyMasteryIds[i], pluginName);
		lightArmorMastery[i] = dataHandler->LookupForm<RE::BGSPerk>(lightMasteryIds[i], pluginName);
	}

	assert(gold);
	assert(leftHandSlot);
	assert(rightHandSlot);
	assert(eitherHandSlot);
}

std::int32_t Resources::PlayerGold() {
	auto* player = RE::PlayerCharacter::GetSingleton();
	return player && gold ? std::max(0, player->GetItemCount(gold)) : 0;
}
