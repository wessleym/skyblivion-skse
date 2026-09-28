#pragma once

//RE:: API bridges
//A few RE:: members differ in shape between CommonLib backends.
//CommonLibSSE-NG is multi-runtime: members whose offset varies between SE and
//VR exist behind a GetXxxRuntimeData() accessor whereas libxse/commonlibsse
//(SE only) exposes them directly. These thin inline helpers hide that
//difference so the application code stays uniform and backend-agnostic.

#include "Platform/Backend.h"

#include <cstddef>
#include <cstdint>

namespace REBridge
{
	//NG's GetPlayerRuntimeData() knows only the SE and AE positions: on VR it reads the wrong memory.
	//VR keeps these fields in its own block, reached through GetVRInfoRuntimeData() (null outside VR).
	//So PlayerCharacter fields get one bridge each, and there is no whole-struct PlayerData bridge.
#if SKY_COMMONLIB == SKY_COMMONLIB_NG
	//The VR block sits at 0xFE0; these are the absolute offsets CommonLib documents for its fields.
	static_assert(offsetof(RE::PlayerCharacter::VR_INFO_RUNTIME_DATA, skills) == 0x10B0 - 0xFE0);
	static_assert(offsetof(RE::PlayerCharacter::VR_INFO_RUNTIME_DATA, amountStolenSold) == 0x100C - 0xFE0);
#endif

	//The player's per-skill experience and level thresholds.
	[[nodiscard]] inline RE::PlayerCharacter::PlayerSkills* PlayerSkills(RE::PlayerCharacter* a_player)
	{
#if SKY_COMMONLIB == SKY_COMMONLIB_NG
		if (REL::Module::IsVR()) {
			return a_player->GetVRInfoRuntimeData()->skills;
		}
		return a_player->GetPlayerRuntimeData().skills;
#else
		return a_player->skills;
#endif
	}

	//Gold from stolen goods sold.
	[[nodiscard]] inline std::int32_t& AmountStolenSold(RE::PlayerCharacter* a_player)
	{
#if SKY_COMMONLIB == SKY_COMMONLIB_NG
		if (REL::Module::IsVR()) {
			return a_player->GetVRInfoRuntimeData()->amountStolenSold;
		}
		return a_player->GetPlayerRuntimeData().amountStolenSold;
#else
		return a_player->amountStolenSold;
#endif
	}

	//PlayerCharacter::GameStatsData: perkCount, difficulty, murder counts. NG's accessor includes VR's position.
	[[nodiscard]] inline auto& GameStats(RE::PlayerCharacter* a_player)
	{
#if SKY_COMMONLIB == SKY_COMMONLIB_NG
		return a_player->GetGameStatsData();
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

	//The actor's vendor faction, calculated when not yet cached.
	//NG's GetVendorFaction returns the value it read before recalculating, so an uncached actor reads as null once.
	[[nodiscard]] inline RE::TESFaction* VendorFaction(RE::Actor* a_actor)
	{
#if SKY_COMMONLIB == SKY_COMMONLIB_NG
		if (auto* faction = a_actor->GetVendorFaction()) {
			return faction;
		}
		return a_actor->GetActorRuntimeData().vendorFaction;
#else
		return a_actor->GetVendorFaction();
#endif
	}

	//LoadingMenu's destination location.
	[[nodiscard]] inline RE::BGSLocation* LoadingMenuLocation(RE::LoadingMenu* a_menu)
	{
#if SKY_COMMONLIB == SKY_COMMONLIB_NG
		return a_menu->GetRuntimeData().currentLocation;
#else
		return a_menu->currentLocation;
#endif
	}

	//Biped slot bits. NG returns an EnumSet, libxse a plain enum.
	[[nodiscard]] inline std::uint32_t SlotMaskBits(const RE::BGSBipedObjectForm* a_form)
	{
#if SKY_COMMONLIB == SKY_COMMONLIB_NG
		return a_form->GetSlotMask().underlying();
#else
		return static_cast<std::uint32_t>(a_form->GetSlotMask());
#endif
	}
}
