#pragma once

// Perk "Apply ... Spell" entry points (Combat Hit, Bashing, Reanimate,
// Weapon Swing, Sneaking): collect more than one matching spell per call and,
// for the entry points that have a separate target, let the perk owner cast
// them on that target. Folded in from the standalone SKYBPerkSpells plugin
// (0.2.0); the mechanism and engine facts are in its README.md.
//
// How it works (all engine facts below were confirmed against Skyblivion.esm
// data or CommonLibSSE-NG declarations):
//  - Every perk entry point goes through one engine function,
//    BGSEntryPoint::HandleEntryPoint(entryPoint, perkOwner, ...). Its variadic
//    tail is one argument per extra condition tab (the perk entry's
//    "PerkConditionTabCount" minus the Perk Owner tab) followed by the result
//    pointer (SpellItem** for the apply-spell entry points).
//  - The call sites that pass one of our entry points are found at load by
//    scanning the game's code for calls to that function, and redirected to
//    a thunk that forwards everything else untouched.
//  - While the engine evaluates one of our calls, its perk-entry visitor
//    (Actor::ForEachPerkEntry, virtual 0x100) is wrapped so every entry whose
//    conditions pass is recorded instead of only the one the engine keeps.
//  - The recorded spells are then delivered by this plugin.
// Every assumption is checked at runtime; a failed check falls back to the
// engine's own single-spell behaviour and logs a warning once.

namespace PerkSpells::EntryPointSpells {
    // Call from Plugin::OnLoad, after the plugin's trampoline is allocated
    // (patches code; uses one trampoline jump for all call sites).
    void Install();
}
