#include "SpellReflect/Reflect.h"

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
#include <format>
#include <functional>
#include <mutex>
#include <optional>
#include <random>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>

using namespace std::literals;

// Hook points and the swap-on-apply / flag-at-hit approach follow MSami2003's
// ReflectSpell (MIT, see THIRD_PARTY_NOTICES.md). Differences: a vtable hook
// and verified call-site hooks instead of prologue detours, one roll per
// spell cast instead of per effect, no global state cleared on every hit,
// no re-reflection, and the projectile is turned around instead of relaunched.

namespace SpellReflect::Reflect {
    namespace {
        using Clock = std::chrono::steady_clock;

        // ---- engine addresses (AE 1.6.1170, checked against the runtime dump) ----
        constexpr REL::RelocationID kAddTargetImpl{ 37832, 38786 };  // Actor's MagicTarget::AddTarget
        // Projectile hit handler. VR's address library has no 43014: the VR offset was
        // matched against SE 1.5.97 in the decrypted exes (identical code, 2026-11-05).
        constexpr REL::VariantID    kHitHandler{ 43014, 44205, 0x77D940 };

        struct AeCallSite {
            std::uint64_t  callerId;  // AE id of the function holding the call
            std::ptrdiff_t offset16;  // offset of the E8 inside it on 1.6.1170 (runtime dump)
            std::ptrdiff_t offset17;  // ... on 1.7.104 (exe, checked offline 2026-10-24)
        };
        // Every direct call of 44205 (1.6.1170 and 1.7.104 alike). 44204 =
        // Projectile::HandleHits (arrow/missile/beam/flame/grenade/barrier), 43802 =
        // ConeProjectile's. The calls are found at run time (CallScan); the offsets
        // are only for chaining onto another plugin's hook there.
        constexpr std::array kAeHitCallSites{
            AeCallSite{ 43716, 0x192, 0x192 },
            AeCallSite{ 43731, 0x17E, 0x17E },
            AeCallSite{ 43802, 0x6FF, 0x721 },
            AeCallSite{ 44166, 0x4DE, 0x4DE },
            AeCallSite{ 44204, 0x21F, 0x239 },
        };

        struct SeCallSite {
            std::uint64_t  callerId;  // SE id of the function holding the call
            std::uint64_t  vrOffset;  // that function on VR 1.4.15 (most are not in VR's address library)
            std::ptrdiff_t offset;    // offset of the E8 inside it, the same on 1.5.97 and VR 1.4.15
        };
        // Every direct call of 43014 on SE 1.5.97 and VR 1.4.15: four, not AE's five (AE
        // split 42567 in two). Read from the decrypted exes 2026-11-05; VR's code matches SE's.
        // 43013 = Projectile::HandleHits, 42634 = ConeProjectile's.
        constexpr std::array kSeHitCallSites{
            SeCallSite{ 42567, 0x75DF30, 0x13A },
            SeCallSite{ 42634, 0x760670, 0x727 },
            SeCallSite{ 42983, 0x77A970, 0x395 },
            SeCallSite{ 43013, 0x77D510, 0x251 },
        };
        constexpr std::size_t kMaxHitCallSites = std::max(kAeHitCallSites.size(), kSeHitCallSites.size());

        constexpr auto  kDecisionLife = 250ms;  // one cast's effects arrive within this
        constexpr auto  kRedirectLife = 10s;    // how long a turned projectile is remembered
        constexpr float kGravity = 686.0f;      // 9.8 m/s^2 in game units (70 units = 1 m)

        // ---- resolved at kDataLoaded ----
        RE::ActorValue g_reflectAV = RE::ActorValue::kNone;
        RE::BGSPerk*   g_arrowPerk = nullptr;
        RE::BGSPerk*   g_arrowShieldPerk = nullptr;  // second Reflect Arrows rank: a shield block reflects too
        RE::BGSPerk*   g_parryPerk = nullptr;        // Reflect Spells: spell projectiles under the arrow rules
        std::atomic<Clock::rep> g_lastArrowReflect{ 0 };  // clock ticks; 0 = never
        RE::BGSKeyword* g_blockingKeyword = nullptr;
        bool           g_hitHookActive = false;

        std::atomic<std::uint32_t> g_logged{ 0 };
        std::atomic<std::uint32_t> g_wardLogged{ 0 };  // powerless ward hits passed to the body

        bool ShouldLog() {
            return g_logged.fetch_add(1, std::memory_order_relaxed) < Settings::Get().logEvents;
        }

        const char* NameOf(const RE::TESForm* a_form) {
            if (!a_form) {
                return "<none>";
            }
            const auto name = a_form->GetName();
            return name && *name ? name : "<unnamed>";
        }

        // References carry their name on the base object.
        const char* NameOf(RE::TESObjectREFR* a_ref) {
            if (!a_ref) {
                return "<none>";
            }
            const auto name = a_ref->GetDisplayFullName();
            return name && *name ? name : "<unnamed>";
        }

        float Roll() {
            thread_local std::mt19937 gen{ std::random_device{}() };
            std::uniform_real_distribution<float> dist(0.0f, 100.0f);
            return dist(gen);
        }

