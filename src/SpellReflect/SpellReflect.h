#pragma once

// SpellReflect: the standalone SKYBSpellReflect plugin (0.2.0) folded into Skyblivion.dll and
// extended for the perk rework (Skyblivion UI integration workspace): the timed block ranked
// by the Block mastery perks, arrow reflection with a shield rank, spell projectiles parried
// by a timed block, the parry window from Speed, and Brute Force (power attacks force locked containers). Settings stay in
// Data\SKSE\Plugins\SKYBSpellReflect.ini; log lines go to Skyblivion.log, prefixed
// "SpellReflect:". The standalone mod must not be enabled alongside this (both would hook the
// same call sites). Third-party notices: SpellReflect\THIRD_PARTY_NOTICES.md.

namespace SpellReflect {
    // Call from Plugin::OnLoad, at plugin load (where the standalone's SKSEPluginLoad installed),
    // after SKSE::AllocTrampoline.
    void Install();

    // SKSE kInputLoaded: the block-press listener.
    void OnInputLoaded();

    // SKSE kDataLoaded: forms, perks, sounds.
    void OnDataLoaded();
}
