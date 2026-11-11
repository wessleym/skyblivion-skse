#include "SpellReflect/BruteForce.h"

#include "SpellReflect/Forms.h"
#include "SpellReflect/Settings.h"

#include "Degradation/Condition.h"
#include "Degradation/DecayService.h"
#include "Degradation/Settings.h"
#include "Platform/Text.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

using namespace std::literals;

namespace SpellReflect::BruteForce {
    namespace {
        using Clock = std::chrono::steady_clock;

        constexpr std::array<const char*, Settings::kLockLevels + 1> kLockNames{ "Novice", "Apprentice", "Adept", "Expert",
                                                                                 "Master", "key only" };

        std::array<RE::BGSPerk*, Settings::kBruteRanks> g_rankPerks{};
        std::uint32_t                                   g_logged = 0;

        // last attempt and last crime report per container (hit events and the hit-frame task: one at a time)
        std::unordered_map<RE::FormID, Clock::time_point> g_lastAttempt;
        std::unordered_map<RE::FormID, Clock::time_point> g_lastCrime;

        bool Within(std::unordered_map<RE::FormID, Clock::time_point>& a_map, RE::FormID a_id, float a_seconds,
                    Clock::time_point a_now) {
            if (a_map.size() > 64) {
                std::erase_if(a_map, [&](const auto& a_entry) { return a_now - a_entry.second > 60s; });
            }
            const auto it = a_map.find(a_id);
            return it != a_map.end() && a_now - it->second < std::chrono::duration<float>(a_seconds);
        }

        int RankOf(RE::PlayerCharacter* a_player) {
            for (int i = static_cast<int>(Settings::kBruteRanks) - 1; i >= 0; --i) {
                if (g_rankPerks[i] && a_player->HasPerk(g_rankPerks[i])) {
                    return i;
                }
            }
            return -1;
        }

        const char* LockText(RE::TESObjectREFR* a_ref) {
            const auto level = std::to_underlying(a_ref->GetLockLevel());
            return level >= 0 && level <= static_cast<int>(Settings::kLockLevels) ? kLockNames[static_cast<std::size_t>(level)] : "none";
        }

        // A locked container, or a locked door unless [BruteForce] bDoors = 0.
        bool CanForce(RE::TESObjectREFR* a_target) {
            const auto base = a_target->GetBaseObject();
            const bool lockable = base && (base->Is(RE::FormType::Container) || (base->Is(RE::FormType::Door) && Settings::Get().bruteDoors));
            return lockable && a_target->IsLocked();
        }

        // A melee weapon that may force locks (fists only with bAllowUnarmed).
        bool CanForceWith(const RE::TESObjectWEAP* a_weapon) {
            return a_weapon && a_weapon->IsMelee() && (!a_weapon->IsHandToHandMelee() || Settings::Get().bruteAllowUnarmed);
        }

        float Roll() {
            static std::mt19937 gen{ std::random_device{}() };
            return std::uniform_real_distribution<float>(0.0f, 100.0f)(gen);
        }

        void Message(const std::string& a_text) {
            if (!a_text.empty()) {
                RE::SendHUDMessage::ShowHUDMessage(a_text.c_str());
            }
        }

        // The noise: what Papyrus CreateDetectionEvent does, the player's process sets a detection
        // event at the container's position.
        void MakeNoise(RE::PlayerCharacter* a_player, RE::TESObjectREFR* a_container, std::int32_t a_level) {
            const auto process = a_player->GetActorRuntimeData().currentProcess;
            if (process && a_level > 0) {
                process->SetActorsDetectionEvent(a_player, a_container->GetPosition(), a_level, a_container);
            }
        }

        // The crime: as lockpicking reports it (1.7.104 id 51981) - an owned lock, the player
        // detected by someone, then the trespass alarm. Returns what happened, for the log.
        const char* ReportCrime(RE::PlayerCharacter* a_player, RE::TESObjectREFR* a_container, Clock::time_point a_now) {
            const auto owner = a_container->GetOwner();
            if (!owner) {
                return "not owned";
            }
            if (owner == a_player->GetActorBase()) {
                return "the player's own";
            }
            const auto lists = RE::ProcessLists::GetSingleton();
            if (!lists) {
                return "no process lists";
            }
            std::uint32_t losCount = 1;
            const auto    level = lists->RequestHighestDetectionLevelAgainstActor(a_player, losCount);
            if (level <= 0) {
                return "not seen";
            }
            const auto& s = Settings::Get();
            if (Within(g_lastCrime, a_container->GetFormID(), s.bruteCrimeRepeat, a_now)) {
                return "seen, already reported";
            }
            g_lastCrime[a_container->GetFormID()] = a_now;
            a_player->TrespassAlarm(a_container, owner, -1);
            return "SEEN, trespass alarm";
        }

