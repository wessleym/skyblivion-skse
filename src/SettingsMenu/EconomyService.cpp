#include "SettingsMenu/EconomyService.h"

#include "SettingsMenu/SettingsMenuTable.h"

namespace {
	constexpr RE::FormID       kGold001 = 0x0000000F;
	//by editor ID: the perk may ship in SKYBEconomy.esp, SKYBCombined.esp or the ESM
	constexpr const char*      kPerk = "SKYBEconomyPerk";

	bool StartsWith(std::string_view a, std::string_view b) { return a.substr(0, b.size()) == b; }
	bool Contains(std::string_view a, std::string_view b) { return a.find(b) != std::string_view::npos; }

	enum class GoldClass { kLoot, kMerchant, kSkip };

	GoldClass Classify(std::string_view a_edid) {
		if (StartsWith(a_edid, "SKYBLLVendorGold") || StartsWith(a_edid, "VendorGold")) {
			return GoldClass::kMerchant;
		}
		for (auto word : {"Invest", "Reward", "PerkMasterTrader", "BelovedLaborer", "MasterGold", "GoldBonus"}) {
			if (Contains(a_edid, word)) {
				return GoldClass::kSkip;
			}
		}
		return GoldClass::kLoot;
	}

	// Leveled lists keep no editor ID of their own; the engine's editor-ID map (filled by po3 Tweaks) has them.
	std::unordered_map<const RE::TESForm*, std::string> LeveledItemNames() {
		std::unordered_map<const RE::TESForm*, std::string> names;
		const auto& [map, lock] = RE::TESForm::GetAllFormsByEditorID();
		const RE::BSReadLockGuard guard{ lock };
		if (map) {
			for (const auto& [edid, form] : *map) {
				if (form && form->Is(RE::FormType::LeveledItem) && edid.c_str()) {
					names.emplace(form, edid.c_str());
				}
			}
		}
		return names;
	}

	template <class F>
	void ForEachWord(std::string_view a_list, F&& a_fn) {
		std::size_t start = 0;
		while (start < a_list.size()) {
			auto end = a_list.find(' ', start);
			if (end == std::string_view::npos) {
				end = a_list.size();
			}
			if (end > start) {
				a_fn(a_list.substr(start, end - start));
			}
			start = end + 1;
		}
	}
}

EconomyService* EconomyService::GetSingleton() {
	static EconomyService singleton;
	return &singleton;
}

