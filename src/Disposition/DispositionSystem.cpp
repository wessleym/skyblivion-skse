#include "DispositionSystem.h"
#include "TESFactions.h"
#include "TESGlobals.h"
#include "TESNPCActivateHook.h"
#include "TESRaces.h"
#include <ClibUtil/RNG.hpp>
#include <cmath>

void DispositionSystem::Initialize() {
	TESNPCActivateHook::Apply();
	Log::INFO("DispositionSystem: Applied TESNPCActivateHook");
}

void DispositionSystem::OnDataLoaded() {
	Log::INFO("DispositionSystem: Looking up Factions, Globals, and Races...");
	LookUpFactions();
	LookUpGlobals();
	LookUpRaces();
	Log::INFO("DispositionSystem: Complete");
}

void DispositionSystem::LookUpFactions() {
	LookUpForm("SKYBDarkBrotherhoodFaction", TESFactions::TES4DarkBrotherhood, "faction");
	LookUpForm("SKYBFightersGuildFaction", TESFactions::TES4FightersGuild, "faction");
	LookUpForm("SKYBMagesGuildFaction", TESFactions::TES4MagesGuild, "faction");
	LookUpForm("SKYBThievesGuildFaction", TESFactions::TES4ThievesGuild, "faction");
}

void DispositionSystem::LookUpGlobals() {
	LookUpForm("SKYBFame", TESGlobals::iFame, "global");
	LookUpForm("SKYBInfamy", TESGlobals::iInfamy, "global");
}

void DispositionSystem::LookUpRaces() {
	LookUpForm("ArgonianRace", TESRaces::ArgonianRace, "race");
	LookUpForm("ArgonianRaceVampire", TESRaces::ArgonianRaceVampire, "race");
	LookUpForm("BretonRace", TESRaces::BretonRace, "race");
	LookUpForm("BretonRaceVampire", TESRaces::BretonRaceVampire, "race");
	LookUpForm("DarkElfRace", TESRaces::DarkElfRace, "race");
	LookUpForm("DarkElfRaceVampire", TESRaces::DarkElfRaceVampire, "race");
	LookUpForm("HighElfRace", TESRaces::HighElfRace, "race");
	LookUpForm("HighElfRaceVampire", TESRaces::HighElfRaceVampire, "race");
	LookUpForm("ImperialRace", TESRaces::ImperialRace, "race");
	LookUpForm("ImperialRaceVampire", TESRaces::ImperialRaceVampire, "race");
	LookUpForm("KhajiitRace", TESRaces::KhajiitRace, "race");
	LookUpForm("KhajiitRaceVampire", TESRaces::KhajiitRaceVampire, "race");
	LookUpForm("NordRace", TESRaces::NordRace, "race");
	LookUpForm("NordRaceVampire", TESRaces::NordRaceVampire, "race");
	LookUpForm("OrcRace", TESRaces::OrcRace, "race");
	LookUpForm("OrcRaceVampire", TESRaces::OrcRaceVampire, "race");
	LookUpForm("RedguardRace", TESRaces::RedguardRace, "race");
	LookUpForm("RedguardRaceVampire", TESRaces::RedguardRaceVampire, "race");
	LookUpForm("WoodElfRace", TESRaces::WoodElfRace, "race");
	LookUpForm("WoodElfRaceVampire", TESRaces::WoodElfRaceVampire, "race");
}