        // Magnitude of the actor's active while-blocking effects (the shield enchantment).
        float BlockingBonus(RE::Actor* a_actor) {
            if (!g_blockingKeyword || !a_actor->IsBlocking()) {
                return 0.0f;
            }
            const auto list = a_actor->AsMagicTarget()->GetActiveEffectList();
            if (!list) {
                return 0.0f;
            }
            float bonus = 0.0f;
            for (const auto effect : *list) {
                if (!effect || effect->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) {
                    continue;
                }
                const auto base = effect->GetBaseObject();
                if (base && base->HasKeyword(g_blockingKeyword)) {
                    bonus += effect->magnitude;
                }
            }
            return std::max(bonus, 0.0f);
        }

        // A reflect effect: modifies the reflect actor value (what the ESP turns Reflect Spell into).
        bool IsReflectEffect(const RE::EffectSetting* a_base) {
            return a_base && a_base->data.primaryAV == g_reflectAV &&
                   (a_base->HasArchetype(RE::EffectSetting::Archetype::kValueModifier) ||
                    a_base->HasArchetype(RE::EffectSetting::Archetype::kPeakValueModifier));
        }

        // The effect is live on the actor (then its magnitude is already in the actor value).
        bool HasLiveEffect(RE::Actor* a_actor, const RE::MagicItem* a_spell, const RE::Effect* a_effect) {
            const auto list = a_actor->AsMagicTarget()->GetActiveEffectList();
            if (!list) {
                return false;
            }
            return std::ranges::any_of(*list, [&](const RE::ActiveEffect* a_active) {
                return a_active && a_active->spell == a_spell && a_active->effect == a_effect &&
                       a_active->flags.none(RE::ActiveEffect::Flag::kDispelled);
            });
        }

        // Reflect from worn constant-effect enchantments that is NOT in the actor value.
        // SKYBNovemberUpdate.esp points the jewelry enchantments (Constant Effect) at
        // SKYBEnchReflectSpellFFSelfConstant 14940C, a Fire and Forget effect. The casting
        // types do not match, so the engine does not keep the effect on the wearer and the
        // actor value stays 0. The worn item's own magnitude counts instead; an effect the
        // engine does keep is skipped here, so nothing counts twice.
        float WornEnchantmentReflect(RE::Actor* a_actor) {
            if (g_reflectAV == RE::ActorValue::kNone) {
                return 0.0f;
            }
            const auto changes = a_actor->GetInventoryChanges(true);
            if (!changes || !changes->entryList) {
                return 0.0f;
            }
            float total = 0.0f;
            for (const auto entry : *changes->entryList) {
                if (!entry || !entry->extraLists) {
                    continue;
                }
                const auto object = entry->GetObject();
                const auto enchantable = object ? object->As<RE::TESEnchantableForm>() : nullptr;
                for (const auto xList : *entry->extraLists) {
                    if (!xList || (!xList->HasType<RE::ExtraWorn>() && !xList->HasType<RE::ExtraWornLeft>())) {
                        continue;
                    }
                    const auto extra = xList->GetByType<RE::ExtraEnchantment>();
                    const auto ench = extra && extra->enchantment ? extra->enchantment :
                                      enchantable                 ? enchantable->formEnchanting :
                                                                    nullptr;
                    if (!ench || ench->GetCastingType() != RE::MagicSystem::CastingType::kConstantEffect) {
                        continue;
                    }
                    for (const auto effect : ench->effects) {
                        if (effect && IsReflectEffect(effect->baseEffect) && !HasLiveEffect(a_actor, ench, effect)) {
                            total += effect->effectItem.magnitude;
                        }
                    }
                }
            }
            return std::max(total, 0.0f);
        }

        struct Chance {
            float value;    // what is rolled against
            float av;       // the reflect actor value
            float worn;     // worn enchantments not in the actor value
            float blocking; // a shield's while-blocking bonus
        };

        // Always-on sources (actor value + worn enchantments) are capped at fMaxChance;
        // a shield's bonus while blocking goes on top, up to fMaxBlockingChance.
        Chance ReflectChance(RE::Actor* a_actor) {
            if (g_reflectAV == RE::ActorValue::kNone) {
                return {};
            }
            const auto& s = Settings::Get();
            Chance c{};
            c.av = REBridge::AVOwner(a_actor)->GetActorValue(g_reflectAV);
            c.worn = WornEnchantmentReflect(a_actor);
            c.blocking = BlockingBonus(a_actor);
            const float always = std::clamp(c.av + c.worn, 0.0f, s.maxChance);
            c.value = c.blocking > 0.0f ? std::min(always + c.blocking, s.maxBlockingChance) : always;
            return c;
        }

        // ---- decisions: one roll per spell cast per target ----
        struct Key {
            RE::FormID            caster;
            RE::FormID            target;
            const RE::MagicItem*  spell;

            bool operator==(const Key&) const = default;
        };

        struct KeyHash {
            std::size_t operator()(const Key& a_key) const noexcept {
                std::size_t h = std::hash<RE::FormID>{}(a_key.caster);
                h = h * 31 + std::hash<RE::FormID>{}(a_key.target);
                h = h * 31 + std::hash<const void*>{}(a_key.spell);
                return h;
            }
        };

        struct Decision {
            bool              reflect;
            Clock::time_point when;
        };

        struct Redirected {
            RE::FormID        reflector;
            Clock::time_point when;
        };

        std::mutex                                     g_lock;
        std::unordered_map<Key, Decision, KeyHash>     g_decisions;   // guarded by g_lock
        std::unordered_map<RE::FormID, Redirected>     g_redirected;  // projectile -> reflector, guarded by g_lock

