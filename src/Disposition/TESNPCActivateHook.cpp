#include <Windows.h>
#include "TESNPCActivateHook.h"
#include "DispositionSystem.h"
#include <ClibUtil/EditorID.hpp>

bool TESNPCActivateHook::Activate(
    RE::TESNPC* a_this,
    RE::TESObjectREFR* a_targetRef,
    RE::TESObjectREFR* a_activatorRef,
    std::uint8_t a_arg3,
    RE::TESBoundObject* a_object,
    std::int32_t a_targetCount)
{
    // Filter to player-initiated activations only (NPCs activating other NPCs would also reach here).
    auto player = RE::PlayerCharacter::GetSingleton();
    if (player && a_activatorRef == player) {
        if (auto actor = a_targetRef ? a_targetRef->As<RE::Actor>() : nullptr) {
            if (auto actorBase = actor->GetActorBase()) {
                std::string edid = clib_util::editorID::get_editorID(actorBase);
                DispositionSystem::SetActorDisposition(actor, edid);
                REX::INFO("Set disposition before TESNPC activate: {}", edid);
            }
        }
    }

    return func(a_this, a_targetRef, a_activatorRef, a_arg3, a_object, a_targetCount);
}

void TESNPCActivateHook::Apply() {
    REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_TESNPC[0] };
    func = vtbl.write_vfunc(idx, Activate);
    REX::INFO("Applied TESNPCActivateHook");
}
