#pragma once
// Moved from SKYBUIDemo.dll into Skyblivion.dll, 2026-10-05 (fold-in, UI integration workspace
// ledger L-0056). Source of record until then: game\native\SKYBUIDemo\src in that workspace.
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
// Progression › Economy rows of the settings menu.
//
//   Gold.Loot      every Gold001 entry of a loot leveled list (containers,
//                  urns, pockets, bodies - vanilla lists included, some loot
//                  gold runs through them) = its data-load count x the row
//   Gold.Merchant  every Gold001 entry of a merchant list (SKYBLLVendorGold*,
//                  vanilla VendorGold*) the same way
//                  Lists whose editor ID names investments, quest rewards
//                  or perk rewards (Invest, Reward, PerkMasterTrader, GoldBonus,
//                  BelovedLaborer, MasterGold) are left alone. Classified
//                  by editor ID (po3 Tweaks loads them); the log lists the
//                  counts per class.
//   Price.Buy/Sell SKYBEconomy.esp PERK 000800 SKYBEconomyPerk (the player's):
//                  its ModBuyPrices / ModSellPrices Multiply values. The
//                  player is given the perk on every game load if missing.
//   chance-none    SetChanceNone scales the DROP chance (100 - value) of
//                  leveled-list chance-none globals (enchanted gear gate,
//                  the SKYBLootChance* rarity dials) from their data-load
//                  (Skyblivion.esm) values.
// Leveled lists roll when a container / body is generated, so gold and loot
// rows reach loot not rolled yet; merchant gold as merchants restock.
class EconomyService {
public:
	static EconomyService* GetSingleton();
	void Register();     // kDataLoaded
	void OnGameReady();  // give the player the price perk

	void Set(std::string_view a_section, std::string_view a_key, float a_value);  // Gold.Loot | Gold.Merchant | Price.Buy | Price.Sell
	void SetChanceNone(std::string_view a_globals, float a_mult);                  // space-separated editor IDs

private:
	EconomyService() = default;
	struct GoldEntry {
		RE::LEVELED_OBJECT* object;
		std::uint16_t       base;
	};
	static void ScaleGold(std::vector<GoldEntry>& a_entries, float a_mult);
	void        WritePrices();

	std::vector<GoldEntry>                  _loot;
	std::vector<GoldEntry>                  _merchant;
	std::unordered_map<std::string, float>  _chanceBase;  // global editor ID -> data-load value
	RE::BGSPerk*                            _perk = nullptr;
	RE::BGSEntryPointFunctionDataOneValue*  _buy = nullptr;
	RE::BGSEntryPointFunctionDataOneValue*  _sell = nullptr;
	float                                   _buyMult = 1.0f;
	float                                   _sellMult = 1.0f;
};