        void Remember(const Key& a_key, bool a_reflect) {
            const auto now = Clock::now();
            std::scoped_lock lock{ g_lock };
            if (g_decisions.size() > 64) {
                std::erase_if(g_decisions, [&](const auto& a_entry) { return now - a_entry.second.when > kDecisionLife; });
            }
            g_decisions[a_key] = { a_reflect, now };
        }

        std::optional<bool> Recall(const Key& a_key) {
            std::scoped_lock lock{ g_lock };
            const auto it = g_decisions.find(a_key);
            if (it == g_decisions.end() || Clock::now() - it->second.when > kDecisionLife) {
                return std::nullopt;
            }
            return it->second.reflect;
        }

        void MarkRedirected(RE::FormID a_projectile, RE::FormID a_reflector) {
            const auto now = Clock::now();
            std::scoped_lock lock{ g_lock };
            if (g_redirected.size() > 64) {
                std::erase_if(g_redirected, [&](const auto& a_entry) { return now - a_entry.second.when > kRedirectLife; });
            }
            g_redirected[a_projectile] = { a_reflector, now };
        }

        // The reflector of a projectile we turned around, if any. Projectile
        // reference IDs are RECYCLED (round 5 log: FF0008BF reused 39 s later),
        // so the entry only counts while the projectile's shooter is still the
        // reflector; a new arrow wearing an old ID drops the stale entry.
        std::optional<RE::FormID> RedirectedBy(RE::Projectile* a_proj) {
            const auto shooterRef = REBridge::Lookup(a_proj->GetProjectileRuntimeData().shooter);
            const auto shooter = shooterRef ? shooterRef->GetFormID() : 0;
            std::scoped_lock lock{ g_lock };
            const auto it = g_redirected.find(a_proj->GetFormID());
            if (it == g_redirected.end()) {
                return std::nullopt;
            }
            if (Clock::now() - it->second.when > kRedirectLife || it->second.reflector != shooter) {
                g_redirected.erase(it);
                return std::nullopt;
            }
            return it->second.reflector;
        }

        // Spells, scrolls, powers and (by default) staffs; never weapon
        // enchantments, poisons, potions, abilities or diseases.
        bool IsReflectableItem(const RE::MagicItem* a_item) {
            if (!a_item || a_item->hostileCount <= 0) {
                return false;
            }
            const auto& s = Settings::Get();
            if (!s.ignoreNoReflectFlag && a_item->GetNoAbsorb()) {
                return false;  // the record's "No Absorb/Reflect" flag
            }
            switch (a_item->GetSpellType()) {
                case RE::MagicSystem::SpellType::kSpell:
                case RE::MagicSystem::SpellType::kScroll:
                case RE::MagicSystem::SpellType::kLesserPower:
                case RE::MagicSystem::SpellType::kPower:
                    return true;
                case RE::MagicSystem::SpellType::kVoicePower:
                    return s.reflectShouts;
                case RE::MagicSystem::SpellType::kStaffEnchantment:
                    return s.reflectStaffs;
                default:
                    return false;
            }
        }

        // An area spell: Oblivion-converted spells keep their per-effect area
        // even where the projectile has no explosion record.
        bool HasArea(const RE::MagicItem* a_item) {
            return std::ranges::any_of(a_item->effects, [](const RE::Effect* a_effect) {
                return a_effect && a_effect->effectItem.area > 0;
            });
        }

        // 570 of Skyblivion's 608 missile projectiles have an explosion, but
        // the plain single-target bolts do not; area spells often have none.
        bool IsExplosive(RE::Projectile* a_proj, const RE::BGSProjectile* a_base, const RE::MagicItem* a_spell) {
            return a_proj->GetProjectileRuntimeData().explosion || (a_base && a_base->data.explosionType) || HasArea(a_spell);
        }

        // Beams, flames and cones cannot be turned around.
        bool CanTurn(RE::Projectile* a_proj, const RE::BGSProjectile* a_base) {
            return a_base && !a_base->IsBeam() && !a_base->IsFlamethrower() && !a_base->IsCone() &&
                   !a_proj->IsBeamProjectile() && !a_proj->IsFlameProjectile();
        }

        // Round 5: returned arrows flew THROUGH the archer. Projectile::InitHavok
        // (AE 44114, vfunc 0x66) bakes (GetCollisionGroup() << 16) | layer into
        // the projectile's phantom at launch, so Havok never reports contacts
        // with the original shooter. Re-apply the group now that the shooter is
        // the reflector: GetCollisionGroup (AE 44195) reads the CURRENT shooter.
        // Returns {old, new} filter info, or {0, 0} when there is no phantom.
        std::pair<std::uint32_t, std::uint32_t> RefreshCollisionGroup(RE::Projectile* a_proj) {
            auto&      rd = a_proj->GetProjectileRuntimeData();
            const auto phantom = reinterpret_cast<RE::bhkRefObject*>(rd.unk0E0);  // bhkSimpleShapePhantom
            const auto object = phantom ? static_cast<RE::hkpWorldObject*>(phantom->referencedObject.get()) : nullptr;
            if (!object) {
                return { 0, 0 };
            }
            auto&      info = object->collidable.broadPhaseHandle.collisionFilterInfo.filter;  // CFilter in v12
            const auto old = info;
            info = (a_proj->GetCollisionGroup() << 16) | (old & 0xFFFF);
            return { old, info };
        }

