#pragma once
// Moved from SKYBDegradation.dll into Skyblivion.dll, 2026-10-05 (fold-in, UI integration workspace
// ledger L-0058). Source of record until then: game\native\SKYBDegradation\src in that workspace.
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Makes the engine's own temper system honour condition below 100%.
//
// Found in the runtime dump of SkyrimSE 1.6.1170 (2026-09-28):
//   id 15990  InventoryEntryData health  = max(1.0, highest ExtraHealth)   <- the 1.0 floor
//   id 26410  weapon damage (actor, weapon, ammo, health, mult, flag); temper bonus via id 26498
//   id 16017  armor rating of an entry; reads 15990, temper bonus via id 26499(health, armor)
// Callers covered:
//   combat    id 44100 (hit data: attacker's equipped weapon) -> 15990 + 26410
//             id 40299 (worn-armor total)                      -> 16017
//   display   id 40252 / 40253 (player damage) -> 15990 -> 40291 -> 26410
//             id 40249 (player armor)          -> 16017
// Seven call sites are redirected (SKSE trampoline, write_call<5>):
//   the four health reads -> Health(): same "highest list" rule without the floor
//   the two 26410 calls   -> Damage(): original at max(health, 1) x Condition::StatMult(health)
//   the 26499 call        -> ArmorBonus(): below 1.0 a negative bonus that scales the rating
//                            to Condition::StatMult(health) of itself
// The smithing menu's own reads (id 51360) are untouched.
//
// Safety: installs only on Anniversary Edition runtimes, and only if EVERY site
// is found holding the vanilla `call` to the expected function: on 1.6.1170 at
// the dumped offset; on another AE build at that offset or, failing that, as the
// one such call in the function's first 0x400 bytes. SE, VR, a hooked site or an
// unfound one = nothing is patched and condition below 100% has no stat effect;
// the log says which. (Skyblivion.dll since 2026-10-05, L-0058.)
namespace SKYBDeg::TemperHook
{
	// plugin load (allocates its own trampoline when it patches).
	bool Install();
	[[nodiscard]] bool Active();
}
