#pragma once

//The sigil stone menu's data feeds. The rows are byte-compatible with the Papyrus builders they replace,
//so the SWF cannot tell the producers apart. The enchant and gem operations are in Sigil/SigilContract.h.
namespace Sigil::SigilMenuContract {
	//The sigil menu is hosted in CustomMenu.
	inline constexpr const char* MenuName = "CustomMenu";

	//SKYBSigilEnchant.psc -> C++:
	namespace ModEvent {
		//Pushes AsPath::AddGear and AsPath::GearDone, then answers GearFed.
		inline constexpr const char* RequestGear = "SKYBSigilGearRequest";
		//strArg: "stonesListID,weaponEnchantmentsListID,armorEnchantmentsListID,gemsListID^pad^selection".
		//The IDs are the psc's own properties, never hard-coded, so an ESM renumber cannot break them.
		//Pushes AsPath::SetStones, then answers StonesFed.
		inline constexpr const char* RequestStones = "SKYBSigilStoneRequest";
	}

	//C++ -> Papyrus:
	namespace Answer {
		//strArg: "count^id,id,...", the gear FormIDs in slot order, so the psc can resolve the chosen slot.
		inline constexpr const char* GearFed = "SKYBSigilGearFed";
		//strArg: the stone row count. The psc closes the menu on 0.
		inline constexpr const char* StonesFed = "SKYBSigilStonesFed";
	}

	//C++ -> ActionScript:
	namespace AsPath {
		//("slot|name|value|damage|armor|type;...") type: W weapon, A armor, J jewelry.
		inline constexpr const char* AddGear = "_root.SigilMenu_mc.addGear";
		//(count)
		inline constexpr const char* GearDone = "_root.SigilMenu_mc.gearDone";
		//(rows, pad[, selection]) Each row:
		//"index|name|count|tier|weaponEffect|weaponMagnitude|weaponDuration|armorEffect|armorMagnitude|armorDuration|gemCount|gemName".
		inline constexpr const char* SetStones = "_root.SigilMenu_mc.setStones";
	}
}
