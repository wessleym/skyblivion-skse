#include "PerkSpells/EntryPointSpells.h"

#include "PerkSpells/Settings.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace PerkSpells::EntryPointSpells {
    namespace {
        using Kind = Settings::Kind;
        using Stacking = Settings::Stacking;
        using EntryPoint = RE::BGSEntryPoint::ENTRY_POINT;
        using ReturnType = RE::BSContainer::ForEachResult;

        constexpr std::size_t kKinds = std::to_underlying(Kind::kTotal);
        constexpr std::size_t kMaxVarArgs = 8;

        // Fixed-width stand-in for the engine's variadic signature. We forward
        // a superset of what any caller passes; the engine only reads the
        // slots its entry point defines.
        using HandleFn = void (*)(std::uint32_t, RE::Actor*, void*, void*, void*, void*, void*, void*, void*, void*);

        HandleFn g_engineHandle = nullptr;

        std::optional<Kind> KindOf(std::uint32_t a_entryPoint) {
            switch (a_entryPoint) {
                case EntryPoint::kApplyCombatHitSpell:
                    return Kind::kCombatHit;
                case EntryPoint::kApplyBashingSpell:
                    return Kind::kBashing;
                case EntryPoint::kApplyReanimateSpell:
                    return Kind::kReanimate;
                case EntryPoint::kApplyWeaponSwingSpell:
                    return Kind::kWeaponSwing;
                case EntryPoint::kApplySneakingSpell:
                    return Kind::kSneaking;
                default:
                    return std::nullopt;
            }
        }

        // PerkConditionTabCount per entry point as authored in Skyblivion.esm
        // (0 = not present in the data, accept whatever the entry says).
        // Subjects arrive in tab order: CombatHit = [weapon, target],
        // Bashing = [target] (confirmed by the 09-28 in-game log).
        constexpr std::array<std::uint8_t, kKinds> kExpectedTabs{ 3, 2, 0, 3, 1 };

        // Weapon Swing and Sneaking deliver to the perk owner itself: in
        // Skyblivion's data the swing spells are self buffs of the actor being
        // swung at (Heightened Reflexes, Weakling's Regret "...SelfSpell").
        bool DeliversToOwner(Kind a_kind) {
            return a_kind == Kind::kWeaponSwing || a_kind == Kind::kSneaking;
        }

        std::string Describe(const RE::TESForm* a_form) {
            if (!a_form) {
                return "none";
            }
            // References carry no name of their own; ask for the display name.
            const auto* ref = a_form->AsReference();
            const char* name = ref ? const_cast<RE::TESObjectREFR*>(ref)->GetDisplayFullName() : a_form->GetName();
            return std::format("{:08X} '{}' ({:02X})", a_form->GetFormID(), name ? name : "",
                               std::to_underlying(a_form->GetFormType()));
        }

        // ---- per-call context ----------------------------------------------

        struct Armed {
            const RE::Actor*                owner = nullptr;
            std::uint32_t                   entryPoint = 0;
            RE::SpellItem**                 slot = nullptr;
            Stacking                        stacking = Stacking::kPerPerk;
            bool                            reached = false;
            bool                            inside = false;
            std::vector<RE::SpellItem*>     spells;
            std::vector<const RE::BGSPerk*> contributors;  // parallel to spells (PerPerk)
        };

        thread_local Armed* t_armed = nullptr;

        class ArmedScope {
        public:
            explicit ArmedScope(Armed* a_armed) :
                _previous(t_armed) {
                t_armed = a_armed;
            }
            ~ArmedScope() { t_armed = _previous; }
            ArmedScope(const ArmedScope&) = delete;
            ArmedScope& operator=(const ArmedScope&) = delete;

        private:
            Armed* _previous;
        };

        // Reads the tab count and function type of the first entry.
        class ProbeVisitor final : public RE::PerkEntryVisitor {
        public:
            ReturnType Visit(RE::BGSPerkEntry* a_entry) override {
                if (!a_entry || a_entry->GetType() != RE::PERK_ENTRY_TYPE::kEntryPoint) {
                    return ReturnType::kContinue;
                }
                const auto* entry = static_cast<RE::BGSEntryPointPerkEntry*>(a_entry);
                found = true;
                tabCount = entry->entryData.numArgs;
                selectsSpell = entry->functionData &&
                               entry->functionData->GetType() == RE::BGSEntryPointFunctionData::ENTRY_POINT_FUNCTION_DATA::kSpellItem;
                return ReturnType::kStop;
            }

            bool         found = false;
            bool         selectsSpell = false;
            std::uint8_t tabCount = 0;
        };

        // Wraps the engine's visitor: lets it evaluate each entry, takes the
        // spell it selected out of the result slot, and keeps it going.
        class CollectingVisitor final : public RE::PerkEntryVisitor {
        public:
            CollectingVisitor(RE::PerkEntryVisitor& a_engine, Armed& a_armed) :
                _engine(a_engine), _armed(a_armed) {}

            ReturnType Visit(RE::BGSPerkEntry* a_entry) override {
                _engine.Visit(a_entry);
                RE::SpellItem* picked = *_armed.slot;
                if (!picked) {
                    return ReturnType::kContinue;
                }
                *_armed.slot = nullptr;

                const RE::BGSPerk* perk = nullptr;
                if (a_entry && a_entry->GetType() == RE::PERK_ENTRY_TYPE::kEntryPoint) {
                    perk = static_cast<RE::BGSEntryPointPerkEntry*>(a_entry)->perk;
                }

                // PerPerk: within one perk the later match replaces the earlier
                // one. The engine never stops the walk (seen in the 09-28 log)
                // and overwrites its result slot per match, so its own single
                // pick is the last match - we keep that per perk.
                if (_armed.stacking == Stacking::kPerPerk && perk) {
                    const auto it = std::ranges::find(_armed.contributors, perk);
                    if (it != _armed.contributors.end()) {
                        const auto index = static_cast<std::size_t>(it - _armed.contributors.begin());
                        const bool elsewhere = std::ranges::find(_armed.spells, picked) != _armed.spells.end();
                        if (!elsewhere) {
                            _armed.spells[index] = picked;
                        }
                        return ReturnType::kContinue;
                    }
                }
                if (std::ranges::find(_armed.spells, picked) == _armed.spells.end()) {
                    _armed.spells.push_back(picked);
                    _armed.contributors.push_back(perk);
                }
                return ReturnType::kContinue;
            }

        private:
            RE::PerkEntryVisitor& _engine;
            Armed&                _armed;
        };

        // One per vtable (N), each with its own original: another plugin may already have hooked the slot in
        // one of them.
        template <std::size_t N>
        struct ForEachPerkEntryHook {
            static void Thunk(const RE::Actor* a_this, EntryPoint a_entryPoint, RE::PerkEntryVisitor& a_visitor) {
                Armed* armed = t_armed;
                if (armed && !armed->inside && armed->owner == a_this &&
                    armed->entryPoint == static_cast<std::uint32_t>(a_entryPoint)) {
                    armed->reached = true;
                    armed->inside = true;
                    CollectingVisitor wrapper{ a_visitor, *armed };
                    original(a_this, a_entryPoint, wrapper);
                    armed->inside = false;
                    return;
                }
                original(a_this, a_entryPoint, a_visitor);
            }

            static void Install(REL::VariantID a_vtable, std::string_view a_name) {
                // VR's Actor vtable has two more entries before it: 0x100 there is RemoveTemporaryPerk
                // (CommonLib's own Actor::ForEachPerkEntry uses RelocateVirtual(0x100, 0x102)).
                const std::size_t slot = REL::Module::IsVR() ? 0x102 : 0x100;
                original = REL::Relocation<std::uintptr_t>{ a_vtable }.write_vfunc(slot, Thunk);
                Log::INFO("PerkSpells: hooked {}::ForEachPerkEntry (vtable slot {:X})", a_name, slot);
            }

            static inline REL::Relocation<decltype(Thunk)> original;
        };

        // ---- delivery ------------------------------------------------------

        RE::Actor* OtherActor(const RE::Actor* a_owner, void* const* a_subjects, std::size_t a_count) {
            RE::Actor* sameAsOwner = nullptr;
            for (std::size_t i = 0; i < a_count; ++i) {
                auto* form = static_cast<RE::TESForm*>(a_subjects[i]);
                if (!form || !form->Is(RE::FormType::ActorCharacter)) {
                    continue;
                }
                auto* actor = static_cast<RE::Actor*>(form);
                if (actor != a_owner) {
                    return actor;
                }
                sameAsOwner = actor;
            }
            return sameAsOwner;
        }

        void Deliver(RE::Actor* a_caster, RE::Actor* a_target, RE::SpellItem* a_spell, RE::Actor* a_blame, Kind a_kind) {
            // Abilities, diseases and addictions are not cast: the engine puts them on the target's spell list
            // (a disease is contracted until cured), so an extra of that type goes on the same way and stays, as
            // the engine's own pick would. In Skyblivion's data (2026-11-06) the only such entries are
            // SKYBCrVampPerk's SKYBVampDisease (Combat Hit) and one Sneaking ability, skipped below.
            switch (a_spell->GetSpellType()) {
                case RE::MagicSystem::SpellType::kAbility:
                case RE::MagicSystem::SpellType::kDisease:
                case RE::MagicSystem::SpellType::kAddiction:
                    if (a_kind == Kind::kSneaking) {
                        // The engine removes its own sneaking spell when sneaking
                        // ends; we have no matching removal point for extras.
                        static std::atomic<bool> warned{ false };
                        if (!warned.exchange(true)) {
                            Log::WARN("PerkSpells: Sneaking: extra ability-type spell {} skipped (no removal point)", Describe(a_spell));
                        }
                        return;
                    }
                    a_target->AddSpell(a_spell);
                    return;
                default:
                    break;
            }
            auto* caster = a_caster->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
            if (!caster) {
                static std::atomic<bool> warned{ false };
                if (!warned.exchange(true)) {
                    Log::WARN("PerkSpells: {}: {} has no instant caster, spell {} not delivered",
                                 Settings::Name(a_kind), Describe(a_caster), Describe(a_spell));
                }
                return;
            }
            caster->CastSpellImmediate(a_spell, false, a_target, 1.0f, false, 0.0f, a_blame);
        }

        // ---- the call-site thunk -------------------------------------------

        void HandleEntryPointThunk(std::uint32_t a_entryPoint, RE::Actor* a_owner,
                                   void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7) {
            const auto forward = [&]() { g_engineHandle(a_entryPoint, a_owner, a0, a1, a2, a3, a4, a5, a6, a7); };

            const auto kind = KindOf(a_entryPoint);
            if (!kind || !a_owner) {
                return forward();
            }
            const auto  k = std::to_underlying(*kind);
            const auto& cfg = Settings::Get();
            const bool  toOwner = DeliversToOwner(*kind);
            const bool  stack = cfg.stacking != Stacking::kOff;
            const bool  swingRoute = *kind == Kind::kWeaponSwing && cfg.swingHostileToAttacker;
            if (!cfg.enabled[k] || !(stack || (cfg.ownerCasts && !toOwner) || swingRoute)) {
                return forward();
            }

            // Nested entry point calls (e.g. from condition functions) must not
            // see this call's context, and this probe must not see an outer one.
            ArmedScope isolate{ nullptr };

            const auto entryPoint = static_cast<EntryPoint>(a_entryPoint);
            if (!a_owner->HasPerkEntries(entryPoint)) {
                return forward();
            }

            ProbeVisitor probe;
            a_owner->ForEachPerkEntry(entryPoint, probe);
            if (!probe.found || !probe.selectsSpell || probe.tabCount == 0 || probe.tabCount > kMaxVarArgs ||
                (kExpectedTabs[k] != 0 && probe.tabCount != kExpectedTabs[k])) {
                static std::array<std::atomic<bool>, kKinds> warned{};
                if (!warned[k].exchange(true)) {
                    Log::WARN("PerkSpells: {}: unexpected entry shape (found={} selectsSpell={} tabs={} expected={}); engine behaviour kept",
                                 Settings::Name(*kind), probe.found, probe.selectsSpell, probe.tabCount, kExpectedTabs[k]);
                }
                return forward();
            }

            void* const       args[kMaxVarArgs]{ a0, a1, a2, a3, a4, a5, a6, a7 };
            const std::size_t subjectCount = probe.tabCount - 1u;
            auto** const      slot = static_cast<RE::SpellItem**>(args[subjectCount]);
            const bool        slotIsSubject = std::find(args, args + subjectCount, static_cast<void*>(slot)) != args + subjectCount;
            if (!slot || slotIsSubject || *slot != nullptr) {
                static std::array<std::atomic<bool>, kKinds> warned{};
                if (!warned[k].exchange(true)) {
                    Log::WARN("PerkSpells: {}: result slot failed validation (slot={} isSubject={} prefilled={}); engine behaviour kept",
                                 Settings::Name(*kind), static_cast<void*>(slot), slotIsSubject, slot && *slot != nullptr);
                }
                return forward();
            }

            // Run the engine, collecting every match (stacking) or only its single pick.
            std::vector<RE::SpellItem*> spells;
            if (stack) {
                Armed armed;
                armed.owner = a_owner;
                armed.entryPoint = a_entryPoint;
                armed.slot = slot;
                armed.stacking = cfg.stacking;
                {
                    ArmedScope arm{ &armed };
                    forward();
                }
                spells = std::move(armed.spells);
                if (*slot) {
                    // Written after the visit (or the visit never reached us).
                    if (std::ranges::find(spells, *slot) == spells.end()) {
                        spells.insert(spells.begin(), *slot);
                    }
                    *slot = nullptr;
                }
                if (!armed.reached) {
                    static std::array<std::atomic<bool>, kKinds> warned{};
                    if (!warned[k].exchange(true)) {
                        Log::WARN("PerkSpells: {}: engine did not visit entries through ForEachPerkEntry; only its single pick is available",
                                     Settings::Name(*kind));
                    }
                }
            } else {
                forward();
                if (*slot) {
                    spells.push_back(*slot);
                    *slot = nullptr;
                }
            }
            if (spells.empty()) {
                return;
            }

            if (swingRoute) {
                // Weapon Swing: the owner is the actor being swung at. Hostile
                // spells go from the owner to the attacker (tab 1); everything
                // else stays a self spell. A beneficial first pick is still
                // applied by the engine exactly as without this option.
                RE::Actor* attacker = OtherActor(a_owner, args, subjectCount);
                if (attacker == a_owner) {
                    attacker = nullptr;
                }
                std::size_t first = 0;
                if (!spells.front()->IsHostile()) {
                    *slot = spells.front();
                    first = 1;
                }
                for (std::size_t i = first; i < spells.size(); ++i) {
                    Deliver(a_owner, spells[i]->IsHostile() && attacker ? attacker : a_owner, spells[i], a_owner, *kind);
                }
                return;
            }
            RE::Actor* target = toOwner ? a_owner : OtherActor(a_owner, args, subjectCount);
            if (!target) {
                *slot = spells.front();
                static std::array<std::atomic<bool>, kKinds> warned{};
                if (!warned[k].exchange(true)) {
                    Log::WARN("PerkSpells: {}: no actor among the condition subjects; engine behaviour kept", Settings::Name(*kind));
                }
            } else if (cfg.ownerCasts && !toOwner) {
                for (auto* spell : spells) {
                    Deliver(a_owner, target, spell, a_owner, *kind);
                }
            } else {
                *slot = spells.front();
                for (std::size_t i = 1; i < spells.size(); ++i) {
                    Deliver(toOwner ? a_owner : target, target, spells[i], a_owner, *kind);
                }
            }
        }

        // ---- installation --------------------------------------------------

        struct CallSite {
            std::uintptr_t address;
            bool           isJump;
            std::uint32_t  entryPoint;
        };

        // Nearest preceding "mov ecx, imm32" (B9 id 00 00 00), or "lea ecx, [reg + disp8]"
        // ([REX] 8D, ModRM mod 01 reg ecx, disp8), naming one of our entry points. SE and VR load
        // Combat Hit the second way in Character::ApplyAllCombatSpellsFromAttack (id 37799:
        // lea ecx,[r14+33h] with r14 zero). Over-inclusion is harmless: the thunk checks the real
        // entry point on every call. On SE 1.5.97, AE 1.6.1170 / 1.7.104 and VR 1.4.15 this finds
        // all five entry points (Combat Hit at two sites).
        std::optional<std::uint32_t> IdentifyEntryPoint(const std::uint8_t* a_code, std::size_t a_offset) {
            constexpr std::size_t kWindow = 64;
            for (std::size_t back = 5; back <= kWindow && back <= a_offset; ++back) {
                const auto* p = a_code + a_offset - back;
                if (p[0] == 0xB9 && p[2] == 0 && p[3] == 0 && p[4] == 0 && KindOf(p[1])) {
                    return p[1];
                }
                const std::size_t rex = (p[0] & 0xF0) == 0x40 ? 1 : 0;
                const std::uint8_t modrm = p[rex + 1];
                if (p[rex] == 0x8D && (modrm & 0xF8) == 0x48 && (modrm & 7) != 4 && KindOf(p[rex + 2])) {
                    return p[rex + 2];
                }
            }
            return std::nullopt;
        }
    }

    void Install() {
        Settings::Load();

        REL::Relocation<std::uintptr_t> handle{ RELOCATION_ID(23073, 23526) };
        const std::uintptr_t            target = handle.address();
        g_engineHandle = reinterpret_cast<HandleFn>(target);
        Log::INFO("PerkSpells: HandleEntryPoint at {:X} (offset {:X})", target, handle.offset());

        const auto  text = REL::Module::get().segment(REL::Segment::textx);
        const auto* code = text.pointer<std::uint8_t>();
        const auto  size = text.size();
        const auto  base = text.address();
        const auto  moduleBase = REL::Module::get().base();

        std::vector<CallSite> sites;
        std::size_t           totalCallers = 0;
        for (std::size_t i = 0; i + 5 <= size; ++i) {
            const auto op = code[i];
            if (op != 0xE8 && op != 0xE9) {
                continue;
            }
            std::int32_t rel = 0;
            std::memcpy(&rel, code + i + 1, sizeof(rel));
            if (base + i + 5 + static_cast<std::intptr_t>(rel) != target) {
                continue;
            }
            ++totalCallers;
            if (const auto ep = IdentifyEntryPoint(code, i)) {
                sites.push_back({ base + i, op == 0xE9, *ep });
            }
        }

        std::array<std::size_t, kKinds> perKind{};
        for (const auto& site : sites) {
            ++perKind[std::to_underlying(*KindOf(site.entryPoint))];
        }
        Log::INFO("PerkSpells: {} direct callers of HandleEntryPoint, patching {} (CombatHit {} Bashing {} Reanimate {} WeaponSwing {} Sneaking {})",
                     totalCallers, sites.size(), perKind[0], perKind[1], perKind[2], perKind[3], perKind[4]);
        for (std::size_t k = 0; k < kKinds; ++k) {
            if (perKind[k] == 0) {
                Log::WARN("PerkSpells: {}: no call site identified - it keeps engine behaviour", Settings::Name(static_cast<Kind>(k)));
            }
        }
        if (sites.empty()) {
            return;
        }

        // Skyblivion.dll allocates one SKSE trampoline for all its call-site hooks
        // (Plugin::OnLoad; a second AllocTrampoline would replace it). Every site here
        // goes to the same thunk and the trampoline keeps one jump per destination, so
        // the redirection costs one 14-byte jump whatever the site count.
        constexpr std::size_t kJumpSize = 14;
        auto&                 trampoline = SKSE::GetTrampoline();
        if (trampoline.free_size() < kJumpSize) {
            Log::ERROR("PerkSpells: trampoline has {} bytes free, {} needed; no call site patched, engine behaviour kept",
                       trampoline.free_size(), kJumpSize);
            return;
        }
        for (const auto& site : sites) {
            const auto previous = site.isJump ? trampoline.write_branch<5>(site.address, HandleEntryPointThunk)
                                              : trampoline.write_call<5>(site.address, HandleEntryPointThunk);
            if (previous != target) {
                Log::ERROR("PerkSpells: call site +{:X}: previous target {:X} != HandleEntryPoint", site.address - moduleBase, previous);
            }
            Log::INFO("PerkSpells: patched {} at +{:X} (ep {})", site.isJump ? "jmp" : "call", site.address - moduleBase, site.entryPoint);
        }

        ForEachPerkEntryHook<0>::Install(RE::VTABLE_Actor[0], "Actor");
        ForEachPerkEntryHook<1>::Install(RE::VTABLE_Character[0], "Character");
        ForEachPerkEntryHook<2>::Install(RE::VTABLE_PlayerCharacter[0], "PlayerCharacter");
    }
}
