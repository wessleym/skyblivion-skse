#pragma once

// Timed block (player only), ranked by the Block mastery perks ([TimedBlock] sRankPerks,
// the highest held counts): a melee hit the engine marks as blocked, landing within a
// FIXED window (fWindow) after the block press, is a timed block. Per rank: the share of
// the damage taken away (100 = the hit is not applied - no damage, stagger or block stamina
// cost; the attacker's enchantment, poison and combat-hit spells are applied earlier and still
// land), whether a reduced hit costs block stamina, the stamina restored, whether the attacker
// staggers, and the cooldown.
//
// Hook: the melee hit function (AE 38627, SE/VR 37673) builds the HitData and then, at
// +0x4A8 (SE/VR +0x3C0), calls the function that applies it to the target (AE 38586,
// SE/VR 37633). That one call is wrapped (write_call, verified first); a negated hit
// skips it. The same function applies arrow and bolt hits, which Decide leaves alone.

namespace SpellReflect::TimedBlock {
    // Install the call-site hook. Call at plugin load (trampoline allocated).
    void Install();

    // Resolve the rank perks and the stagger spell. Call at SKSE kDataLoaded.
    void OnDataLoaded();
}
