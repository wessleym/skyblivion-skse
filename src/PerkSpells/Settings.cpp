#include "PerkSpells/Settings.h"

#include "Platform/IniFile.h"
#include "Platform/Text.h"

#include <string>

using namespace std::literals;

namespace PerkSpells::Settings {
    namespace {
        Values g_values;

        // key is "section.name", lower-cased.
        void Apply(const std::string& a_key, const std::string& a_value) {
            auto& v = g_values;
            if (a_key == "spells.sstacking") {
                const auto s = Text::Lower(a_value);
                if (s == "off") {
                    v.stacking = Stacking::kOff;
                } else if (s == "perperk") {
                    v.stacking = Stacking::kPerPerk;
                } else if (s == "all") {
                    v.stacking = Stacking::kAll;
                } else {
                    Log::WARN("PerkSpells: INI: unknown sStacking '{}', keeping {}", a_value, Name(v.stacking));
                }
            } else if (a_key == "spells.bownercasts") {
                v.ownerCasts = Text::ParseBool(a_value, v.ownerCasts);
            } else if (a_key == "spells.bswinghostiletoattacker") {
                v.swingHostileToAttacker = Text::ParseBool(a_value, v.swingHostileToAttacker);
            } else if (a_key == "entrypoints.bcombathit") {
                v.enabled[std::to_underlying(Kind::kCombatHit)] = Text::ParseBool(a_value, true);
            } else if (a_key == "entrypoints.bbashing") {
                v.enabled[std::to_underlying(Kind::kBashing)] = Text::ParseBool(a_value, true);
            } else if (a_key == "entrypoints.breanimate") {
                v.enabled[std::to_underlying(Kind::kReanimate)] = Text::ParseBool(a_value, true);
            } else if (a_key == "entrypoints.bweaponswing") {
                v.enabled[std::to_underlying(Kind::kWeaponSwing)] = Text::ParseBool(a_value, true);
            } else if (a_key == "entrypoints.bsneaking") {
                v.enabled[std::to_underlying(Kind::kSneaking)] = Text::ParseBool(a_value, true);
            } else {
                Log::WARN("PerkSpells: INI: unknown key '{}'", a_key);
            }
        }
    }

    void Load() {
        constexpr auto path = "Data/SKSE/Plugins/SKYBPerkSpells.ini";
        const bool read = IniFile::Read(path, "PerkSpells: INI", [](const IniFile::Entry& a_entry) {
            Apply(Text::Lower(a_entry.section + "." + a_entry.key), a_entry.value);
        });
        if (!read) {
            Log::INFO("PerkSpells: INI: {} not found, using defaults", path);
        }

        const auto& v = g_values;
        Log::INFO("PerkSpells: settings: stacking={} ownerCasts={} swingHostileToAttacker={} combatHit={} bashing={} reanimate={} weaponSwing={} sneaking={}",
                  Name(v.stacking), v.ownerCasts, v.swingHostileToAttacker,
                  v.enabled[0], v.enabled[1], v.enabled[2], v.enabled[3], v.enabled[4]);
    }

    const Values& Get() {
        return g_values;
    }

    std::string_view Name(Kind a_kind) {
        switch (a_kind) {
            case Kind::kCombatHit:
                return "CombatHit"sv;
            case Kind::kBashing:
                return "Bashing"sv;
            case Kind::kReanimate:
                return "Reanimate"sv;
            case Kind::kWeaponSwing:
                return "WeaponSwing"sv;
            case Kind::kSneaking:
                return "Sneaking"sv;
            default:
                return "?"sv;
        }
    }

    std::string_view Name(Stacking a_stacking) {
        switch (a_stacking) {
            case Stacking::kOff:
                return "Off"sv;
            case Stacking::kPerPerk:
                return "PerPerk"sv;
            case Stacking::kAll:
                return "All"sv;
            default:
                return "?"sv;
        }
    }
}
