#include "SpellReflect/Settings.h"

#include "Platform/IniFile.h"
#include "Platform/Text.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace std::literals;

namespace SpellReflect::Settings {
    namespace {
        Values g_values;

        // a_out keeps its value when a_value is not a number.
        template <class T>
        void ParseNumber(const std::string& a_key, const std::string& a_value, T& a_out) {
            T value{};
            if (Text::Parse(a_value, value)) {
                a_out = value;
            } else {
                Log::WARN("SpellReflect: INI: bad number '{}' for {}", a_value, a_key);
            }
        }

        // One value per timed-block rank (Apprentice, Journeyman, Expert, Master).
        template <class Fn>
        void ApplyRankList(const std::string& a_key, const std::string& a_value, Fn a_apply) {
            const auto items = Text::Split(a_value, ',', true);
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
            const auto items = Text::Split(a_value, ',', true);
            if (items.size() != kLockLevels) {
                Log::WARN("SpellReflect: INI: {} has {} values, expected {} (Novice, Apprentice, Adept, Expert, Master)",
                          a_key, items.size(), kLockLevels);
            }
            for (std::size_t i = 0; i < std::min(items.size(), kLockLevels); ++i) {
                if (!items[i].empty()) {
                    ParseNumber(a_key, items[i], a_out[i]);
                }
            }
        }

