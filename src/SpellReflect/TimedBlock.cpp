#include "SpellReflect/TimedBlock.h"

#include "SpellReflect/BlockTiming.h"
#include "SpellReflect/CallScan.h"
#include "SpellReflect/Feedback.h"
#include "SpellReflect/Forms.h"
#include "SpellReflect/Settings.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <mutex>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std::literals;

namespace SpellReflect::TimedBlock {
    namespace {
        using Clock = std::chrono::steady_clock;

        // 38627 = melee hit; it makes one `call 38586` with rcx = target,
        // rdx = &HitData (after HitData::Populate, AE 44001). The return value is
        // unused by the caller. The call is found at run time (CallScan); the
        // offsets below are only for chaining onto another plugin's hook there.
        //
        // The block's stamina cost is charged inside 38586: on 1.7.104 the only call of the
        // leaf that computes it (id 26430: percentBlocked x physicalDamage x fStaminaBlockDmgMult
        // + stagger term) is at 38586+0xB39 (exe read offline, 2026-11-04). So a hit that is not
        // applied costs no stamina, and a reduced hit's cost can be given back right after.
        constexpr std::uint64_t kMeleeHitId = 38627;
        constexpr std::uint64_t kApplyHitId = 38586;
        constexpr std::array    kApplyHitHints{
            CallScan::Hint{ { 1, 6, 1170, 0 }, 0x4A8 },  // runtime dump
            CallScan::Hint{ { 1, 7, 104, 0 }, 0x4A8 },   // exe, checked offline 2026-10-24
        };

        constexpr std::array<const char*, Settings::kBlockRanks> kRankNames{ "Apprentice", "Journeyman", "Expert", "Master" };

        std::array<RE::BGSPerk*, Settings::kBlockRanks> g_rankPerks{};
        RE::SpellItem*    g_staggerSpell = nullptr;
        bool              g_hookActive = false;
        Clock::time_point g_lastTimedBlock{};  // main thread only (melee hits)
        std::uint32_t     g_logged = 0;

        // The highest Block mastery rank the player holds, or -1.
        int RankOf(RE::PlayerCharacter* a_player) {
            for (int i = static_cast<int>(Settings::kBlockRanks) - 1; i >= 0; --i) {
                if (g_rankPerks[i] && a_player->HasPerk(g_rankPerks[i])) {
                    return i;
                }
            }
            return -1;
        }

        // The attacker staggers (Master): the stagger spell, cast by the player. On the main
        // thread through the task queue, with handles, after the hit has been processed.
        void Stagger(RE::PlayerCharacter* a_player, RE::Actor* a_aggressor) {
            const auto tasks = SKSE::GetTaskInterface();
            if (!g_staggerSpell || !tasks) {
                return;
            }
            const auto playerHandle = a_player->GetHandle();
            const auto targetHandle = a_aggressor->GetHandle();
            tasks->AddTask([playerHandle, targetHandle]() {
                const auto playerPtr = playerHandle.get();
                const auto targetPtr = targetHandle.get();
                const auto player = playerPtr.get();
                const auto target = targetPtr.get();
                if (!player || !target || target->IsDead()) {
                    return;
                }
                if (const auto caster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
                    caster->CastSpellImmediate(g_staggerSpell, false, target, 1.0f, false, 0.0f, player);
                }
            });
        }

        enum class Outcome {
            kNone,     // not a timed block: the engine applies the hit as usual
            kReduced,  // a timed block below 100%: the hit is applied with less damage
            kNegated   // a timed block at 100%: the hit is not applied at all
        };

        struct Decision {
            Outcome outcome = Outcome::kNone;
            bool    keepStamina = false;  // give back the block's stamina cost after a reduced hit
        };

