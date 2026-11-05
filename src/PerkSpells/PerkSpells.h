#pragma once

// PerkSpells: the standalone SKYBPerkSpells plugin (0.2.0) folded into Skyblivion.dll.
// Perk "Apply ... Spell" entry points deliver every matching spell (per perk by
// default) and the perk owner casts them on the target. Settings stay in
// Data\SKSE\Plugins\SKYBPerkSpells.ini; log lines go to Skyblivion.log, prefixed
// "PerkSpells:". The standalone mod must not be enabled alongside this (both would
// redirect the same call sites).

namespace PerkSpells {
    // Call from Plugin::OnLoad, at plugin load (where the standalone's SKSEPluginLoad
    // installed), after SKSE::AllocTrampoline.
    void Install();
}
