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
    auto* taskInterface = SKSE::GetTaskInterface();
    
    //<NEB> Assert is better than a null check for skse interfaces
    assert(taskInterface);

    //<NEB> Handles over form ids for tasks
    RE::ActorHandle handle = actor->GetHandle();
        
    taskInterface->AddTask([handle]() {
        RE::ActorPtr actor = handle.get();
        //<NEB>
        //It's best to use the IsPlayerRef function. Trivial but it's slightly quicker.
        if (!actor || actor->IsPlayerRef() == false) {
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
