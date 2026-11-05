#include "PerkSpells/Settings.h"

#include <cctype>
#include <fstream>
#include <string>

using namespace std::literals;

namespace PerkSpells::Settings {
    namespace {
        Values g_values;

        std::string Trim(std::string_view a_text) {
            std::size_t begin = 0;
            std::size_t end = a_text.size();
            while (begin < end && std::isspace(static_cast<unsigned char>(a_text[begin]))) {
                ++begin;
            }
            while (end > begin && std::isspace(static_cast<unsigned char>(a_text[end - 1]))) {
                --end;
            }
            return std::string(a_text.substr(begin, end - begin));
        }

        std::string Lower(std::string a_text) {
            for (auto& c : a_text) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return a_text;
        }

        bool ParseBool(const std::string& a_value, bool a_fallback) {
            const auto v = Lower(a_value);
            if (v == "1" || v == "true" || v == "yes" || v == "on") {
                return true;
            }
            if (v == "0" || v == "false" || v == "no" || v == "off") {
                return false;
            }
            return a_fallback;
        }

        // key is "section.name", lower-cased.
        void Apply(const std::string& a_key, const std::string& a_value) {
            auto& v = g_values;
            if (a_key == "spells.sstacking") {
                const auto s = Lower(a_value);
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
                v.ownerCasts = ParseBool(a_value, v.ownerCasts);
            } else if (a_key == "spells.bswinghostiletoattacker") {
                v.swingHostileToAttacker = ParseBool(a_value, v.swingHostileToAttacker);
            } else if (a_key == "entrypoints.bcombathit") {
                v.enabled[std::to_underlying(Kind::kCombatHit)] = ParseBool(a_value, true);
            } else if (a_key == "entrypoints.bbashing") {
                v.enabled[std::to_underlying(Kind::kBashing)] = ParseBool(a_value, true);
            } else if (a_key == "entrypoints.breanimate") {
                v.enabled[std::to_underlying(Kind::kReanimate)] = ParseBool(a_value, true);
            } else if (a_key == "entrypoints.bweaponswing") {
                v.enabled[std::to_underlying(Kind::kWeaponSwing)] = ParseBool(a_value, true);
            } else if (a_key == "entrypoints.bsneaking") {
                v.enabled[std::to_underlying(Kind::kSneaking)] = ParseBool(a_value, true);
            } else if (a_key == "debug.bpatcheverycallsite") {
                v.patchEveryCallSite = ParseBool(a_value, v.patchEveryCallSite);
            } else if (a_key == "debug.ilogcallsperentrypoint") {
                try {
                    v.logCallsPerEntryPoint = static_cast<std::uint32_t>(std::stoul(a_value));
                } catch (...) {
                    Log::WARN("PerkSpells: INI: bad iLogCallsPerEntryPoint '{}'", a_value);
                }
            } else {
                Log::WARN("PerkSpells: INI: unknown key '{}'", a_key);
            }
        }
    }

    void Load() {
        constexpr auto path = "Data/SKSE/Plugins/SKYBPerkSpells.ini";
        std::ifstream file(path);
        if (!file) {
            Log::INFO("PerkSpells: INI: {} not found, using defaults", path);
        } else {
            std::string section;
            std::string line;
            while (std::getline(file, line)) {
                const auto cut = line.find_first_of(";#");
                auto text = Trim(cut == std::string::npos ? line : line.substr(0, cut));
                if (text.empty()) {
                    continue;
                }
                if (text.front() == '[' && text.back() == ']') {
                    section = Lower(Trim(std::string_view(text).substr(1, text.size() - 2)));
                    continue;
                }
                const auto eq = text.find('=');
                if (eq == std::string::npos) {
                    continue;
                }
                Apply(section + "." + Lower(Trim(std::string_view(text).substr(0, eq))),
                      Trim(std::string_view(text).substr(eq + 1)));
            }
        }

        const auto& v = g_values;
        Log::INFO("PerkSpells: settings: stacking={} ownerCasts={} swingHostileToAttacker={} combatHit={} bashing={} reanimate={} weaponSwing={} sneaking={} patchEveryCallSite={} logCalls={}",
                  Name(v.stacking), v.ownerCasts, v.swingHostileToAttacker,
                  v.enabled[0], v.enabled[1], v.enabled[2], v.enabled[3], v.enabled[4],
                  v.patchEveryCallSite, v.logCallsPerEntryPoint);
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
