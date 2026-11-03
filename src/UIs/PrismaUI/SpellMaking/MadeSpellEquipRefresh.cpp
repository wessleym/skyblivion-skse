#include "MadeSpellEquipRefresh.h"
#include "SpellMakingStore.h"

namespace SpellMaking {

	int MadeSpellEquipRefresh::s_delay = 0;
	int MadeSpellEquipRefresh::s_generation = 0;

	//Starts a frame countdown.
	void MadeSpellEquipRefresh::Schedule() {
		++s_generation;//invalidate any refresh still pumping from an earlier load
		s_delay = 150;//let the load fully settle before re-equipping
		Pump(s_generation);
	}

	//One tick of the countdown
	//Re-posts itself to the SKSE task interface (one run per frame) until s_delay reaches zero,
	//then calls Run().
	//generation pins the generation this chain belongs to, so a newer Schedule() silently cancels it.
	void MadeSpellEquipRefresh::Pump(int generation) {
		auto* task = SKSE::GetTaskInterface();
		if (!task) {
			return;
		}
		task->AddTask([generation]() {
			if (generation != s_generation) {
				return;//A newer load superseded this refresh.
			}
			if (s_delay > 0) {
				--s_delay;
				Pump(generation);
				return;
			}
			Run();
		});
	}

	//Re-equips any made (placeholder-pool) spell currently in the player's hands,
	//which rebuilds the readied-hand effect the game load left missing.
	void MadeSpellEquipRefresh::Run() {
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* equipManager = RE::ActorEquipManager::GetSingleton();
		if (!player || !equipManager) {
			return;
		}
		const struct {
			bool       leftHand;
			RE::FormID slotID;
		} hands[] = {
			{ false, 0x00013F42 },//right hand
			{ true, 0x00013F43 }//left hand
		};
		for (const auto& hand : hands) {
			auto* equipped = player->GetEquippedObject(hand.leftHand);
			auto* spell = equipped ? equipped->As<RE::SpellItem>() : nullptr;
			if (!spell || !SpellMakingStore::IsPlaceholder(spell)) {
				continue;
			}
			if (auto* equipSlot = RE::TESForm::LookupByID<RE::BGSEquipSlot>(hand.slotID)) {
				//Re-equip to rebuild the readied-hand effect.
				equipManager->EquipSpell(player, spell, equipSlot);
			}
		}
	}

}