        // key is "section.name", lower-cased.
        void Apply(const std::string& a_key, const std::string& a_value) {
            auto& v = g_values;
            if (a_key == "reflect.sactorvalue") {
                v.actorValue = a_value;
            } else if (a_key == "reflect.fmaxchance") {
                ParseNumber(a_key, a_value, v.maxChance);
            } else if (a_key == "reflect.sblockingkeyword") {
                v.blockingKeyword = a_value;
            } else if (a_key == "reflect.fmaxblockingchance") {
                ParseNumber(a_key, a_value, v.maxBlockingChance);
            } else if (a_key == "reflect.smode") {
                const auto s = Text::Lower(a_value);
                if (s == "swap") {
                    v.mode = Mode::kSwap;
                } else if (s == "bounce") {
                    v.mode = Mode::kBounce;
                } else {
                    Log::WARN("SpellReflect: INI: unknown sMode '{}', keeping {}", a_value, Name(v.mode));
                }
            } else if (a_key == "reflect.sreturnaim") {
                const auto s = Text::Lower(a_value);
                if (s == "retrace") {
                    v.returnAim = Aim::kRetrace;
                } else if (s == "direct") {
                    v.returnAim = Aim::kDirect;
                } else {
                    Log::WARN("SpellReflect: INI: unknown sReturnAim '{}', keeping Retrace/Direct as set", a_value);
                }
            } else if (a_key == "reflect.breflecttouch") {
                v.reflectTouch = Text::ParseBool(a_value, v.reflectTouch);
            } else if (a_key == "reflect.breflectexplosive") {
                v.reflectExplosive = Text::ParseBool(a_value, v.reflectExplosive);
            } else if (a_key == "reflect.breflectshouts") {
                v.reflectShouts = Text::ParseBool(a_value, v.reflectShouts);
            } else if (a_key == "reflect.breflectstaffs") {
                v.reflectStaffs = Text::ParseBool(a_value, v.reflectStaffs);
            } else if (a_key == "reflect.bignorenoreflectflag") {
                v.ignoreNoReflectFlag = Text::ParseBool(a_value, v.ignoreNoReflectFlag);
            } else if (a_key == "arrows.benabled") {
                v.arrowsEnabled = Text::ParseBool(a_value, v.arrowsEnabled);
            } else if (a_key == "arrows.ballowwithshield") {
                v.arrowAllowShield = Text::ParseBool(a_value, v.arrowAllowShield);
            } else if (a_key == "arrows.sperk") {
                v.arrowPerk = a_value;
            } else if (a_key == "arrows.sshieldperk") {
                v.arrowShieldPerk = a_value;
            } else if (a_key == "arrows.swindowactorvalue") {
                v.windowActorValue = a_value;
            } else if (a_key == "arrows.fwindowbase") {
                ParseNumber(a_key, a_value, v.windowBase);
            } else if (a_key == "arrows.fwindowperpoint") {
                ParseNumber(a_key, a_value, v.windowPerPoint);
            } else if (a_key == "arrows.fwindowmax") {
                ParseNumber(a_key, a_value, v.windowMax);
            } else if (a_key == "arrows.ffacingangle") {
                ParseNumber(a_key, a_value, v.facingAngle);
            } else if (a_key == "arrows.fcooldown") {
                ParseNumber(a_key, a_value, v.arrowCooldown);
            } else if (a_key == "spellparry.benabled") {
                v.parryEnabled = Text::ParseBool(a_value, v.parryEnabled);
            } else if (a_key == "spellparry.sperk") {
                v.parryPerk = a_value;
            } else if (a_key == "spellparry.bparryexplosive") {
                v.parryExplosive = Text::ParseBool(a_value, v.parryExplosive);
            } else if (a_key == "timedblock.benabled") {
                v.timedBlockEnabled = Text::ParseBool(a_value, v.timedBlockEnabled);
            } else if (a_key == "timedblock.fwindow") {
                ParseNumber(a_key, a_value, v.timedBlockWindow);
            } else if (a_key == "timedblock.srankperks") {
                ApplyRankList(a_key, a_value, [](BlockRank& a_rank, const std::string& a_item) { a_rank.perk = a_item; });
            } else if (a_key == "timedblock.fdamagereduction") {
                ApplyRankList(a_key, a_value, [&](BlockRank& a_rank, const std::string& a_item) { ParseNumber(a_key, a_item, a_rank.damageReduction); });
            } else if (a_key == "timedblock.fcooldown") {
                ApplyRankList(a_key, a_value, [&](BlockRank& a_rank, const std::string& a_item) { ParseNumber(a_key, a_item, a_rank.cooldown); });
            } else if (a_key == "timedblock.fstaminarestorepercent") {
                ApplyRankList(a_key, a_value, [&](BlockRank& a_rank, const std::string& a_item) { ParseNumber(a_key, a_item, a_rank.staminaRestorePercent); });
            } else if (a_key == "timedblock.bnostaminacost") {
                ApplyRankList(a_key, a_value, [](BlockRank& a_rank, const std::string& a_item) { a_rank.noStaminaCost = Text::ParseBool(a_item, a_rank.noStaminaCost); });
            } else if (a_key == "timedblock.bstagger") {
                ApplyRankList(a_key, a_value, [](BlockRank& a_rank, const std::string& a_item) { a_rank.stagger = Text::ParseBool(a_item, a_rank.stagger); });
            } else if (a_key == "timedblock.sstaggerspell") {
                v.staggerSpell = a_value;
            } else if (a_key == "bruteforce.benabled") {
                v.bruteEnabled = Text::ParseBool(a_value, v.bruteEnabled);
            } else if (a_key == "bruteforce.srankperks") {
                const auto items = Text::Split(a_value, ',', true);
                if (items.size() != kBruteRanks) {
                    Log::WARN("SpellReflect: INI: {} has {} values, expected {}", a_key, items.size(), kBruteRanks);
                }
                for (std::size_t i = 0; i < std::min(items.size(), kBruteRanks); ++i) {
                    v.bruteRankPerks[i] = items[i];
                }
            } else if (a_key == "bruteforce.fchancerank1" || a_key == "bruteforce.fchancerank2" || a_key == "bruteforce.fchancerank3") {
                ParseLockTable(a_key, a_value, v.bruteChance[static_cast<std::size_t>(a_key.back() - '1')]);
            } else if (a_key == "bruteforce.ballowunarmed") {
                v.bruteAllowUnarmed = Text::ParseBool(a_value, v.bruteAllowUnarmed);
            } else if (a_key == "bruteforce.bdoors") {
                v.bruteDoors = Text::ParseBool(a_value, v.bruteDoors);
            } else if (a_key == "bruteforce.inoiselevel") {
                ParseNumber(a_key, a_value, v.bruteNoiseLevel);
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
                v.bruteOpenOnSuccess = Text::ParseBool(a_value, v.bruteOpenOnSuccess);
            } else if (a_key == "bruteforce.fattemptgap") {
                ParseNumber(a_key, a_value, v.bruteAttemptGap);
            } else if (a_key == "bruteforce.fcrimerepeatseconds") {
                ParseNumber(a_key, a_value, v.bruteCrimeRepeat);
            } else if (a_key == "bruteforce.ssuccessmessage") {
                v.bruteSuccessMessage = a_value;
            } else if (a_key == "bruteforce.sfailmessage") {
                v.bruteFailMessage = a_value;
            } else if (a_key == "bruteforce.stoostrongmessage") {
                v.bruteTooStrongMessage = a_value;
            } else if (a_key == "bruteforce.bhitframe") {
                v.bruteHitFrame = Text::ParseBool(a_value, v.bruteHitFrame);
            } else if (a_key == "bruteforce.freach") {
                ParseNumber(a_key, a_value, v.bruteReach);
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
                ParseNumber(a_key, a_value, v.visualDuration);
            } else if (a_key == "debug.bobserveonly") {
                v.observeOnly = Text::ParseBool(a_value, v.observeOnly);
            } else if (a_key == "debug.ilogevents") {
                ParseNumber(a_key, a_value, v.logEvents);
            } else {
                Log::WARN("SpellReflect: INI: unknown key '{}'", a_key);
            }
        }
    }

    void Load() {
        constexpr auto path = "Data/SKSE/Plugins/SKYBSpellReflect.ini";
        const bool read = IniFile::Read(path, "SpellReflect: INI", [](const IniFile::Entry& a_entry) {
            Apply(Text::Lower(a_entry.section + "." + a_entry.key), a_entry.value);
        });
        if (!read) {
            Log::INFO("SpellReflect: INI: {} not found, using defaults", path);
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
        Log::INFO("SpellReflect: settings: brute force hitFrame={} reach={}", v.bruteHitFrame, v.bruteReach);
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