        // Turn a projectile around: it flies at a_aimAt, owned by a_reflector.
        void Redirect(RE::Projectile* a_proj, const RE::BGSProjectile* a_base, RE::Actor* a_reflector, RE::Actor* a_aimAt) {
            auto& rd = a_proj->GetProjectileRuntimeData();

            // Round 1: arrows dropped straight down. Their velocity fields held
            // far less than a flight speed in game units, and the lob computed
            // from that pointed the arrow up at almost no speed. Now each field
            // keeps its own length (only the direction turns), and the flight
            // time for the lob comes from the record's speed, with the lob bounded.
            const float velocityLength = rd.velocity.Length();
            const float linearLength = rd.linearVelocity.Length();
            float flightSpeed = std::max(velocityLength, linearLength);
            if (flightSpeed < 500.0f && a_base && a_base->data.speed > 0.0f) {
                flightSpeed = a_base->data.speed * (rd.speedMult > 0.0f ? rd.speedMult : 1.0f);
            }
            if (flightSpeed < 500.0f) {
                flightSpeed = 3000.0f;
            }

            const auto  from = a_proj->GetPosition();
            const float distance = (a_aimAt->GetPosition() - from).Length();
            float       lob = 0.0f;
            RE::NiPoint3 delta;
            // Round 5: a straight aim at the torso clipped the ledge/cover the
            // archer shot from (every returned arrow hit layer 1 = static).
            // Retrace = reverse the incoming velocity. With constant horizontal
            // speed and the same gravity the reversed arc IS the incoming arc, so
            // it lands where the shot was fired from - a line known to be clear.
            const bool retrace = Settings::Get().returnAim == Settings::Aim::kRetrace && linearLength > 100.0f;
            if (retrace) {
                delta = -rd.linearVelocity;
            } else {
                auto aim = a_aimAt->GetPosition();
                aim.z += a_aimAt->GetHeight() * 0.6f;  // torso
                delta = aim - from;
                if (a_base && a_base->data.gravity > 0.0f && distance > 0.0f) {
                    const float t = distance / flightSpeed;
                    lob = std::min(0.5f * kGravity * a_base->data.gravity * t * t, 0.25f * distance);
                    delta.z += lob;
                }
            }
            if (delta.Unitize() <= 0.0f) {
                return;
            }

            if (ShouldLog()) {
                Log::INFO("SpellReflect: turn ({}): |velocity| {:.1f} |linearVelocity| {:.1f} record speed {:.0f} x{:.2f} gravity {:.2f} -> "
                             "flight speed {:.0f}, distance {:.0f}, lob {:.0f}, dir ({:.2f}, {:.2f}, {:.2f}), flags {:#x}, "
                             "weapon damage {:.1f}, projectile {:08X}",
                             retrace ? "retrace" : "direct", velocityLength, linearLength,
                             a_base ? a_base->data.speed : 0.0f, rd.speedMult, a_base ? a_base->data.gravity : 0.0f,
                             flightSpeed, distance, lob, delta.x, delta.y, delta.z, rd.flags.underlying(), rd.weaponDamage, a_proj->GetFormID());
            }

            rd.velocity = delta * (velocityLength > 0.0f ? velocityLength : flightSpeed);
            rd.linearVelocity = delta * (linearLength > 0.0f ? linearLength : flightSpeed);
            rd.shooter = a_reflector->CreateRefHandle();
            rd.desiredTarget = a_aimAt->CreateRefHandle();
            if (const auto cause = a_reflector->GetActorCause()) {
                a_proj->SetActorCause(cause);
            }
            a_proj->data.angle.x = -std::asin(std::clamp(delta.z, -1.0f, 1.0f));
            a_proj->data.angle.z = std::atan2(delta.x, delta.y);

            // The new group ignores the reflector instead of the original shooter.
            // Pairs with the reflector that already exist keep reporting; those
            // hits are skipped via MarkRedirected below.
            const auto [oldFilter, newFilter] = RefreshCollisionGroup(a_proj);
            if (ShouldLog()) {
                Log::INFO("SpellReflect: turn: projectile {:08X} collision filter {:#010x} -> {:#010x}{}", a_proj->GetFormID(), oldFilter,
                             newFilter, oldFilter == 0 && newFilter == 0 ? " (no phantom!)" : "");
            }

            MarkRedirected(a_proj->GetFormID(), a_reflector->GetFormID());
        }

        enum class HitResult {
            kPass,  // let the engine process the hit
            kSkip   // the projectile was turned around: ignore this collision
        };

