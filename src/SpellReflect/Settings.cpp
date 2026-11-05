#include "SpellReflect/Settings.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <vector>

using namespace std::literals;

namespace SpellReflect::Settings {
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

        void ParseFloat(const std::string& a_key, const std::string& a_value, float& a_out) {
            try {
                a_out = std::stof(a_value);
            } catch (...) {
                Log::WARN("SpellReflect: INI: bad number '{}' for {}", a_value, a_key);
            }
        }

        // "a, b, c" -> trimmed items (empty items kept, so positions line up with the ranks).
        std::vector<std::string> SplitList(const std::string& a_value) {
            std::vector<std::string> items;
            std::size_t start = 0;
            while (true) {
                const auto comma = a_value.find(',', start);
                items.push_back(Trim(std::string_view(a_value).substr(start, comma == std::string::npos ? std::string::npos : comma - start)));
                if (comma == std::string::npos) {
                    break;
                }
                start = comma + 1;
            }
            return items;
        }

        // One value per timed-block rank (Apprentice, Journeyman, Expert, Master).
        template <class Fn>
        void ApplyRankList(const std::string& a_key, const std::string& a_value, Fn a_apply) {
            const auto items = SplitList(a_value);
            if (items.size() != kBlockRanks) {
                Log::WARN("SpellReflect: INI: {} has {} values, expected {} (one per Block mastery rank); extra values are ignored, missing ones keep their defaults",
                          a_key, items.size(), kBlockRanks);
            }
            for (std::size_t i = 0; i < std::min(items.size(), kBlockRanks); ++i) {
                if (!items[i].empty()) {
                    a_apply(g_values.blockRanks[i], items[i]);
                }
            }
        }

        // Five values, one per lock level (Novice, Apprentice, Adept, Expert, Master).
        void ParseLockTable(const std::string& a_key, const std::string& a_value, LockTable& a_out) {
            const auto items = SplitList(a_value);
            if (items.size() != kLockLevels) {
                Log::WARN("SpellReflect: INI: {} has {} values, expected {} (Novice, Apprentice, Adept, Expert, Master)",
                          a_key, items.size(), kLockLevels);
            }
            for (std::size_t i = 0; i < std::min(items.size(), kLockLevels); ++i) {
                if (!items[i].empty()) {
                    ParseFloat(a_key, items[i], a_out[i]);
                }
            }
        }

