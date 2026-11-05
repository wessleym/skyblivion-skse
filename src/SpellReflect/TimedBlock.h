#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Timed block (player only), ranked by the Block mastery perks ([TimedBlock] sRankPerks,
// the highest held counts): a melee hit the engine marks as blocked, landing within a
// FIXED window (fWindow) after the block press, is a timed block. Per rank: the share of
// the damage taken away (100 = the hit is not applied at all - no damage, stagger, hit
// effects or block stamina cost), whether a reduced hit costs block stamina, the stamina
// restored, whether the attacker staggers, and the cooldown.
//
// Hook: the melee hit function (AE 38627) builds the HitData and then, at
// +0x4A8, calls the function that applies it to the target (AE 38586). That
// one call is wrapped (write_call, verified first); a negated hit skips it.

namespace SpellReflect::TimedBlock {
    // Install the call-site hook. Call at plugin load (trampoline allocated).
    void Install();

    // Resolve the rank perks and the stagger spell. Call at SKSE kDataLoaded.
    void OnDataLoaded();
}
