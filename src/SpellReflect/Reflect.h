#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Chance-based Reflect Spell (Oblivion style) + perk-gated parries: arrows (Reflect Arrows,
// rank 2 with a shield) and spell projectiles (Reflect Spells) turned back by a timed block.
//
// Two engine hooks:
//  * Actor MagicTarget::AddTarget - vtable slot 1 of the MagicTarget base of
//    Character and PlayerCharacter (a pointer swap, no code patched). A
//    reflected spell's effects are re-added to the caster.
//  * the projectile hit handler (AE id 44205) - its 5 direct call sites,
//    each checked before any is patched. Rolls once per projectile hit, and
//    turns projectiles around (bounce mode, reflected arrows).

namespace SpellReflect::Reflect {
    // Install both hooks. Call at plugin load, after Settings::Load().
    void Install();

    // Resolve the actor values and the parry perks. Call at SKSE kDataLoaded.
    void OnDataLoaded();
}
