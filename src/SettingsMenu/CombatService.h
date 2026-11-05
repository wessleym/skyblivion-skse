#pragma once
#include <array>
#include <string>
#include <string_view>
// Combat > Damage rows of the settings menu: damage taken / dealt by the player
// and, optionally, the player's allies.
//
//   player  every level of fDiffMultHPByPC{VE,E,N,H,VH,L} (damage dealt) and
//           fDiffMultHPToPC{..} (damage taken) = its data-load value x the row,
//           so the rows stack with difficulty and cover weapons and spells.
//           Game settings are not saved: SettingsStore re-applies on every load.
//   allies  the four Multiply entries of SKYBCombatAllyPerk (weapons and damaging
//           spells, taken / dealt), which fire only for player teammates and
//           commanded actors. The game data distributes the perk; this service
//           writes its values. "Allies Follow These Rules" off = all four at 1.0.
//
// Without the perk the player half still works; the log says so.
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
