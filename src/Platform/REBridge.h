#pragma once

//RE:: API bridges
//A few RE:: members differ in shape between CommonLib backends.
//CommonLibSSE-NG is multi-runtime: members whose offset varies between SE and
//VR exist behind a GetXxxRuntimeData() accessor whereas libxse/commonlibsse
//(SE only) exposes them directly. These thin inline helpers hide that
//difference so the application code stays uniform and backend-agnostic.

#include "Platform/Backend.h"

namespace REBridge
{
	[[nodiscard]] inline auto& PlayerData(RE::PlayerCharacter* a_player)
	{
#if SKY_COMMONLIB == SKY_COMMONLIB_NG
		return a_player->GetPlayerRuntimeData();
#else
		return *a_player;
#endif
	}

	[[nodiscard]] inline RE::ActorValueOwner* AVOwner(RE::Actor* a_actor)
	{
#if SKY_COMMONLIB == SKY_COMMONLIB_NG
		return a_actor->AsActorValueOwner();
#else
		return static_cast<RE::ActorValueOwner*>(a_actor);
#endif
	}

	[[nodiscard]] inline RE::ActorState* ActorStateOf(RE::Actor* a_actor)
	{
#if SKY_COMMONLIB == SKY_COMMONLIB_NG
		return a_actor->AsActorState();
#else
		return static_cast<RE::ActorState*>(a_actor);
#endif
	}
}