        Decision Decide(RE::Actor* a_target, RE::HitData& a_hit) {
            const auto player = RE::PlayerCharacter::GetSingleton();
            if (!player || a_target != player || a_hit.flags.none(RE::HitData::Flag::kBlocked)) {
                return {};
            }
            const int rankIndex = RankOf(player);
            if (rankIndex < 0) {
                return {};
            }
            const auto aggressorPtr = a_hit.aggressor.get();
            const auto aggressor = aggressorPtr.get();
            if (!aggressor || aggressor == player) {
                return {};
            }

            const auto& s = Settings::Get();
            const auto& rank = s.blockRanks[rankIndex];
            const auto  now = Clock::now();
            const float since = BlockTiming::SecondsSinceBlockPress();
            const bool  cooling = now - g_lastTimedBlock < std::chrono::duration<float>(rank.cooldown);
            const bool  timed = since >= 0.0f && since <= s.timedBlockWindow && !cooling;
            const float damage = a_hit.totalDamage;
            const bool  negate = rank.damageReduction >= 100.0f;

            Decision decision;
            float    restored = 0.0f;
            if (timed && !s.observeOnly) {
                g_lastTimedBlock = now;
                decision.outcome = negate ? Outcome::kNegated : Outcome::kReduced;
                decision.keepStamina = !negate && rank.noStaminaCost;
                if (!negate) {
                    a_hit.totalDamage = damage * (1.0f - rank.damageReduction / 100.0f);
                }
                if (rank.staminaRestorePercent > 0.0f) {
                    const auto  av = player->AsActorValueOwner();
                    const float maxStamina = av->GetPermanentActorValue(RE::ActorValue::kStamina);
                    restored = maxStamina * rank.staminaRestorePercent / 100.0f;
                    av->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, restored);  // vfunc 06 (v12 name)
                }
                if (rank.stagger) {
                    Stagger(player, aggressor);
                }
                Feedback::Play(Feedback::Event::kTimedBlock, player);
            }
            if (g_logged < s.logEvents) {
                ++g_logged;
                const std::string result =
                    !timed ? "no"s :
                    negate ? std::format("TIMED BLOCK ({}), hit negated, +{:.0f} stamina{}", kRankNames[rankIndex], restored,
                                         rank.stagger ? ", attacker staggered" : "") :
                             std::format("TIMED BLOCK ({}), damage -{:.0f}% -> {:.1f}{}, +{:.0f} stamina{}", kRankNames[rankIndex],
                                         rank.damageReduction, damage * (1.0f - rank.damageReduction / 100.0f),
                                         rank.noStaminaCost ? ", no block stamina cost" : "", restored,
                                         rank.stagger ? ", attacker staggered" : "");
                Log::INFO("SpellReflect: blocked melee hit: {} damage {:.1f} pressed {:.3f}s ago window {:.3f}s rank {} cooldown {:.1f}s{} -> {}{}",
                          aggressor->GetDisplayFullName(), damage, since, s.timedBlockWindow, kRankNames[rankIndex], rank.cooldown,
                          cooling ? " (cooldown)" : "", result, s.observeOnly ? " (observe only)" : "");
            }
            return decision;
        }

        struct ApplyHitHook {
            static void thunk(RE::Actor* a_target, RE::HitData& a_hit) {
                const auto decision = Decide(a_target, a_hit);
                if (decision.outcome == Outcome::kNegated) {
                    return;  // negated: the hit is never applied (no damage, stagger, hit effects or block stamina cost)
                }
                if (!decision.keepStamina) {
                    func(a_target, a_hit);
                    return;
                }
                // A reduced timed block costs no stamina: the block's cost is charged inside the
                // hit-apply function, so whatever stamina it takes is given back at once.
                const auto  av = a_target->AsActorValueOwner();
                const float before = av->GetActorValue(RE::ActorValue::kStamina);
                func(a_target, a_hit);
                const float after = av->GetActorValue(RE::ActorValue::kStamina);
                if (after < before) {
                    av->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, before - after);
                }
                if (g_logged < Settings::Get().logEvents) {
                    Log::INFO("SpellReflect: timed block: block stamina cost {:.1f} given back (stamina {:.1f} -> {:.1f} -> {:.1f})",
                              before - after, before, after, av->GetActorValue(RE::ActorValue::kStamina));
                }
            }
            static inline REL::Relocation<decltype(thunk)> func;
        };
    }

    void Install() {
        if (!Settings::Get().timedBlockEnabled) {
            Log::INFO("SpellReflect: timed block: disabled in the INI");
            return;
        }
        const auto applyHit = REL::ID(kApplyHitId).address();
        const auto calls = CallScan::FindCalls(applyHit);
        Log::INFO("SpellReflect: timed block: {} direct calls of the hit-apply function", calls.size());
        const auto site = CallScan::FindCallIn("timed block", kMeleeHitId, applyHit, calls, kApplyHitHints);
        if (!site) {
            Log::ERROR("SpellReflect: timed block: OFF (melee hit call not found)");
            return;
        }
        ApplyHitHook::func = SKSE::GetTrampoline().write_call<5>(*site, ApplyHitHook::thunk);
        g_hookActive = true;
        Log::INFO("SpellReflect: timed block: hook installed");
    }

    void OnDataLoaded() {
        const auto& s = Settings::Get();
        if (!g_hookActive) {
            return;
        }
        std::size_t found = 0;
        for (std::size_t i = 0; i < Settings::kBlockRanks; ++i) {
            g_rankPerks[i] = Forms::Resolve<RE::BGSPerk>(s.blockRanks[i].perk, std::format("timed block rank {} ({})", i + 1, kRankNames[i]));
            found += g_rankPerks[i] ? 1 : 0;
        }
        if (found == 0) {
            Log::WARN("SpellReflect: timed block: OFF (no rank perk found)");
            return;
        }
        g_staggerSpell = Forms::Resolve<RE::SpellItem>(s.staggerSpell, "timed block stagger spell");
        if (!g_staggerSpell && std::ranges::any_of(s.blockRanks, [](const auto& a_rank) { return a_rank.stagger; })) {
            Log::WARN("SpellReflect: timed block: no stagger spell; the attacker is not staggered");
        }
        Log::INFO("SpellReflect: timed block: window {}s (fixed), {} of {} rank perks found", s.timedBlockWindow, found,
                  Settings::kBlockRanks);
    }
}