        // Extra wear on the weapon that struck, through the degradation service (when it is on).
        // a_left: the hand that struck when the caller knows it (the hit-frame path reads it off the attack
        // event); otherwise the hand holding the weapon, and the right one when both hold the same weapon.
        float Wear(RE::PlayerCharacter* a_player, RE::TESObjectWEAP* a_weapon, std::size_t a_lockLevel, bool a_success,
                   std::optional<bool> a_left) {
            using SKYBDeg::G;
            if (!SKYBDeg::Settings::On(G::Enabled)) {
                return -1.0f;
            }
            const bool left = a_left ? *a_left :
                                       a_player->GetEquippedObject(true) == a_weapon && a_player->GetEquippedObject(false) != a_weapon;
            const auto list = SKYBDeg::Condition::FindWornList(a_weapon, left);
            if (!list) {
                return -1.0f;
            }
            const float rate = SKYBDeg::Settings::Get(G::WeapRate) * SKYBDeg::Settings::Get(G::WeapRateMult) *
                               (a_success ? Settings::Get().bruteWearMultSuccess : Settings::Get().bruteWearMultFail)[a_lockLevel];
            return SKYBDeg::DecayService::Degrade(a_weapon, list, rate, "brute force");
        }

        // The sounds the player hears at the lock ([BruteForce] sSuccessSound / sFailSound).
        RE::BGSSoundDescriptorForm* g_successSound = nullptr;
        RE::BGSSoundDescriptorForm* g_failSound = nullptr;

        // Queued to the main thread (hit events and graph tasks may come from elsewhere).
        void PlaySoundAt(RE::BGSSoundDescriptorForm* a_sound, RE::TESObjectREFR* a_lock) {
            const auto tasks = SKSE::GetTaskInterface();
            if (!a_sound || !a_lock || !tasks) {
                return;
            }
            tasks->AddTask([a_sound, handle = a_lock->GetHandle()]() {
                const auto ref = REBridge::Lookup(handle);
                const auto audio = RE::BSAudioManager::GetSingleton();
                if (!ref || !audio) {
                    return;
                }
                RE::BSSoundHandle sound;
                if (!audio->GetSoundHandle(sound, a_sound)) {
                    return;
                }
                sound.SetPosition(ref->GetPosition());
                if (const auto root = ref->Get3D()) {
                    sound.SetObjectToFollow(root);
                }
                sound.Play();
            });
        }

        // Nothing within fAttemptGap of the last attempt on this container (the other hand, the
        // same swing again, or the swing the other path already counted). a_left: the striking hand
        // when known (see Wear).
        void Attempt(RE::PlayerCharacter* a_player, RE::TESObjectREFR* a_container, RE::TESObjectWEAP* a_weapon, int a_rank,
                     std::string_view a_via, std::optional<bool> a_left) {
            const auto& s = Settings::Get();
            const auto  now = Clock::now();
            const auto  id = a_container->GetFormID();
            if (Within(g_lastAttempt, id, s.bruteAttemptGap, now)) {
                return;
            }
            g_lastAttempt[id] = now;
            if (g_logged < s.logEvents) {
                Log::INFO("SpellReflect: brute force: attempt from the {}", a_via);
            }

            const auto base = a_container->GetBaseObject();
            const bool isDoor = base && base->Is(RE::FormType::Door);
            const auto lockLevel = std::to_underlying(a_container->GetLockLevel());
            const bool forceable = lockLevel >= 0 && static_cast<std::size_t>(lockLevel) < Settings::kLockLevels;
            const float chance = forceable ? s.bruteChance[a_rank][static_cast<std::size_t>(lockLevel)] : 0.0f;
            const float roll = chance > 0.0f ? Roll() : 100.0f;
            const bool  success = chance > 0.0f && roll < chance && !s.observeOnly;

            if (success) {
                if (const auto lock = a_container->GetLock()) {
                    lock->SetLocked(false);
                    REBridge::AddLockChange(a_container);
                }
                Message(s.bruteSuccessMessage);
                PlaySoundAt(g_successSound, a_container);
            } else {
                Message(chance > 0.0f ? s.bruteFailMessage : s.bruteTooStrongMessage);
                PlaySoundAt(g_failSound, a_container);
            }

            std::string crime = "observe only";
            float       condition = -1.0f;
            if (!s.observeOnly) {
                MakeNoise(a_player, a_container, s.bruteNoiseLevel);
                crime = ReportCrime(a_player, a_container, now);
                if (forceable) {
                    condition = Wear(a_player, a_weapon, static_cast<std::size_t>(lockLevel), success, a_left);
                }
            }

            if (g_logged < s.logEvents) {
                ++g_logged;
                const auto name = a_container->GetDisplayFullName();
                Log::INFO("SpellReflect: brute force: {} '{}' {:08X} lock {} rank {} weapon '{}' chance {:.0f}% roll {:.1f} -> {}; noise {}; crime: {}; weapon condition {}",
                          isDoor ? "door" : "container", name ? name : "", id, LockText(a_container),
                          a_rank + 1, a_weapon->GetName(), chance, roll, success ? "UNLOCKED" : (chance > 0.0f ? "held" : "too strong"),
                          s.bruteNoiseLevel, crime, condition < 0.0f ? "unchanged (degradation off)"s : std::format("{:.1f}%", condition * 100.0f));
            }

            // containers only: activating a load door would carry the player through it mid-fight
            if (success && s.bruteOpenOnSuccess && !isDoor) {
                a_container->ActivateRef(a_player, 0, nullptr, 0, false);
            }
        }

