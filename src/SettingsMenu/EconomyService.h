#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
// Progression > Economy rows of the settings menu.
//
//   Gold.Loot      every Gold001 entry of a loot leveled list (vanilla lists
//                  included) = its data-load count x the row
//   Gold.Merchant  the same for merchant lists (SKYBLLVendorGold*, VendorGold*)
//                  Lists are classified by editor ID (loaded by po3 Tweaks);
//                  investment and quest / perk reward lists are left alone.
//   Price.Buy/Sell the ModBuyPrices / ModSellPrices Multiply values of
//                  SKYBEconomyPerk, given to the player on every game load.
//   chance-none    SetChanceNone scales the drop chance (100 - value) of
//                  leveled-list chance-none globals from their data-load values.
// Leveled lists roll when a container or body is generated, so the rows reach loot
// not rolled yet, and merchant gold as merchants restock.
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
