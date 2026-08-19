#include <Windows.h>
#include "CharacterLoad3D.h"
#include "DispositionSystem.h"
#include <ClibUtil/EditorID.hpp>

bool CharacterLoad3D::thunk(actor, a_backgroundLoading)
{
    auto niAVObject = func(actor, a_backgroundLoading);

    if (!niAVObject) return; 

    // Filter to player-initiated activations only (NPCs activating other NPCs would also reach here).
    auto player = RE::PlayerCharacter::GetSingleton();
    if (player && actor != player) {
      if (auto actorBase = actor->GetActorBase()) {
            std::string edid = clib_util::editorID::get_editorID(actorBase);
            DispositionSystem::SetInitialDisposition(actor, edid);
            Log::INFO("Set disposition after Character Load3d on Actor: {}", edid);
      }
        
    }

    return niAVObject;
}

void CharacterLoad3D::Apply() {
    REL::Relocation<std::uintptr_t> vtbl{ RE::Character::VTABLE[0] };
    func = vtbl.write_vfunc(idx, Activate);
}