        class HitSink final : public RE::BSTEventSink<RE::TESHitEvent> {
        public:
            static HitSink* GetSingleton() {
                static HitSink singleton;
                return &singleton;
            }

            RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override {
                if (!a_event || !Settings::Get().bruteEnabled) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                // the player striking something that is not an actor: the only hits Brute Force reads
                const auto player = RE::PlayerCharacter::GetSingleton();
                const auto cause = a_event->cause.get();
                const auto target = a_event->target.get();
                if (!player || cause != player || !target || target == player || target->As<RE::Actor>()) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                // the weapon's own event (an enchantment's or poison's follow-up names the ENCH / ALCH)
                const auto weapon = RE::TESForm::LookupByID<RE::TESObjectWEAP>(a_event->source);
                if (a_event->projectile != 0 || a_event->flags.none(RE::TESHitEvent::Flag::kPowerAttack) ||
                    !CanForceWith(weapon) || !CanForce(target)) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                if (const int rank = RankOf(player); rank >= 0) {
                    // the hit event names no hand: Wear reads it off the equipped weapons
                    Attempt(player, target, weapon, rank, "hit event", std::nullopt);
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        // ---- the hit-frame path: needs no TESHitEvent on the container -----------------------
        // The player's animation graphs send "HitFrame" when a swing lands. During a power attack
        // (the high process's attack data carries kPowerAttack) the crosshair reference within
        // fReach is the target. Graph events can arrive off the main thread: the sink only stamps
        // the time and queues the rest as an SKSE task.

        RE::ObjectRefHandle     g_crosshair;  // main thread: SKSE's crosshair event and the hit-frame task
        std::atomic<Clock::rep> g_lastHitFrame{ 0 };
        constexpr auto          kSameSwing = 150ms;  // one swing reaches both player graphs (first and third person)

        bool ContainsNoCase(std::string_view a_text, std::string_view a_part) {
            return Text::Lower(a_text).find(Text::Lower(a_part)) != std::string::npos;
        }

        RE::TESObjectWEAP* StrikingWeapon(RE::PlayerCharacter* a_player, bool a_left) {
            if (const auto form = a_player->GetEquippedObject(a_left)) {
                return form->As<RE::TESObjectWEAP>();
            }
            return RE::TESForm::LookupByID<RE::TESObjectWEAP>(0x1F4);  // Unarmed (fists)
        }

        void OnHitFrame() {
            const auto& s = Settings::Get();
            const auto  player = RE::PlayerCharacter::GetSingleton();
            if (!player || !s.bruteEnabled || !s.bruteHitFrame) {
                return;
            }
            const auto             process = player->GetActorRuntimeData().currentProcess;
            const auto             high = process ? process->high : nullptr;
            const auto             attack = high ? high->attackData.get() : nullptr;
            const std::string_view attackEvent = attack && attack->event.c_str() ? attack->event.c_str() : "";
            const bool             left = ContainsNoCase(attackEvent, "lefthand");  // "attackPowerStartLeft" is the right hand
            const auto             target = REBridge::Lookup(g_crosshair);
            if (!attack || attack->data.flags.none(RE::AttackData::AttackFlag::kPowerAttack) || !target ||
                target->As<RE::Actor>() || !CanForce(target.get()) ||
                player->GetPosition().GetDistance(target->GetPosition()) > s.bruteReach) {
                return;
            }
            const auto weapon = StrikingWeapon(player, left);
            const int  rank = RankOf(player);
            if (CanForceWith(weapon) && rank >= 0) {
                Attempt(player, target.get(), weapon, rank, "hit frame", left);
            }
        }

        class GraphSink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
        public:
            static GraphSink* GetSingleton() {
                static GraphSink singleton;
                return &singleton;
            }

            RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override {
                if (!a_event || !a_event->holder || !a_event->holder->IsPlayerRef() || a_event->tag != "HitFrame"sv) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                const auto now = Clock::now().time_since_epoch().count();
                auto       last = g_lastHitFrame.load();
                if (now - last < std::chrono::duration_cast<Clock::duration>(kSameSwing).count() ||
                    !g_lastHitFrame.compare_exchange_strong(last, now)) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                SKSE::GetTaskInterface()->AddTask([] { OnHitFrame(); });
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        // The player's graphs are rebuilt with the player's 3D (loading a game, a new race): add the
        // sink again whenever a loading screen or the race menu closes. Adding it twice is a no-op.
        void AddGraphSink() {
            const auto                     player = RE::PlayerCharacter::GetSingleton();
            RE::BSAnimationGraphManagerPtr manager;
            if (!player || !player->GetAnimationGraphManager(manager) || !manager) {
                Log::WARN("SpellReflect: brute force: no player animation graph yet; the hit-frame path waits for the next loading screen");
                return;
            }
            const auto  sink = GraphSink::GetSingleton();
            std::size_t added = 0;
            std::size_t total = 0;
            for (const auto& graph : manager->graphs) {
                if (!graph) {
                    continue;
                }
                ++total;
                const auto source = graph->GetEventSource<RE::BSAnimationGraphEvent>();
                if (std::ranges::find(source->sinks, sink) == source->sinks.end()) {
                    source->AddEventSink(sink);
                    ++added;
                }
            }
            if (added > 0) {
                Log::INFO("SpellReflect: brute force: hit-frame sink added to {} of {} player animation graph(s)", added, total);
            }
        }

        class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
        public:
            static MenuSink* GetSingleton() {
                static MenuSink singleton;
                return &singleton;
            }

            RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
                if (a_event && !a_event->opening &&
                    (a_event->menuName == RE::LoadingMenu::MENU_NAME || a_event->menuName == RE::RaceSexMenu::MENU_NAME)) {
                    SKSE::GetTaskInterface()->AddTask([] { AddGraphSink(); });
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        class CrosshairSink final : public RE::BSTEventSink<SKSE::CrosshairRefEvent> {
        public:
            static CrosshairSink* GetSingleton() {
                static CrosshairSink singleton;
                return &singleton;
            }

            RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* a_event, RE::BSTEventSource<SKSE::CrosshairRefEvent>*) override {
                g_crosshair = a_event && a_event->crosshairRef ? a_event->crosshairRef->GetHandle() : RE::ObjectRefHandle{};
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        void InstallHitFrame() {
            if (!Settings::Get().bruteHitFrame) {
                Log::INFO("SpellReflect: brute force: hit-frame path off (bHitFrame = 0)");
                return;
            }
            const auto crosshair = SKSE::GetCrosshairRefEventSource();
            const auto ui = RE::UI::GetSingleton();
            if (!crosshair || !ui) {
                Log::WARN("SpellReflect: brute force: hit-frame path OFF (no {} event source)", crosshair ? "menu" : "crosshair");
                return;
            }
            crosshair->AddEventSink(CrosshairSink::GetSingleton());
            ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuSink::GetSingleton());
            Log::INFO("SpellReflect: brute force: hit-frame path on (power-attack HitFrame on the crosshair reference within {:.0f} units)",
                      Settings::Get().bruteReach);
        }
    }

    void OnDataLoaded() {
        const auto& s = Settings::Get();
        if (!s.bruteEnabled) {
            Log::INFO("SpellReflect: brute force: disabled in the INI");
            return;
        }
        std::size_t found = 0;
        for (std::size_t i = 0; i < Settings::kBruteRanks; ++i) {
            g_rankPerks[i] = Forms::Resolve<RE::BGSPerk>(s.bruteRankPerks[i], std::format("brute force rank {}", i + 1));
            found += g_rankPerks[i] ? 1 : 0;
        }
        if (found == 0) {
            Log::WARN("SpellReflect: brute force: OFF (no rank perk found)");
            return;
        }
        g_successSound = Forms::Resolve<RE::BGSSoundDescriptorForm>(s.bruteSuccessSound, "brute force sound, forced");
        g_failSound = Forms::Resolve<RE::BGSSoundDescriptorForm>(s.bruteFailSound, "brute force sound, held");
        const auto holder = RE::ScriptEventSourceHolder::GetSingleton();
        if (!holder) {
            Log::ERROR("SpellReflect: brute force: OFF (no event source holder)");
            return;
        }
        holder->AddEventSink<RE::TESHitEvent>(HitSink::GetSingleton());
        Log::INFO("SpellReflect: brute force: listening for hit events ({} of {} rank perks found)", found, Settings::kBruteRanks);
        InstallHitFrame();
    }
}