        HitResult OnSpellHit(RE::Projectile* a_proj, RE::Actor* a_shooter, RE::Actor* a_target, std::uint32_t a_layer) {
            const auto& s = Settings::Get();
            auto&       rd = a_proj->GetProjectileRuntimeData();
            const auto  spell = rd.spell;
            if (!IsReflectableItem(spell)) {
                return HitResult::kPass;
            }
            const auto baseObject = a_proj->GetBaseObject();
            const auto base = baseObject ? baseObject->As<RE::BGSProjectile>() : nullptr;
            if (IsExplosive(a_proj, base, spell) && !s.reflectExplosive) {
                return HitResult::kPass;  // explosions and area spells are never reflected
            }
            // Every hit's own decision is kept, a "no" included: a beam or flame hits several times
            // a second, and an earlier "yes" must not carry over to a hit whose roll failed.
            const Key   key{ a_shooter->GetFormID(), a_target->GetFormID(), spell };
            const auto  c = ReflectChance(a_target);
            const float chance = c.value;
            if (chance <= 0.0f) {
                Remember(key, false);
                return HitResult::kPass;
            }
            const float roll = Roll();
            const bool  reflect = roll < chance;
            const bool  turn = reflect && s.mode == Settings::Mode::kBounce && CanTurn(a_proj, base);

            if (ShouldLog()) {
                Log::INFO("SpellReflect: spell hit: {} -> {} '{}' layer {} chance {:.0f}% (av {:.1f} worn {:.1f} blocking {:.1f}) roll {:.1f} -> {}{}",
                             NameOf(a_shooter), NameOf(a_target), NameOf(spell), a_layer, chance, c.av, c.worn, c.blocking, roll,
                             reflect ? (turn ? "BOUNCE" : "SWAP") : "no",
                             s.observeOnly ? " (observe only)" : "");
            }
            if (!reflect || s.observeOnly) {
                Remember(key, false);
                return HitResult::kPass;
            }
            Feedback::Play(Feedback::Event::kSpellReflect, a_target);
            if (turn) {
                Redirect(a_proj, base, a_target, a_shooter);
                return HitResult::kSkip;
            }
            // Swap: the hit goes ahead and AddTarget re-routes its effects.
            Remember(key, true);
            return HitResult::kPass;
        }

        // Angle (degrees) between where the actor faces and where the projectile came from.
        float IncomingAngle(RE::Actor* a_actor, const RE::Projectile* a_proj, RE::Actor* a_shooter) {
            const auto& rd = a_proj->GetProjectileRuntimeData();
            float       fromX = -rd.velocity.x;
            float       fromY = -rd.velocity.y;
            if (std::abs(fromX) + std::abs(fromY) < 1.0f) {
                const auto d = a_shooter->GetPosition() - a_actor->GetPosition();
                fromX = d.x;
                fromY = d.y;
            }
            const float len = std::sqrt(fromX * fromX + fromY * fromY);
            if (len <= 0.0f) {
                return 0.0f;
            }
            const float heading = a_actor->GetAngleZ();
            const float dot = (std::sin(heading) * fromX + std::cos(heading) * fromY) / len;
            return std::acos(std::clamp(dot, -1.0f, 1.0f)) * 57.2957795f;
        }

        // The parry rules, shared by arrows and spell projectiles (Skyblivion integration
        // workspace, perk rework): block within the window before the projectile lands,
        // face where it came from, not in the parry cooldown. A shield block needs the
        // second Reflect Arrows rank (or [Arrows] bAllowWithShield).
        struct Parry {
            bool  blocking;
            bool  shield;
            bool  shieldAllowed;
            float since;
            float window;
            float angle;
            bool  cooling;
            bool  ok;
        };

        Parry CheckParry(RE::PlayerCharacter* a_player, const RE::Projectile* a_proj, RE::Actor* a_shooter) {
            const auto& s = Settings::Get();
            Parry       p{};
            p.blocking = a_player->IsBlocking();
            const auto leftHand = a_player->GetEquippedObject(true);
            const auto leftArmor = leftHand ? leftHand->As<RE::TESObjectARMO>() : nullptr;
            p.shield = leftArmor && leftArmor->IsShield();
            p.shieldAllowed = s.arrowAllowShield || (g_arrowShieldPerk && a_player->HasPerk(g_arrowShieldPerk));
            p.since = BlockTiming::SecondsSinceBlockPress();
            p.window = BlockTiming::Window(a_player);
            p.angle = IncomingAngle(a_player, a_proj, a_shooter);
            const auto last = g_lastArrowReflect.load(std::memory_order_relaxed);
            p.cooling = last != 0 && Clock::now() - Clock::time_point(Clock::duration(last)) <
                                         std::chrono::duration<float>(s.arrowCooldown);
            p.ok = p.blocking && (!p.shield || p.shieldAllowed) && p.since >= 0.0f && p.since <= p.window &&
                   p.angle <= s.facingAngle && !p.cooling;
            return p;
        }

        void StartParryCooldown() {
            g_lastArrowReflect.store(Clock::now().time_since_epoch().count(), std::memory_order_relaxed);
        }

        HitResult OnArrowHit(RE::Projectile* a_proj, RE::Actor* a_shooter, RE::Actor* a_target, std::uint32_t a_layer) {
            const auto& s = Settings::Get();
            if (!s.arrowsEnabled || !g_arrowPerk) {
                return HitResult::kPass;
            }
            const auto player = RE::PlayerCharacter::GetSingleton();
            if (a_target != player || !player->HasPerk(g_arrowPerk)) {
                return HitResult::kPass;
            }
            const auto p = CheckParry(player, a_proj, a_shooter);

            if (ShouldLog()) {
                Log::INFO("SpellReflect: arrow hit: {} -> player layer {} blocking={} shield={} (shield rank {}) pressed {:.3f}s ago window {:.3f}s ({} {:.0f}) angle {:.0f}{} -> {}{}",
                          NameOf(a_shooter), a_layer, p.blocking, p.shield, p.shieldAllowed, p.since, p.window, s.windowActorValue,
                          BlockTiming::WindowPoints(player), p.angle, p.cooling ? " (cooldown)" : "",
                          p.ok ? "REFLECT" : "no", s.observeOnly ? " (observe only)" : "");
            }
            if (!p.ok || s.observeOnly) {
                return HitResult::kPass;
            }
            StartParryCooldown();
            const auto baseObject = a_proj->GetBaseObject();
            Redirect(a_proj, baseObject ? baseObject->As<RE::BGSProjectile>() : nullptr, player, a_shooter);
            Feedback::Play(Feedback::Event::kArrowReflect, player);
            return HitResult::kSkip;
        }

