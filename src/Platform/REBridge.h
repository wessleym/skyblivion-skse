#pragma once

//RE:: API bridges
//CommonLibSSE-NG builds one DLL for Skyrim SE, AE and VR. Members whose offset differs between runtimes
//are reached through GetXxxRuntimeData() accessors, and multiply-inherited bases through AsXxx()
//accessors. Some of those accessors read the wrong memory on VR, and some direct accesses compile but
//use a fixed offset that is wrong at run time. These helpers are the project's one way to reach each.

#include <cmath>
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

	//The reference behind a handle (ActorHandle, ObjectRefHandle, ProjectileHandle), or null once it is gone.
	//Use this instead of get() on a non-const handle: NG's non-const get() calls address-library id 12785
	//(get the pointer and clear a stale handle), which VR's library lacks, so its first call is fatal on VR.
	//The const get() calls id 12204, which every runtime's library has.
	template <class Handle>
	[[nodiscard]] inline auto Lookup(const Handle& a_handle)
	{
		return a_handle.get();
	}

	//The console's selected reference. NG's Console::GetSelectedRef() uses the non-const get() (see Lookup).
	[[nodiscard]] inline RE::NiPointer<RE::TESObjectREFR> ConsoleSelectedRef()
	{
		const auto handle = RE::Console::GetSelectedRefHandle();
		return Lookup(handle);
	}

	//An inventory entry's display name, as ExtraDataList::GetDisplayName() returns it. NG's GetDisplayName()
	//resolves an ExtraReferenceHandle (a persistent reference, e.g. a quest item) with the non-const get()
	//(see Lookup), so on VR such entries are named here, with the same lookup order.
	[[nodiscard]] inline const char* DisplayName(RE::ExtraDataList* a_list, RE::TESBoundObject* a_obj)
	{
		if (!a_list) {
			return a_obj->GetName();
		}
		if (REL::Module::IsVR()) {
			if (auto* xRef = a_list->GetByType<RE::ExtraReferenceHandle>()) {
				//The original reference's name record first, then the entry's own.
				const auto ref = Lookup(xRef->containerRef);
				RE::ExtraTextDisplayData* xText = ref && !ref->IsDeleted() ? ref->extraList.GetByType<RE::ExtraTextDisplayData>() : nullptr;
				if (!xText) {
					xText = a_list->GetByType<RE::ExtraTextDisplayData>();
				}
				//As NG does: an entry whose health is not 1.0 gets a name record, which names its temper level.
				const auto* xHealth = a_list->GetByType<RE::ExtraHealth>();
				const float health = xHealth ? xHealth->health : 1.0f;
				if (!xText && std::abs(health - 1.0f) >= 0.001f) {
					xText = new RE::ExtraTextDisplayData();
					a_list->Add(xText);
				}
				const char* name = xText ? xText->GetDisplayName(a_obj, health) : nullptr;
				return name && *name ? name : a_obj->GetName();
			}
		}
		return a_list->GetDisplayName(a_obj);
	}

	//Records the reference's lock change so the save keeps it (after unlocking it in code). NG's
	//TESObjectREFR::AddLockChange() uses address-library id 19110, which VR's library lacks. The VR address
	//0x297310 comes from the SE-to-VR binary match and disassembles to the same code as SE 1.5.97's id 19110
	//(checked 2026-11-05).
	inline void AddLockChange(RE::TESObjectREFR* a_ref)
	{
		using func_t = void (*)(RE::TESObjectREFR*);
		static REL::Relocation<func_t> func{ REL::VariantID(19110, 19512, 0x297310) };
		func(a_ref);
	}

	//The actor value of this name, or kNone for a name the game does not know. The game's lookup (id 26570)
	//returns kTotal (164) for an unknown name, not kNone, and an actor value of 164 reads past the end of the
	//game's actor-value table (checked in the SE 1.5.97 disassembly, 2026-11-07).
	inline RE::ActorValue ActorValueByName(const char* a_name)
	{
		auto* list = a_name && *a_name ? RE::ActorValueList::GetSingleton() : nullptr;
		const auto av = list ? list->LookupActorValueByName(a_name) : RE::ActorValue::kNone;
		return static_cast<std::uint32_t>(av) < static_cast<std::uint32_t>(RE::ActorValue::kTotal) ? av : RE::ActorValue::kNone;
	}
}
