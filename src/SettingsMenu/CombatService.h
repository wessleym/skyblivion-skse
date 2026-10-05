#pragma once
// Moved from SKYBUIDemo.dll into Skyblivion.dll, 2026-10-05 (fold-in, UI integration workspace
// ledger L-0056). Source of record until then: game\native\SKYBUIDemo\src in that workspace.
#include <array>
#include <string>
#include <string_view>
// Combat > Damage rows of the settings menu: damage taken / dealt by the player
// and, optionally, the player's allies.
//
//   player  the engine's own difficulty multipliers. Every level of
//           fDiffMultHPByPC{VE,E,N,H,VH,L} (damage the player deals) and
//           fDiffMultHPToPC{..} (damage the player takes) becomes its
//           value at data load x the menu's multiplier, so the multipliers
//           stack with the game's difficulty and cover weapons and spells.
//           Game settings are not saved: SettingsStore re-applies on every
//           load.
//   allies  SKYBCombat.esp PERK 000800 SKYBCombatAllyPerk (hidden). Scott
//           distributes it in the data (not this DLL). Its four Multiply
//           entries - weapons taken / dealt, damaging spells taken / dealt -
//           fire only for player teammates and commanded actors (summons,
//           reanimated dead); this service writes their values.
//           "Allies Follow These Rules" off = all four at 1.0.
//
// Missing SKYBCombat.esp = the player half still works; the log says so.
class CombatService {
public:
	static CombatService* GetSingleton();
	void Register();                                  // kDataLoaded
	void Set(std::string_view a_key, float a_value);  // "Taken" | "Dealt" | "Allies"

private:
	CombatService() = default;
	void Apply();

	struct Gmst {
		RE::Setting* setting = nullptr;
		float        base = 1.0f;
	};
	std::array<Gmst, 6> _byPC{};  // damage the player deals, per difficulty level
	std::array<Gmst, 6> _toPC{};  // damage the player takes
	// the ally perk's value slots, in the ESP's entry order
	RE::BGSEntryPointFunctionDataOneValue* _incomingWeapon = nullptr;
	RE::BGSEntryPointFunctionDataOneValue* _outgoingWeapon = nullptr;
	RE::BGSEntryPointFunctionDataOneValue* _incomingSpell = nullptr;
	RE::BGSEntryPointFunctionDataOneValue* _outgoingSpell = nullptr;
	float _taken = 1.0f;
	float _dealt = 1.0f;
	bool  _allies = true;
	bool  _registered = false;
};