        // Reflect Spells (perk rework): a hostile spell projectile on the player, parried under
        // the arrow rules, flies back at its caster. Beams, flames and cones cannot turn: their
        // effects land on the caster instead (the AddTarget swap). nullopt = not parried, the
        // chance-based reflection decides as before.
        std::optional<HitResult> OnSpellParry(RE::Projectile* a_proj, RE::Actor* a_shooter, RE::Actor* a_target, std::uint32_t a_layer) {
            const auto& s = Settings::Get();
            if (!s.parryEnabled || !g_parryPerk) {
                return std::nullopt;
            }
            const auto player = RE::PlayerCharacter::GetSingleton();
            if (a_target != player || !player->HasPerk(g_parryPerk)) {
                return std::nullopt;
            }
            auto&      rd = a_proj->GetProjectileRuntimeData();
            const auto spell = rd.spell;
            if (!IsReflectableItem(spell)) {
                return std::nullopt;
            }
            const auto baseObject = a_proj->GetBaseObject();
            const auto base = baseObject ? baseObject->As<RE::BGSProjectile>() : nullptr;
            if (IsExplosive(a_proj, base, spell) && !s.parryExplosive) {
                return std::nullopt;
            }
            const auto p = CheckParry(player, a_proj, a_shooter);
            const bool canTurn = CanTurn(a_proj, base);

            if (ShouldLog()) {
                Log::INFO("SpellReflect: spell parry: {} -> player '{}' layer {} blocking={} shield={} (shield rank {}) pressed {:.3f}s ago window {:.3f}s ({} {:.0f}) angle {:.0f}{} -> {}{}",
                          NameOf(a_shooter), NameOf(spell), a_layer, p.blocking, p.shield, p.shieldAllowed, p.since, p.window,
                          s.windowActorValue, BlockTiming::WindowPoints(player), p.angle, p.cooling ? " (cooldown)" : "",
                          p.ok ? (canTurn ? "PARRY (turned)" : "PARRY (effects to the caster)") : "no",
                          s.observeOnly ? " (observe only)" : "");
            }
            if (!p.ok || s.observeOnly) {
                return std::nullopt;
            }
            StartParryCooldown();
            Feedback::Play(Feedback::Event::kSpellReflect, player);
            if (canTurn) {
                Redirect(a_proj, base, player, a_shooter);
                return HitResult::kSkip;
            }
            Remember({ a_shooter->GetFormID(), player->GetFormID(), spell }, true);
            return HitResult::kPass;
        }

        HitResult OnHit(RE::Projectile* a_proj, RE::TESObjectREFR* a_hit, std::uint32_t a_layer) {
            if (!a_proj || !a_hit) {
                return HitResult::kPass;
            }
            // A projectile we turned around is never reflected again; it just
            // ignores its reflector while it leaves.
            if (const auto reflector = RedirectedBy(a_proj)) {
                return *reflector == a_hit->GetFormID() ? HitResult::kSkip : HitResult::kPass;
            }
            const auto target = a_hit->As<RE::Actor>();
            if (!target || target->IsDead()) {
                return HitResult::kPass;
            }
            // Ward-layer hits. The team's reflect art (SKYBSpellAltReflectSpellHitArt, 149409) carries a ward
            // collision: the shield's while-blocking visual (MGEF 0D27C0 via 200193's PerkToApply) and the
            // reflect feedback put it on the actor with no ward power behind it. The engine stops spells at
            // that ward: the actor takes nothing, and a reflect decided here never reaches AddTarget, so the
            // caster takes nothing either. The skip below is meant to let the projectile on to the body, but it
            // only partly can (see HitHook::thunk): beams and flames still stop at such a ward. The full fix is
            // an art without the ward collision. A real ward (Ward spells, Spellbreaker: WardPower above 0) is
            // the engine's, and the roll waits for a body hit.
            if (a_layer == static_cast<std::uint32_t>(RE::COL_LAYER::kWard)) {
                const float wardPower = REBridge::AVOwner(target)->GetActorValue(RE::ActorValue::kWardPower);
                if (wardPower <= 0.0f) {
                    if (g_wardLogged.fetch_add(1, std::memory_order_relaxed) < 20) {
                        Log::INFO("SpellReflect: ward hit on {} with no ward power: projectile {:08X} goes on to the body",
                                  NameOf(a_hit), a_proj->GetFormID());
                    }
                    return HitResult::kSkip;
                }
                return HitResult::kPass;
            }
            auto&      rd = a_proj->GetProjectileRuntimeData();
            const auto shooterRef = REBridge::Lookup(rd.shooter);
            const auto shooter = shooterRef ? shooterRef->As<RE::Actor>() : nullptr;
            if (!shooter || shooter == target) {
                return HitResult::kPass;
            }
            if (rd.spell) {
                if (const auto parried = OnSpellParry(a_proj, shooter, target, a_layer)) {
                    return *parried;
                }
                return OnSpellHit(a_proj, shooter, target, a_layer);
            }
            if (rd.ammoSource) {
                return OnArrowHit(a_proj, shooter, target, a_layer);
            }
            return HitResult::kPass;
        }

