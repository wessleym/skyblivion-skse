#include "SettingsMenu/CombatService.h"

namespace {
	constexpr std::array<const char*, 6> kLevels = {"VE", "E", "N", "H", "VH", "L"};
	//by editor ID: the perk may ship in SKYBCombat.esp, SKYBCombined.esp or the ESM
	constexpr const char*                kAllyPerk = "SKYBCombatAllyPerk";
}

CombatService* CombatService::GetSingleton() {
	static CombatService singleton;
	return &singleton;
}

void CombatService::Register() {
	auto* gmsts = RE::GameSettingCollection::GetSingleton();
	for (std::size_t i = 0; i < kLevels.size(); ++i) {
		for (auto [arr, prefix] : {std::pair{&_byPC, "fDiffMultHPByPC"}, std::pair{&_toPC, "fDiffMultHPToPC"}}) {
			const auto name = std::format("{}{}", prefix, kLevels[i]);
			auto*      s = gmsts ? gmsts->GetSetting(name.c_str()) : nullptr;
			if (!s) {
				Log::WARN("combat: game setting {} not found - that difficulty level is left alone", name);
				continue;
			}
			//every difficulty level deals and takes x1: the settings menu's Damage rows are the only scale
			//(the vanilla difficulty option is hidden in the journal and the title screen)
			Log::INFO("combat: {} was {} - now 1 (times the Damage row)", name, s->GetFloat());
			(*arr)[i] = {s, 1.0f};
		}
	}

	auto* dh = RE::TESDataHandler::GetSingleton();
	auto* perk = dh ? RE::TESForm::LookupByEditorID<RE::BGSPerk>(kAllyPerk) : nullptr;
	if (!perk) {
		Log::WARN("combat: perk {} not found in any loaded plugin - the damage rows reach the player only, not allies", kAllyPerk);
	} else {
		for (auto* e : perk->perkEntries) {
			if (!e || e->GetType() != RE::PERK_ENTRY_TYPE::kEntryPoint) {
				continue;
			}
			auto* ep = static_cast<RE::BGSEntryPointPerkEntry*>(e);
			auto* fn = ep->functionData && ep->functionData->GetType() == RE::BGSEntryPointFunctionData::ENTRY_POINT_FUNCTION_DATA::kOneValue ?
						   static_cast<RE::BGSEntryPointFunctionDataOneValue*>(ep->functionData) :
						   nullptr;
			if (!fn) {
				continue;
			}
			switch (ep->entryData.entryPoint.get()) {
				case RE::BGSEntryPoint::ENTRY_POINT::kModIncomingDamage:
					_incomingWeapon = fn;
					break;
				case RE::BGSEntryPoint::ENTRY_POINT::kModAttackDamage:
					_outgoingWeapon = fn;
					break;
				case RE::BGSEntryPoint::ENTRY_POINT::kModIncomingSpellMagnitude:
					_incomingSpell = fn;
					break;
				case RE::BGSEntryPoint::ENTRY_POINT::kModSpellMagnitude:
					_outgoingSpell = fn;
					break;
				default:
					break;
			}
		}
		if (!_incomingWeapon || !_outgoingWeapon || !_incomingSpell || !_outgoingSpell) {
			Log::ERROR("combat: SKYBCombatAllyPerk is missing one of its four entries - allies untouched");
			_incomingWeapon = _outgoingWeapon = _incomingSpell = _outgoingSpell = nullptr;
		} else {
			Log::INFO("combat: SKYBCombatAllyPerk {:08X} found - values written here, distribution is in the data",
						 perk->GetFormID());
		}
	}
	_registered = true;
	Apply();
}

void CombatService::Set(std::string_view a_key, float a_value) {
	if (a_key == "Taken") {
		_taken = a_value;
	} else if (a_key == "Dealt") {
		_dealt = a_value;
	} else if (a_key == "Allies") {
		_allies = a_value >= 0.5f;
	} else {
		Log::WARN("combat: unknown setting '{}'", a_key);
		return;
	}
	Apply();
}

void CombatService::Apply() {
	if (!_registered) {
		return;  // values arrive before data load only on odd paths; Register applies them
	}
	for (std::size_t i = 0; i < kLevels.size(); ++i) {
		if (_byPC[i].setting) {
			_byPC[i].setting->data.f = _byPC[i].base * _dealt;
		}
		if (_toPC[i].setting) {
			_toPC[i].setting->data.f = _toPC[i].base * _taken;
		}
	}
	const float taken = _allies ? _taken : 1.0f;
	const float dealt = _allies ? _dealt : 1.0f;
	if (_incomingWeapon) {
		_incomingWeapon->data = taken;
		_incomingSpell->data = taken;
		_outgoingWeapon->data = dealt;
		_outgoingSpell->data = dealt;
	}
	Log::INFO("combat: damage taken x{} dealt x{} (allies {})", _taken, _dealt,
				 _incomingWeapon ? (_allies ? "follow" : "off") : "not available");
}