void EconomyService::Register() {
	auto* dh = RE::TESDataHandler::GetSingleton();
	if (!dh) {
		return;
	}

	// ---- gold in leveled lists, classified by the list's editor ID
	const auto names = LeveledItemNames();
	if (names.empty()) {
		Log::ERROR("economy: no leveled-list editor IDs loaded (po3 Tweaks with Load EditorIDs) - gold rows do nothing");
	}
	std::size_t lists[3]{}, unnamed = 0;
	for (auto* list : dh->GetFormArray<RE::TESLevItem>()) {
		if (!list || names.empty()) {
			continue;
		}
		bool hasGold = false;
		for (auto& e : list->entries) {
			if (e.form && e.form->GetFormID() == kGold001) {
				hasGold = true;
				break;
			}
		}
		if (!hasGold) {
			continue;
		}
		const auto              named = names.find(list);
		const std::string_view edid = named != names.end() ? std::string_view{ named->second } : std::string_view{};
		if (edid.empty()) {
			++unnamed;  // treated as loot
		}
		const auto cls = Classify(edid);
		++lists[static_cast<int>(cls)];
		if (cls == GoldClass::kSkip) {
			continue;
		}
		for (auto& e : list->entries) {
			if (e.form && e.form->GetFormID() == kGold001) {
				(cls == GoldClass::kMerchant ? _merchant : _loot).push_back({&e, e.count});
			}
		}
	}
	Log::INFO("economy: gold lists - {} loot ({} entries), {} merchant ({} entries), {} left alone (investments / rewards){}",
				 lists[0], _loot.size(), lists[1], _merchant.size(), lists[2],
				 unnamed ? std::format(", {} without an editor ID counted as loot", unnamed) : "");

	// ---- chance-none globals the menu scales: their Skyblivion.esm values
	for (const auto& r : SKYBSettingsMenu::kRows) {
		if (r.applier != SKYBSettingsMenu::Applier::kChanceNone) {
			continue;
		}
		ForEachWord(r.key, [this](std::string_view a_edid) {
			if (auto* g = RE::TESForm::LookupByEditorID<RE::TESGlobal>(a_edid)) {
				_chanceBase.emplace(std::string(a_edid), g->value);
				Log::INFO("economy: {} = {} at data load", a_edid, g->value);
			} else {
				Log::WARN("economy: GlobalVariable {} not found", a_edid);
			}
		});
	}

	// ---- the player's price perk
	_perk = RE::TESForm::LookupByEditorID<RE::BGSPerk>(kPerk);
	if (!_perk) {
		Log::WARN("economy: perk {} not found in any loaded plugin - Buying / Selling Prices do nothing", kPerk);
		return;
	}
	for (auto* e : _perk->perkEntries) {
		if (!e || e->GetType() != RE::PERK_ENTRY_TYPE::kEntryPoint) {
			continue;
		}
		auto* ep = static_cast<RE::BGSEntryPointPerkEntry*>(e);
		auto* fn = ep->functionData && ep->functionData->GetType() == RE::BGSEntryPointFunctionData::ENTRY_POINT_FUNCTION_DATA::kOneValue ?
					   static_cast<RE::BGSEntryPointFunctionDataOneValue*>(ep->functionData) :
					   nullptr;
		if (ep->entryData.entryPoint == RE::BGSEntryPoint::ENTRY_POINT::kModBuyPrices) {
			_buy = fn;
		} else if (ep->entryData.entryPoint == RE::BGSEntryPoint::ENTRY_POINT::kModSellPrices) {
			_sell = fn;
		}
	}
	if (!_buy || !_sell) {
		Log::ERROR("economy: SKYBEconomyPerk is missing its buy / sell entry");
		_buy = _sell = nullptr;
	}
}

void EconomyService::OnGameReady() {
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (_perk && player && !player->HasPerk(_perk)) {
		player->AddPerk(_perk);
		Log::INFO("economy: SKYBEconomyPerk given to the player");
	}
}

void EconomyService::ScaleGold(std::vector<GoldEntry>& a_entries, float a_mult) {
	for (auto& g : a_entries) {
		const long v = std::lround(static_cast<double>(g.base) * a_mult);
		g.object->count = static_cast<std::uint16_t>(std::clamp<long>(v, 1, 65535));
	}
}

void EconomyService::WritePrices() {
	if (_buy && _sell) {
		_buy->data = _buyMult;
		_sell->data = _sellMult;
	}
}

void EconomyService::Set(std::string_view a_section, std::string_view a_key, float a_value) {
	if (a_section == "Gold" && a_key == "Loot") {
		ScaleGold(_loot, a_value);
	} else if (a_section == "Gold" && a_key == "Merchant") {
		ScaleGold(_merchant, a_value);
	} else if (a_section == "Price" && a_key == "Buy") {
		_buyMult = a_value;
		WritePrices();
	} else if (a_section == "Price" && a_key == "Sell") {
		_sellMult = a_value;
		WritePrices();
	} else {
		Log::WARN("economy: unknown setting {}.{}", a_section, a_key);
		return;
	}
	Log::INFO("economy: {}.{} x{}", a_section, a_key, a_value);
}

void EconomyService::SetChanceNone(std::string_view a_globals, float a_mult) {
	ForEachWord(a_globals, [this, a_mult](std::string_view a_edid) {
		const auto it = _chanceBase.find(std::string(a_edid));
		auto*      g = RE::TESForm::LookupByEditorID<RE::TESGlobal>(a_edid);
		if (it == _chanceBase.end() || !g) {
			return;
		}
		// scale the chance to DROP (100 - chance none)
		const float drop = std::clamp((100.0f - it->second) * a_mult, 0.0f, 100.0f);
		g->value = std::round(100.0f - drop);
		Log::INFO("economy: {} = {} (drop chance {}% -> {}%)", a_edid, g->value, 100.0f - it->second, drop);
	});
}