        // ---- hit handler call sites ----
        using HitFn = bool(RE::Projectile*, RE::TESObjectREFR*, RE::NiPoint3*, void*, std::uint32_t, std::uint32_t, bool*);

        template <std::size_t I>
        struct HitHook {
            static bool thunk(RE::Projectile* a_proj, RE::TESObjectREFR* a_hit, RE::NiPoint3* a_location,
                              void* a_unk, std::uint32_t a_layer, std::uint32_t a_material, bool* a_handled) {
                const auto result = OnHit(a_proj, a_hit, a_layer);
                if (result == HitResult::kSkip) {
                    // true does not move on to the next contact. In HandleHits (missiles, arrows) it ends this
                    // frame's contacts, the body behind the ward included; the beam/flame target pick ignores the
                    // value and impacts the ward anyway; barriers have already impacted. So a powerless ward
                    // (the reflect feedback art, the shield's blocking art) still stops beams and flames.
                    return true;
                }
                return func(a_proj, a_hit, a_location, a_unk, a_layer, a_material, a_handled);
            }
            static inline REL::Relocation<HitFn> func;
        };

        using HitSites = std::array<std::uintptr_t, kMaxHitCallSites>;  // 0 = no site at that slot on this runtime

        template <std::size_t... I>
        void WriteHitHooks(const HitSites& a_sites, std::index_sequence<I...>) {
            auto& trampoline = SKSE::GetTrampoline();
            ((a_sites[I] ? (void)(HitHook<I>::func = trampoline.write_call<5>(a_sites[I], HitHook<I>::thunk)) : (void)0), ...);
        }

        void InstallHitHook() {
            const auto& module = REL::Module::get();
            const auto  target = kHitHandler.address();
            const auto  calls = CallScan::FindCalls(target);
            Log::INFO("SpellReflect: hit hook: {} direct calls of the hit handler", calls.size());

            HitSites    sites{};
            std::size_t count = 0;
            bool        ok = true;
            auto        resolve = [&](std::size_t a_index, std::string_view a_caller, std::uintptr_t a_callerAddress,
                               std::span<const CallScan::Hint> a_hints) {
                const auto found = CallScan::FindCallIn(std::format("hit hook: site {}", a_index), a_caller, a_callerAddress,
                                                        target, calls, a_hints);
                if (!found) {
                    ok = false;
                    return;
                }
                sites[a_index] = *found;
            };
            if (REL::Module::IsAE()) {
                count = kAeHitCallSites.size();
                for (std::size_t i = 0; i < count; ++i) {
                    const auto& site = kAeHitCallSites[i];
                    const std::array hints{
                        CallScan::Hint{ { 1, 6, 1170, 0 }, site.offset16 },
                        CallScan::Hint{ { 1, 7, 104, 0 }, site.offset17 },
                    };
                    resolve(i, std::format("{}", site.callerId), REL::ID(site.callerId).address(), hints);
                }
            } else {
                count = kSeHitCallSites.size();
                for (std::size_t i = 0; i < count; ++i) {
                    const auto& site = kSeHitCallSites[i];
                    const std::array hints{
                        CallScan::Hint{ { 1, 5, 97, 0 }, site.offset },
                        CallScan::Hint{ { 1, 4, 15, 0 }, site.offset },
                    };
                    const auto caller = REL::Module::IsVR() ? REL::Offset(site.vrOffset).address() : REL::ID(site.callerId).address();
                    resolve(i, std::format("{}", site.callerId), caller, hints);
                }
            }
            // A caller this plugin does not know would keep engine behaviour: say so.
            for (const auto& call : calls) {
                if (std::ranges::find(sites, call.site) == sites.end()) {
                    Log::WARN("SpellReflect: hit hook: unknown caller at +{:#x} (function +{:#x}) is not hooked",
                                 call.site - module.base(), call.function ? call.function - module.base() : 0);
                }
            }
            if (!ok) {
                Log::ERROR("SpellReflect: hit hook: NOT installed (all or nothing); projectile reflection OFF");
                return;
            }
            WriteHitHooks(sites, std::make_index_sequence<kMaxHitCallSites>{});
            g_hitHookActive = true;
            Log::INFO("SpellReflect: hit hook: installed at {} call sites", count);
        }

        // ---- MagicTarget::AddTarget ----
        thread_local bool t_rerouting = false;

