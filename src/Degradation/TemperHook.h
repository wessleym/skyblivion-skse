#pragma once

// Makes the engine's own temper system honour condition below 100%.
//
// Engine functions (AE ids, offsets from 1.6.1170):
//   id 15990  InventoryEntryData health  = max(1.0, highest ExtraHealth)   <- the 1.0 floor
//   id 26410  weapon damage (actor, weapon, ammo, health, mult, flag); temper bonus via id 26498
//   id 16017  armor rating of an entry; reads 15990, temper bonus via id 26499(health, armor)
// Callers covered:
//   melee     the hit data (id 44001; melee swings, bashes, kill moves) -> 16016 -> 15990 + 26409 (-> 26410)
//   arrows    id 44100 (projectile constructors: the projectile's weaponDamage) -> 15990 + 26410.
//             A projectile hit's hit data is built from the projectile (the projectile branch of
//             38627 calls 44002, which reads neither 16016 nor 15990), so an arrow is scaled once.
//   armor     id 40299 (worn-armor total)                      -> 16017
//   display   id 40252 / 40253 (player damage) -> 15990 -> 40291 -> 26410
//             id 40249 (player armor)          -> 16017
// Nine call sites are redirected (the plugin's SKSE trampoline, allocated in Plugin::OnLoad; write_call<5>):
//   the five health reads -> Health(): same "highest list" rule without the floor (worn copies only, when worn)
//   the two 26410 calls and the 26409 call -> Damage(): original at max(health, 1) x Condition::StatMult(health)
//   the 26499 call        -> ArmorBonus(): below 1.0 a negative bonus that scales the rating
//                            to Condition::StatMult(health) of itself
// The smithing menu's own reads (id 51360) are untouched.
//
// Safety: installs only on Anniversary Edition runtimes, and only where a site is found holding the
// vanilla `call` to the expected function: on 1.6.1170 at the known offset; on another AE build at
// that offset or, failing that, as the one such call near the function's start (0x400 bytes; 0x60 in
// 16016, which 16017 follows). The seven sites install all or none; the two melee sites then install
// both or neither. SE, VR, a hooked site or an unfound one = nothing is patched there; the log says which.
namespace SKYBDeg::TemperHook
{
	// Called at plugin load; patches all seven sites or none, then the two melee sites or neither.
	void Install();
	// Whether Install patched the seven sites.
	[[nodiscard]] bool Installed();
}
