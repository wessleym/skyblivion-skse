#include <Windows.h>
#include "CharacterLoad3DHook.h"
#include "DispositionSystem.h"
#include <mutex>

RE::NiAVObject* CharacterLoad3DHook::thunk(RE::Actor* actor, bool a_backgroundLoading)
{
    auto* niAVObject = func(actor, a_backgroundLoading);
    if (!niAVObject) {
        return niAVObject;
    }

    //Load3D can run on the background loading thread and SetInitialDisposition dispatches a
    //Papyrus call, so hand the work to the main thread. Capture the FormID rather than the
    //pointer: the actor may be gone by the time the task runs.
    if (auto* taskInterface = SKSE::GetTaskInterface()) {
        const RE::FormID formID = actor->GetFormID();
        taskInterface->AddTask([formID]() {
            auto* a = RE::TESForm::LookupByID<RE::Actor>(formID);
            if (!a || a == RE::PlayerCharacter::GetSingleton()) {
                return;
            }
            DispositionSystem::SetInitialDisposition(a);
            });
    }

    return niAVObject;
}

void CharacterLoad3DHook::Apply() {
    REL::Relocation<std::uintptr_t> vtbl{ RE::Character::VTABLE[0] };
    func = vtbl.write_vfunc(idx, thunk);
}
