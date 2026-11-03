#include <Windows.h> //ClibUtil/EditorID.hpp calls GetModuleHandleA. The libxse backend does not include it.
#include "DispositionSystem.h"
#include "TESFactions.h"
#include "TESGlobals.h"
#include "CharacterLoad3DHook.h"
#include "TESRaces.h"
#include <ClibUtil/EditorID.hpp>
#include <ClibUtil/RNG.hpp>
#include <cmath>

void DispositionSystem::Initialize() {
	CharacterLoad3DHook::Apply();
	Log::INFO("DispositionSystem: Applied CharacterLoad3DHook");
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
// actor loads in a save (the caller skips actors whose disposition actor value is non-zero).
float DispositionSystem::CalcDisposition(RE::Actor* npc) {
	auto player = RE::PlayerCharacter::GetSingleton();
	if (!player || !npc) {
		return 40.0f;
	}

	//player and npc are non null.
	auto playerAVOwner = REBridge::AVOwner(player);
	auto npcAVOwner = REBridge::AVOwner(npc);

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

	float disposition = 40.0f;

	if (playerRaceNormalized == TESRaces::ArgonianRace) {
		disposition -= 10.0f;
	}
	else if (playerRaceNormalized == TESRaces::WoodElfRace && playerSex == 0) {
		disposition -= 10.0f;
	}
	else if (playerRaceNormalized == TESRaces::DarkElfRace && playerSex == 0) {
		disposition -= 10.0f;
	}
	else if (playerRaceNormalized == TESRaces::NordRace) {
		disposition -= 10.0f;
	}
	else if (playerRaceNormalized == TESRaces::RedguardRace && playerSex == 0) {
		disposition -= 10.0f;
	}
	else if (playerRaceNormalized == TESRaces::ImperialRace) {
		disposition += 10.0f;
	}
	else if (playerRaceNormalized == TESRaces::OrcRace) {
		disposition += playerSex == 0 ? -15.0f : -10.0f;
	}

	if (playerRaceNormalized == npcRaceNormalized) {
		disposition += 5.0f;
	}

	if (playerRaceNormalized == TESRaces::OrcRace) {
		disposition -= 5.0f;
	}

	if (playerRaceNormalized == TESRaces::DarkElfRace) {
		disposition -= 5.0f;

		if (npcRaceNormalized == TESRaces::HighElfRace) {
			disposition -= 5.0f;
		}
		else if (npcRaceNormalized == TESRaces::ArgonianRace) {
			disposition -= 5.0f;
		}
	}

	if (playerRaceNormalized == TESRaces::HighElfRace) {
		disposition -= 5.0f;

		if (npcRaceNormalized == TESRaces::HighElfRace) {
			disposition += 5.0f;
		}
		else if (npcRaceNormalized == TESRaces::NordRace) {
			disposition += 5.0f;
		}
		else if (npcRaceNormalized == TESRaces::OrcRace) {
			disposition += 5.0f;
		}
		else if (npcRaceNormalized == TESRaces::ArgonianRace) {
			disposition -= 5.0f;
		}
		else if (npcRaceNormalized == TESRaces::DarkElfRace) {
			disposition -= 5.0f;
		}
		else if (npcRaceNormalized == TESRaces::KhajiitRace) {
			disposition -= 5.0f;
		}
	}

	if (playerRaceNormalized == TESRaces::RedguardRace) {
		if (npcRaceNormalized == TESRaces::BretonRace) {
			disposition -= 5.0f;
		}
		else if (npcRaceNormalized == TESRaces::ImperialRace) {
			disposition -= 5.0f;
		}
	}

	// This was previously used but is no longer useful since it would affect disposition
	// at the moment an NPC's 3D loads instead of at the moment the conversation starts.
	// if (REBridge::ActorStateOf(player)->IsWeaponDrawn()) {
	//	 disposition -= 10.0f;
	// }

	const float speechDiff =
		(playerAVOwner->GetActorValue(RE::ActorValue::kSpeech) -
			npcAVOwner->GetActorValue(RE::ActorValue::kSpeech)) /
		4.0f;

	disposition += speechDiff;

	if (disposition > 100.0f) {
		disposition = 100.0f;
	}
	else if (disposition < 0.0f) {
		disposition = 0.0f;
	}

	static thread_local clib_util::RNG rng;
	disposition += rng.generate<float>(-15.0f, 15.0f);

	return disposition;
}

int DispositionSystem::GetDispositionActorValue(RE::Actor* actor) {
	auto* avOwner = REBridge::AVOwner(actor);
	int disposition = static_cast<int>(avOwner->GetActorValue(kDispositionAV));
	return disposition;
}

void DispositionSystem::SetDispositionActorValue(RE::Actor* actor, float value, bool force) {
	PapyrusSetActorValue(actor, kDispositionAVName, value, force);
}

// Actor values set directly through SKSE are not saved, so they are lost when the game is
// reloaded. The value is calculated in SKSE and set through a Papyrus call, which persists.
void DispositionSystem::PapyrusSetActorValue(RE::TESObjectREFR* a_ref, RE::BSFixedString valueName, float value, bool force) {
	auto actor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
	if (!actor || valueName.empty()) {
		return;
	}

	auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
	auto policy = vm->GetObjectHandlePolicy();
	auto handle = policy->GetHandleForObject(actor->FORMTYPE, actor);

	auto args = RE::MakeFunctionArguments(RE::BSFixedString{ valueName }, float{ value });
	RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
	const auto functionName = force ? "ForceActorValue" : "SetActorValue";
	vm->DispatchMethodCall(handle, "Actor", functionName, args, callback);
	Log::DEBUG("Actor.{}({})", functionName, value);
}

bool DispositionSystem::RaceAllowsPlayerDialogue(RE::Actor* actor) {
	auto* race = actor->GetRace();
	return race && race->data.flags.any(RE::RACE_DATA::Flag::kAllowPCDialogue);
}

void DispositionSystem::SetInitialDisposition(RE::Actor* actor) {
	if (!actor) {
		Log::WARN("DispositionSystem::SetInitialDisposition: actor was null.");
		return;
	}
	
	if (!RaceAllowsPlayerDialogue(actor)) {
		return;
	}

	auto avOwner = REBridge::AVOwner(actor);

	// Non-zero means this actor already had disposition set this save game.
	if (avOwner->GetActorValue(kDispositionAV) != 0.0f) {
		return;
	}

	auto disposition = CalcDisposition(actor);

	// Clamped to at least 1 so a calculated 0 is not read as unset and recalculated.
	if (disposition < 1.0f) {
		disposition = 1.0f;
	}

	disposition = std::round(disposition);
	// Papyrus call so changes persist.
	SetDispositionActorValue(actor, disposition);

	auto* actorBase = actor->GetActorBase();
	const std::string actorBaseEditorID = actorBase ? clib_util::editorID::get_editorID(actorBase) : std::string{};
	auto* race = actor->GetRace();
	const std::string raceEditorID = race ? clib_util::editorID::get_editorID(race) : std::string{};
	Log::INFO("Set initial disposition for {} ({:08X}) [{}] to {}", actorBaseEditorID, actor->GetFormID(), raceEditorID, disposition);
}