        // The caster to send this effect to, or nullptr to apply it normally.
        RE::Actor* RerouteTo(RE::MagicTarget* a_this, const RE::MagicTarget::AddTargetData& a_data) {
            if (!a_data.caster || !a_data.magicItem || !a_data.effect) {
                return nullptr;
            }
            const auto targetRef = a_this->GetTargetStatsObject();  // runtime-safe MagicTarget -> Actor
            const auto target = targetRef ? targetRef->As<RE::Actor>() : nullptr;
            const auto caster = a_data.caster->As<RE::Actor>();
            if (!target || !caster || caster == target || target->IsDead()) {
                return nullptr;
            }
            const Key key{ caster->GetFormID(), target->GetFormID(), a_data.magicItem };
            if (const auto known = Recall(key)) {
                return *known ? caster : nullptr;  // decided at the projectile hit / first effect
            }
            // Nothing decided by a projectile hit: only touch spells roll here.
            // Aimed, target-location and explosion effects never do.
            const auto& s = Settings::Get();
            if (!s.reflectTouch || a_data.magicItem->GetDelivery() != RE::MagicSystem::Delivery::kTouch ||
                !IsReflectableItem(a_data.magicItem) || (HasArea(a_data.magicItem) && !s.reflectExplosive)) {
                return nullptr;
            }
            const auto  c = ReflectChance(target);
            const float chance = c.value;
            if (chance <= 0.0f) {
                return nullptr;
            }
            const float roll = Roll();
            const bool  reflect = roll < chance && !s.observeOnly;
            Remember(key, reflect);  // the spell's other effects follow this roll
            if (ShouldLog()) {
                Log::INFO("SpellReflect: touch: {} -> {} '{}' chance {:.0f}% roll {:.1f} -> {}{}",
                             NameOf(caster), NameOf(target), NameOf(a_data.magicItem), chance, roll,
                             roll < chance ? "SWAP" : "no", s.observeOnly ? " (observe only)" : "");
            }
            if (reflect) {
                Feedback::Play(Feedback::Event::kSpellReflect, target);
            }
            return reflect ? caster : nullptr;
        }

        template <std::size_t I>  // 0 = Character, 1 = PlayerCharacter
        struct AddTargetHook {
            static bool thunk(RE::MagicTarget* a_this, RE::MagicTarget::AddTargetData& a_data) {
                if (!t_rerouting) {
                    if (const auto caster = RerouteTo(a_this, a_data)) {
                        const auto original = a_data.caster;
                        a_data.caster = a_this->GetTargetStatsObject();  // the reflector casts it now
                        t_rerouting = true;  // the caster's own AddTarget must not reflect it back
                        const bool added = caster->AsMagicTarget()->AddTarget(a_data);
                        t_rerouting = false;
                        a_data.caster = original;  // the caller may reuse the data for other targets
                        return added;
                    }
                }
                return func(a_this, a_data);
            }
            static inline REL::Relocation<decltype(thunk)> func;
        };

        template <std::size_t I>
        void InstallAddTarget(const REL::VariantID& a_vtable, std::string_view a_name) {
            REL::Relocation<std::uintptr_t> vtable{ a_vtable };
            const auto current = *reinterpret_cast<const std::uintptr_t*>(vtable.address() + sizeof(void*) * 1);
            if (current == kAddTargetImpl.address()) {
                Log::INFO("SpellReflect: AddTarget hook: {} slot holds the engine function", a_name);
            } else {
                Log::INFO("SpellReflect: AddTarget hook: {} slot already replaced by another plugin ({:#x}); chaining", a_name, current);
            }
            AddTargetHook<I>::func = vtable.write_vfunc(1, AddTargetHook<I>::thunk);
        }
    }

    void Install() {
        InstallAddTarget<0>(RE::VTABLE_Character[4], "Character"sv);
        InstallAddTarget<1>(RE::VTABLE_PlayerCharacter[4], "PlayerCharacter"sv);
        InstallHitHook();
    }

    void OnDataLoaded() {
        const auto& s = Settings::Get();

        // The engine's own name lookup (what Papyrus uses), so an INI name
        // resolves exactly as it would in a script.
        g_reflectAV = REBridge::ActorValueByName(s.actorValue.c_str());
        if (g_reflectAV == RE::ActorValue::kNone) {
            Log::ERROR("SpellReflect: reflect actor value '{}' not found; spell reflection OFF", s.actorValue);
        } else {
            Log::INFO("SpellReflect: reflect actor value: {} ({}), cap {}% ({}% with a while-blocking bonus)", s.actorValue,
                         std::to_underlying(g_reflectAV), s.maxChance, s.maxBlockingChance);
        }

        g_blockingKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(s.blockingKeyword);
        if (g_blockingKeyword) {
            Log::INFO("SpellReflect: while-blocking keyword: {} {:08X}", s.blockingKeyword, g_blockingKeyword->GetFormID());
        } else {
            Log::WARN("SpellReflect: while-blocking keyword '{}' not found; shield reflection bonuses are OFF", s.blockingKeyword);
        }

        if (s.arrowsEnabled) {
            g_arrowPerk = Forms::Resolve<RE::BGSPerk>(s.arrowPerk, "arrow perk");
            if (!g_arrowPerk) {
                Log::WARN("SpellReflect: arrow reflection OFF (no perk)");
            }
            // The second rank only widens the first: without it, a shield block never reflects.
            g_arrowShieldPerk = Forms::Resolve<RE::BGSPerk>(s.arrowShieldPerk, "arrow shield perk");
            if (!g_arrowShieldPerk && !s.arrowAllowShield) {
                Log::WARN("SpellReflect: no arrow shield perk: only weapon / bare-handed blocks reflect");
            }
        }
        if (s.parryEnabled) {
            g_parryPerk = Forms::Resolve<RE::BGSPerk>(s.parryPerk, "spell parry perk");
            if (!g_parryPerk) {
                Log::WARN("SpellReflect: spell parry OFF (no perk)");
            }
        }
        if (!g_hitHookActive) {
            Log::WARN("SpellReflect: hit hook inactive: only touch spells can be reflected, no arrows");
        }
    }
}
