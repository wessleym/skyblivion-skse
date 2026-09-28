#pragma once

#include <string>

namespace Sigil {

	//Enchants one carried copy of a weapon or apparel item without a visible world drop.
	//
	//Every case (plain, worn, tempered) goes through the engine's own drop path, all within one task:
	//RemoveItem with kDropping, 2000 units under the player, then enchant the dropped reference's extra list,
	//then PickUpObject. The reference's 3D never loads, so nothing renders. This is the Papyrus
	//DropObject -> SetEnchantment -> AddItem flow, completed within one frame.
	//
	//Two constraints, each established by a crash:
	//	Never hand-construct an ExtraDataList. Its default constructor is not exported, and on AE 1.6.629+
	//	BaseExtraList carries a vtable, so a zeroed allocation crashed inside the engine's Add.
	//	Only the engine's own lists are mutated here.
	//	Never mutate a worn instance's list around a queued ActorEquipManager::UnequipObject.
	//	An equipped weapon's unequip corrupted the mutated list. kDropping unequips through the proper path first.
	class SigilEnchanting {
	public:
		enum class Result {
			Enchanted,
			//Every carried copy is already enchanted.
			AlreadyEnchanted,
			//Not carried, or a form did not resolve.
			Missing
		};

		//Prefers a plain copy; otherwise a clean special instance (tempered or worn), preferring one not worn.
		//name may be empty, which keeps the item's name.
		static Result Apply(RE::PlayerCharacter* player, RE::FormID gearID, RE::FormID enchantmentID, std::uint16_t charge, const std::string& name);
	};

}
