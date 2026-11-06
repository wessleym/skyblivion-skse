#pragma once

//RE:: API bridges
//CommonLibSSE-NG builds one DLL for Skyrim SE, AE and VR. Members whose offset differs between runtimes
//are reached through GetXxxRuntimeData() accessors, and multiply-inherited bases through AsXxx()
//accessors. Some of those accessors read the wrong memory on VR, and some direct accesses compile but
//use a fixed offset that is wrong at run time. These helpers are the project's one way to reach each.

#include <cstddef>
#include <cstdint>

namespace REBridge
{
	//NG's GetPlayerRuntimeData() knows only the SE and AE positions: on VR it reads the wrong memory.
	//VR keeps these fields in its own block, reached through GetVRInfoRuntimeData() (null outside VR).
	//So PlayerCharacter fields get one bridge each, and there is no whole-struct PlayerData bridge.
	//The VR block sits at 0xFE0; these are the absolute offsets CommonLib documents for its fields.
	static_assert(offsetof(RE::PlayerCharacter::VR_INFO_RUNTIME_DATA, skills) == 0x10B0 - 0xFE0);
	static_assert(offsetof(RE::PlayerCharacter::VR_INFO_RUNTIME_DATA, amountStolenSold) == 0x100C - 0xFE0);

	//The player's per-skill experience and level thresholds.
	[[nodiscard]] inline RE::PlayerCharacter::PlayerSkills* PlayerSkills(RE::PlayerCharacter* a_player)
	{
		if (REL::Module::IsVR()) {
			return a_player->GetVRInfoRuntimeData()->skills;
		}
		return a_player->GetPlayerRuntimeData().skills;
	}

	//Gold from stolen goods sold.
	[[nodiscard]] inline std::int32_t& AmountStolenSold(RE::PlayerCharacter* a_player)
	{
		if (REL::Module::IsVR()) {
			return a_player->GetVRInfoRuntimeData()->amountStolenSold;
		}
		return a_player->GetPlayerRuntimeData().amountStolenSold;
	}

	//PlayerCharacter::GameStatsData: perkCount, difficulty, murder counts. NG's accessor includes VR's position.
	[[nodiscard]] inline auto& GameStats(RE::PlayerCharacter* a_player)
	{
		return a_player->GetGameStatsData();
	}

	//Actor's ActorValueOwner base is at +0xB0 before 1.6.629 (SE, early AE, VR) and +0xB8 from 1.6.629.
	//A static_cast, or an ActorValueOwner method called directly on an Actor*, uses the compile-time offset
	//+0x90, so the call goes through an invalid vtable pointer. AsActorValueOwner() selects the offset at run time.
	[[nodiscard]] inline RE::ActorValueOwner* AVOwner(RE::Actor* a_actor)
	{
		return a_actor->AsActorValueOwner();
	}

	//Actor's ActorState base has the same hazard; AsActorState() selects its offset at run time.
	[[nodiscard]] inline RE::ActorState* ActorStateOf(RE::Actor* a_actor)
	{
		return a_actor->AsActorState();
	}

	//The actor's vendor faction, calculated when not yet cached.
	//NG's GetVendorFaction returns the value it read before recalculating, so an uncached actor reads as null once.
	[[nodiscard]] inline RE::TESFaction* VendorFaction(RE::Actor* a_actor)
	{
		if (auto* faction = a_actor->GetVendorFaction()) {
			return faction;
		}
		return a_actor->GetActorRuntimeData().vendorFaction;
	}

	//LoadingMenu's destination location.
	[[nodiscard]] inline RE::BGSLocation* LoadingMenuLocation(RE::LoadingMenu* a_menu)
	{
		return a_menu->GetRuntimeData().currentLocation;
	}
}