        // key is "section.name", lower-cased.
        void Apply(const std::string& a_key, const std::string& a_value) {
            auto& v = g_values;
            if (a_key == "reflect.sactorvalue") {
                v.actorValue = a_value;
            } else if (a_key == "reflect.fmaxchance") {
                ParseFloat(a_key, a_value, v.maxChance);
            } else if (a_key == "reflect.sblockingkeyword") {
                v.blockingKeyword = a_value;
            } else if (a_key == "reflect.fmaxblockingchance") {
                ParseFloat(a_key, a_value, v.maxBlockingChance);
            } else if (a_key == "reflect.smode") {
                const auto s = Lower(a_value);
                if (s == "swap") {
                    v.mode = Mode::kSwap;
                } else if (s == "bounce") {
                    v.mode = Mode::kBounce;
                } else {
                    Log::WARN("SpellReflect: INI: unknown sMode '{}', keeping {}", a_value, Name(v.mode));
                }
            } else if (a_key == "reflect.sreturnaim") {
                const auto s = Lower(a_value);
                if (s == "retrace") {
                    v.returnAim = Aim::kRetrace;
                } else if (s == "direct") {
                    v.returnAim = Aim::kDirect;
                } else {
                    Log::WARN("SpellReflect: INI: unknown sReturnAim '{}', keeping Retrace/Direct as set", a_value);
                }
            } else if (a_key == "reflect.breflecttouch") {
                v.reflectTouch = ParseBool(a_value, v.reflectTouch);
            } else if (a_key == "reflect.breflectexplosive") {
                v.reflectExplosive = ParseBool(a_value, v.reflectExplosive);
            } else if (a_key == "reflect.breflectshouts") {
                v.reflectShouts = ParseBool(a_value, v.reflectShouts);
            } else if (a_key == "reflect.breflectstaffs") {
                v.reflectStaffs = ParseBool(a_value, v.reflectStaffs);
            } else if (a_key == "reflect.bignorenoreflectflag") {
                v.ignoreNoReflectFlag = ParseBool(a_value, v.ignoreNoReflectFlag);
            } else if (a_key == "arrows.benabled") {
                v.arrowsEnabled = ParseBool(a_value, v.arrowsEnabled);
            } else if (a_key == "arrows.ballowwithshield") {
                v.arrowAllowShield = ParseBool(a_value, v.arrowAllowShield);
            } else if (a_key == "arrows.sperk") {
                v.arrowPerk = a_value;
            } else if (a_key == "arrows.sshieldperk") {
                v.arrowShieldPerk = a_value;
            } else if (a_key == "arrows.swindowactorvalue") {
                v.windowActorValue = a_value;
            } else if (a_key == "arrows.fwindowbase") {
                ParseFloat(a_key, a_value, v.windowBase);
            } else if (a_key == "arrows.fwindowperpoint") {
                ParseFloat(a_key, a_value, v.windowPerPoint);
            } else if (a_key == "arrows.fwindowmax") {
                ParseFloat(a_key, a_value, v.windowMax);
            } else if (a_key == "arrows.ffacingangle") {
                ParseFloat(a_key, a_value, v.facingAngle);
            } else if (a_key == "arrows.fcooldown") {
                ParseFloat(a_key, a_value, v.arrowCooldown);
            } else if (a_key == "spellparry.benabled") {
                v.parryEnabled = ParseBool(a_value, v.parryEnabled);
            } else if (a_key == "spellparry.sperk") {
                v.parryPerk = a_value;
            } else if (a_key == "spellparry.bparryexplosive") {
                v.parryExplosive = ParseBool(a_value, v.parryExplosive);
            } else if (a_key == "timedblock.benabled") {
                v.timedBlockEnabled = ParseBool(a_value, v.timedBlockEnabled);
            } else if (a_key == "timedblock.fwindow") {
                ParseFloat(a_key, a_value, v.timedBlockWindow);
            } else if (a_key == "timedblock.srankperks") {
                ApplyRankList(a_key, a_value, [](BlockRank& a_rank, const std::string& a_item) { a_rank.perk = a_item; });
            } else if (a_key == "timedblock.fdamagereduction") {
                ApplyRankList(a_key, a_value, [&](BlockRank& a_rank, const std::string& a_item) { ParseFloat(a_key, a_item, a_rank.damageReduction); });
            } else if (a_key == "timedblock.fcooldown") {
                ApplyRankList(a_key, a_value, [&](BlockRank& a_rank, const std::string& a_item) { ParseFloat(a_key, a_item, a_rank.cooldown); });
            } else if (a_key == "timedblock.fstaminarestorepercent") {
                ApplyRankList(a_key, a_value, [&](BlockRank& a_rank, const std::string& a_item) { ParseFloat(a_key, a_item, a_rank.staminaRestorePercent); });
            } else if (a_key == "timedblock.bnostaminacost") {
                ApplyRankList(a_key, a_value, [](BlockRank& a_rank, const std::string& a_item) { a_rank.noStaminaCost = ParseBool(a_item, a_rank.noStaminaCost); });
            } else if (a_key == "timedblock.bstagger") {
                ApplyRankList(a_key, a_value, [](BlockRank& a_rank, const std::string& a_item) { a_rank.stagger = ParseBool(a_item, a_rank.stagger); });
            } else if (a_key == "timedblock.sstaggerspell") {
                v.staggerSpell = a_value;
            } else if (a_key == "bruteforce.benabled") {
                v.bruteEnabled = ParseBool(a_value, v.bruteEnabled);
            } else if (a_key == "bruteforce.srankperks") {
                const auto items = SplitList(a_value);
                if (items.size() != kBruteRanks) {
                    Log::WARN("SpellReflect: INI: {} has {} values, expected {}", a_key, items.size(), kBruteRanks);
                }
                for (std::size_t i = 0; i < std::min(items.size(), kBruteRanks); ++i) {
                    v.bruteRankPerks[i] = items[i];
                }
            } else if (a_key == "bruteforce.fchancerank1" || a_key == "bruteforce.fchancerank2" || a_key == "bruteforce.fchancerank3") {
                ParseLockTable(a_key, a_value, v.bruteChance[static_cast<std::size_t>(a_key.back() - '1')]);
            } else if (a_key == "bruteforce.ballowunarmed") {
                v.bruteAllowUnarmed = ParseBool(a_value, v.bruteAllowUnarmed);
            } else if (a_key == "bruteforce.bdoors") {
                v.bruteDoors = ParseBool(a_value, v.bruteDoors);
            } else if (a_key == "bruteforce.inoiselevel") {
                try {
                    v.bruteNoiseLevel = std::stoi(a_value);
                } catch (...) {
                    Log::WARN("SpellReflect: INI: bad iNoiseLevel '{}'", a_value);
                }
            } else if (a_key == "bruteforce.fwearmult") {
                ParseLockTable(a_key, a_value, v.bruteWearMultFail);
                ParseLockTable(a_key, a_value, v.bruteWearMultSuccess);
            } else if (a_key == "bruteforce.fwearmultfail") {
                ParseLockTable(a_key, a_value, v.bruteWearMultFail);
            } else if (a_key == "bruteforce.fwearmultsuccess") {
                ParseLockTable(a_key, a_value, v.bruteWearMultSuccess);
            } else if (a_key == "bruteforce.ssuccesssound") {
                v.bruteSuccessSound = a_value;
            } else if (a_key == "bruteforce.sfailsound") {
                v.bruteFailSound = a_value;
            } else if (a_key == "bruteforce.bopenonsuccess") {
                v.bruteOpenOnSuccess = ParseBool(a_value, v.bruteOpenOnSuccess);
            } else if (a_key == "bruteforce.fattemptgap") {
                ParseFloat(a_key, a_value, v.bruteAttemptGap);
            } else if (a_key == "bruteforce.fcrimerepeatseconds") {
                ParseFloat(a_key, a_value, v.bruteCrimeRepeat);
            } else if (a_key == "bruteforce.ssuccessmessage") {
                v.bruteSuccessMessage = a_value;
            } else if (a_key == "bruteforce.sfailmessage") {
                v.bruteFailMessage = a_value;
            } else if (a_key == "bruteforce.stoostrongmessage") {
                v.bruteTooStrongMessage = a_value;
            } else if (a_key == "bruteforce.bhitframe") {
                v.bruteHitFrame = ParseBool(a_value, v.bruteHitFrame);
            } else if (a_key == "bruteforce.freach") {
                ParseFloat(a_key, a_value, v.bruteReach);
            } else if (a_key == "bruteforce.btrace") {
                v.bruteTrace = ParseBool(a_value, v.bruteTrace);
            } else if (a_key == "feedback.ssoundlist") {
                v.soundList = a_value;
            } else if (a_key == "feedback.sspellreflectsound") {
                v.spellReflectSound = a_value;
            } else if (a_key == "feedback.sspellreflectvisual") {
                v.spellReflectVisual = a_value;
            } else if (a_key == "feedback.sarrowreflectsound") {
                v.arrowReflectSound = a_value;
            } else if (a_key == "feedback.sarrowreflectvisual") {
                v.arrowReflectVisual = a_value;
            } else if (a_key == "feedback.stimedblocksound") {
                v.timedBlockSound = a_value;
            } else if (a_key == "feedback.stimedblockvisual") {
                v.timedBlockVisual = a_value;
            } else if (a_key == "feedback.fvisualduration") {
                ParseFloat(a_key, a_value, v.visualDuration);
            } else if (a_key == "debug.bobserveonly") {
                v.observeOnly = ParseBool(a_value, v.observeOnly);
            } else if (a_key == "debug.btracereturns") {
                v.traceReturns = ParseBool(a_value, v.traceReturns);
            } else if (a_key == "debug.ilogevents") {
                try {
                    v.logEvents = static_cast<std::uint32_t>(std::stoul(a_value));
                } catch (...) {
                    Log::WARN("SpellReflect: INI: bad iLogEvents '{}'", a_value);
                }
            } else {
                Log::WARN("SpellReflect: INI: unknown key '{}'", a_key);
            }
        }
    }

    void Load() {
        constexpr auto path = "Data/SKSE/Plugins/SKYBSpellReflect.ini";
        std::ifstream file(path);
        if (!file) {
            Log::INFO("SpellReflect: INI: {} not found, using defaults", path);
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

        auto& v = g_values;
        v.maxChance = std::clamp(v.maxChance, 0.0f, 100.0f);
        v.maxBlockingChance = std::clamp(v.maxBlockingChance, v.maxChance, 100.0f);
        v.windowMax = std::max(v.windowMax, 0.0f);
        v.facingAngle = std::clamp(v.facingAngle, 0.0f, 180.0f);
        for (auto& rank : v.blockRanks) {
            rank.damageReduction = std::clamp(rank.damageReduction, 0.0f, 100.0f);
            rank.cooldown = std::max(rank.cooldown, 0.0f);
            rank.staminaRestorePercent = std::clamp(rank.staminaRestorePercent, 0.0f, 100.0f);
        }

        Log::INFO("SpellReflect: settings: returnAim={}", v.returnAim == Aim::kRetrace ? "Retrace" : "Direct");
        Log::INFO("SpellReflect: settings: av={} maxChance={} blockingKeyword={} maxBlockingChance={} mode={} touch={} explosive={} shouts={} staffs={} ignoreNoReflect={}",
                  v.actorValue, v.maxChance, v.blockingKeyword, v.maxBlockingChance, Name(v.mode), v.reflectTouch, v.reflectExplosive,
                  v.reflectShouts, v.reflectStaffs, v.ignoreNoReflectFlag);
        Log::INFO("SpellReflect: settings: arrows={} perk={} shieldPerk={} allowShield={} windowAV={} window={}+{}/pt max {} facing={} cooldown={} observeOnly={} logEvents={}",
                  v.arrowsEnabled, v.arrowPerk, v.arrowShieldPerk, v.arrowAllowShield, v.windowActorValue, v.windowBase, v.windowPerPoint,
                  v.windowMax, v.facingAngle, v.arrowCooldown, v.observeOnly, v.logEvents);
        Log::INFO("SpellReflect: settings: spellParry={} perk={} explosive={}", v.parryEnabled, v.parryPerk, v.parryExplosive);
        Log::INFO("SpellReflect: settings: timedBlock={} window={} staggerSpell={} visualDuration={}", v.timedBlockEnabled,
                  v.timedBlockWindow, v.staggerSpell, v.visualDuration);
        for (auto& row : v.bruteChance) {
            for (auto& c : row) {
                c = std::clamp(c, 0.0f, 100.0f);
            }
        }
        for (auto* table : { &v.bruteWearMultFail, &v.bruteWearMultSuccess }) {
            for (auto& w : *table) {
                w = std::max(w, 0.0f);
            }
        }
        Log::INFO("SpellReflect: settings: bruteForce={} doors={} perks={}, {}, {} unarmed={} noise={} open={} gap={}s crimeRepeat={}s",
                  v.bruteEnabled, v.bruteDoors, v.bruteRankPerks[0], v.bruteRankPerks[1], v.bruteRankPerks[2], v.bruteAllowUnarmed,
                  v.bruteNoiseLevel, v.bruteOpenOnSuccess, v.bruteAttemptGap, v.bruteCrimeRepeat);
        v.bruteReach = std::max(v.bruteReach, 0.0f);
        Log::INFO("SpellReflect: settings: brute force hitFrame={} reach={} trace={}", v.bruteHitFrame, v.bruteReach, v.bruteTrace);
        for (std::size_t i = 0; i < kBruteRanks; ++i) {
            const auto& c = v.bruteChance[i];
            Log::INFO("SpellReflect: settings: brute force rank {}: chance {}/{}/{}/{}/{}% (Novice..Master)", i + 1, c[0], c[1], c[2], c[3], c[4]);
        }
        const auto& wf = v.bruteWearMultFail;
        const auto& ws = v.bruteWearMultSuccess;
        Log::INFO("SpellReflect: settings: brute force wear x{}/{}/{}/{}/{} held, x{}/{}/{}/{}/{} forced (Novice..Master)", wf[0], wf[1],
                  wf[2], wf[3], wf[4], ws[0], ws[1], ws[2], ws[3], ws[4]);
        Log::INFO("SpellReflect: settings: brute force sounds: forced '{}', held '{}'", v.bruteSuccessSound, v.bruteFailSound);
        for (std::size_t i = 0; i < kBlockRanks; ++i) {
            const auto& r = v.blockRanks[i];
            Log::INFO("SpellReflect: settings: timed block rank {}: perk={} reduction={}% cooldown={}s stamina={}% noStaminaCost={} stagger={}",
                      i + 1, r.perk, r.damageReduction, r.cooldown, r.staminaRestorePercent, r.noStaminaCost, r.stagger);
        }
    }

    const Values& Get() {
        return g_values;
    }

    std::string_view Name(Mode a_mode) {
        switch (a_mode) {
            case Mode::kSwap:
                return "Swap"sv;
            case Mode::kBounce:
                return "Bounce"sv;
            default:
                return "?"sv;
        }
    }
}
