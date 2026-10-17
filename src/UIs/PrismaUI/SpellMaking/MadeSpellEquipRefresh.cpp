#include "MadeSpellEquipRefresh.h"
#include "SpellMakingStore.h"
#include "Resources.h"

namespace SpellMaking {

	void MadeSpellEquipRefresh::Schedule() {
		auto* task = SKSE::GetTaskInterface();
		assert(task);
		task->AddTask([]() { Run(); });
	}

	//Re-equips any made (placeholder-pool) spell currently in the player's hands,
	//which rebuilds the readied-hand effect the game load left missing.
	void MadeSpellEquipRefresh::Run() {
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* equipManager = RE::ActorEquipManager::GetSingleton();

		assert(player && equipManager);

		if (!player || !equipManager) {
			return;
		}
		const struct {
			bool       leftHand;
			RE::BGSEquipSlot* slot;
		} hands[] = {
			{ false, Resources::rightHandSlot },//right hand
			{ true, Resources::leftHandSlot }//left hand
		};
		for (const auto& hand : hands) {
			auto* equipped = player->GetEquippedObject(hand.leftHand);
			auto* spell = equipped ? equipped->As<RE::SpellItem>() : nullptr;
			if (!spell || !SpellMakingStore::IsPlaceholder(spell)) {
				continue;
			}

			//Re-equip to rebuild the readied-hand effect.
			equipManager->EquipSpell(player, spell, hand.slot);
		}
	}

}
