#pragma once

// PerkSpells settings, read once at load from Data\SKSE\Plugins\SKYBPerkSpells.ini
// (the standalone SKYBPerkSpells plugin's file, same keys and defaults).
// Missing file or keys fall back to the defaults below.

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace PerkSpells::Settings {
    // The perk entry points this plugin manages.
    enum class Kind : std::uint8_t {
        kCombatHit,
        kBashing,
        kReanimate,
        kWeaponSwing,
        kSneaking,
        kTotal
    };

    enum class Stacking : std::uint8_t {
        kOff,      // engine behaviour: one spell per entry point call
        kPerPerk,  // each perk contributes at most one spell; different perks stack
        kAll       // every matching entry contributes its spell
    };

    struct Values {
        Stacking                                            stacking = Stacking::kPerPerk;
        bool                                                ownerCasts = true;
        bool                                                swingHostileToAttacker = true;
        std::array<bool, std::to_underlying(Kind::kTotal)> enabled{ true, true, true, true, true };
    };

    void              Load();
    const Values&     Get();
    std::string_view  Name(Kind a_kind);
    std::string_view  Name(Stacking a_stacking);
}
