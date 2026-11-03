#pragma once

//Deferred re-equip of the player's in-hand made spells after a game load.
//On a save's load the base game restores the equipped placeholder before the co-save refills its effects.
//So the readied-hand cast art never attaches.
//Re-equipping from an SKSE task, after the co-save load returns, rebuilds that visual.
//It only helps because PopulateSpell clears avEffectSetting; otherwise the re-equip rebuilds the stale art.
namespace SpellMaking {

	class MadeSpellEquipRefresh {
	public:
		// Schedules the deferred refresh. Call after a co-save load restores made spells.
		static void Schedule();

	private:
		static void Run();
	};

}
