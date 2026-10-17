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

    //Load3D can run on the background loading thread and SetInitialDisposition changes actor
    //values, so hand the work to the main thread. Capture a handle rather than the pointer:
    //the actor may be gone by the time the task runs.
    auto* taskInterface = SKSE::GetTaskInterface();

    assert(taskInterface);

    RE::ActorHandle handle = actor->GetHandle();

    taskInterface->AddTask([handle]() {
        RE::ActorPtr actor = handle.get();
        if (!actor || actor->IsPlayerRef()) {
            return;
        }
        DispositionSystem::SetInitialDisposition(actor.get());
        });

    return niAVObject;
}

void CharacterLoad3DHook::Apply() {
    REL::Relocation<std::uintptr_t> vtbl{ RE::Character::VTABLE[0] };
    func = vtbl.write_vfunc(idx, thunk);
}