// Calculates the actor's disposition toward the player. Runs only the first time an
// actor loads in a save (gated by the caller checking disp == 0).
float DispositionSystem::CalcDisposition(RE::Actor* npc) {
	auto player = RE::PlayerCharacter::GetSingleton();
	if (!player || !npc) {
		return 40.0f;
	}

	//player and npc are non null.
	auto playerAVOwner = REBridge::AVOwner(player);
	auto npcAVOwner = REBridge::AVOwner(npc);
	//if (!playerAVOwner || !npcAVOwner) {//Unreachable since playerAVOwner npcAVOwner are non null
	//    return 40.0f;
	//}

	auto NormalizeRace = [](RE::TESRace* race) -> RE::TESRace* {
		if (race == TESRaces::HighElfRaceVampire) return TESRaces::HighElfRace;
		if (race == TESRaces::DarkElfRaceVampire) return TESRaces::DarkElfRace;
		if (race == TESRaces::WoodElfRaceVampire) return TESRaces::WoodElfRace;
		if (race == TESRaces::BretonRaceVampire) return TESRaces::BretonRace;
		if (race == TESRaces::NordRaceVampire) return TESRaces::NordRace;
		if (race == TESRaces::KhajiitRaceVampire) return TESRaces::KhajiitRace;
		if (race == TESRaces::ImperialRaceVampire) return TESRaces::ImperialRace;
		if (race == TESRaces::RedguardRaceVampire) return TESRaces::RedguardRace;
		if (race == TESRaces::OrcRaceVampire) return TESRaces::OrcRace;
		if (race == TESRaces::ArgonianRaceVampire) return TESRaces::ArgonianRace;
		return race;
		};

	auto playerRace = player->GetRace();
	auto npcRace = npc->GetRace();
	if (!playerRace || !npcRace) {
		return 40.0f;
	}

	auto playerRaceNormalized = NormalizeRace(playerRace);
	auto npcRaceNormalized = NormalizeRace(npcRace);

	auto playerBase = player->GetActorBase();
	const int playerSex = playerBase ? static_cast<int>(playerBase->GetSex()) : 0;

	float disp = 40.0f;

	if (playerRaceNormalized == TESRaces::ArgonianRace) {
		disp -= 10.0f;
	}
	else if (playerRaceNormalized == TESRaces::WoodElfRace && playerSex == 0) {
		disp -= 10.0f;
	}
	else if (playerRaceNormalized == TESRaces::DarkElfRace && playerSex == 0) {
		disp -= 10.0f;
	}
	else if (playerRaceNormalized == TESRaces::NordRace) {
		disp -= 10.0f;
	}
	else if (playerRaceNormalized == TESRaces::RedguardRace && playerSex == 0) {
		disp -= 10.0f;
	}
	else if (playerRaceNormalized == TESRaces::ImperialRace) {
		disp += 10.0f;
	}
	else if (playerRaceNormalized == TESRaces::OrcRace) {
		disp += playerSex == 0 ? -15.0f : -10.0f;
	}

	if (playerRaceNormalized == npcRaceNormalized) {
		disp += 5.0f;
	}

	if (playerRaceNormalized == TESRaces::OrcRace) {
		disp -= 5.0f;
	}

	if (playerRaceNormalized == TESRaces::DarkElfRace) {
		disp -= 5.0f;

		if (npcRaceNormalized == TESRaces::HighElfRace) {
			disp -= 5.0f;
		}
		else if (npcRaceNormalized == TESRaces::ArgonianRace) {
			disp -= 5.0f;
		}
	}

	if (playerRaceNormalized == TESRaces::HighElfRace) {
		disp -= 5.0f;

		if (npcRaceNormalized == TESRaces::HighElfRace) {
			disp += 5.0f;
		}
		else if (npcRaceNormalized == TESRaces::NordRace) {
			disp += 5.0f;
		}
		else if (npcRaceNormalized == TESRaces::OrcRace) {
			disp += 5.0f;
		}
		else if (npcRaceNormalized == TESRaces::ArgonianRace) {
			disp -= 5.0f;
		}
		else if (npcRaceNormalized == TESRaces::DarkElfRace) {
			disp -= 5.0f;
		}
		else if (npcRaceNormalized == TESRaces::KhajiitRace) {
			disp -= 5.0f;
		}
	}

	if (playerRaceNormalized == TESRaces::RedguardRace) {
		if (npcRaceNormalized == TESRaces::BretonRace) {
			disp -= 5.0f;
		}
		else if (npcRaceNormalized == TESRaces::ImperialRace) {
			disp -= 5.0f;
		}
	}

	if (REBridge::ActorStateOf(player)->IsWeaponDrawn()) {
		disp -= 10.0f;
	}

	const float speechDiff =
		(playerAVOwner->GetActorValue(RE::ActorValue::kSpeech) -
			npcAVOwner->GetActorValue(RE::ActorValue::kSpeech)) /
		4.0f;

	disp += speechDiff;

	if (disp > 100.0f) {
		disp = 100.0f;
	}
	else if (disp < 0.0f) {
		disp = 0.0f;
	}

	static thread_local clib_util::RNG rng;
	disp += rng.generate<float>(-15.0f, 15.0f);

	return disp;
}

int DispositionSystem::GetDispositionActorValue(RE::Actor* actor) {
	auto* avOwner = REBridge::AVOwner(actor);
	int disposition = static_cast<int>(avOwner->GetActorValue(kDispositionAV));
	return disposition;
}

void DispositionSystem::SetDispositionActorValue(RE::Actor* actor, float value, bool force) {
	PapyrusSetActorValue(actor, kDispositionAVName, value, force);
}

// SKSE data changes to actor values don't persist, so next time the player loads the game
// any changes we made through SKSE would be gone. We use SKSE to calc data and papyrus to
// set it (papyrus persists).
void DispositionSystem::PapyrusSetActorValue(RE::TESObjectREFR* a_ref, RE::BSFixedString valueName, float value, bool force) {
	auto actor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
	if (!actor || valueName.empty()) {
		return;
	}

	auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
	auto policy = vm->GetObjectHandlePolicy();
	auto handle = policy->GetHandleForObject(actor->FORMTYPE, actor);

	auto args = RE::MakeFunctionArguments(RE::BSFixedString{ valueName }, float{ value });
	RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
	const auto functionName = force ? "ForceActorValue" : "SetActorValue";
	vm->DispatchMethodCall(handle, "Actor", functionName, args, cb);
	Log::DEBUG("Actor.{}({})", functionName, value);
}

void DispositionSystem::SetInitialDisposition(RE::Actor* actor, std::string_view edid) {
	if (!actor) {
		Log::WARN("DispositionSystem::SetInitialDisposition: actor was null.");
		return;
	}

	auto avOwner = REBridge::AVOwner(actor);

	const auto currentDisp = avOwner->GetActorValue(kDispositionAV);

	Log::DEBUG("actor {} disp == {}", edid, currentDisp);

	// If current disp == 0.0, this actor hasn't had disposition set yet this save game.
	if (currentDisp == 0.0f) {
		auto disposition = CalcDisposition(actor);

		// Mark NPC disposition so it isn't needlessly recalced if it happened to be 0.0f first try.
		if (disposition < 1.0f) {
			disposition = 1.0f;
		}

		disposition = std::round(disposition);
		// Papyrus call so changes persist.
		SetDispositionActorValue(actor, disposition);

		Log::DEBUG("set actor {} disp to {}", edid, disposition);
	}
}
