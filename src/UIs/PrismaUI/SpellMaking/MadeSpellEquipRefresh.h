#pragma once

//Deferred re-equip of the player's in-hand made spells after a game load.
//On a save's load the base game restores the equipped placeholder before the co-save refills its effects.
//So the readied-hand cast art never attaches.
//Scheduling a re-equip once the load has settled rebuilds that visual.
namespace SpellMaking {

	class MadeSpellEquipRefresh {
	public:
		// Schedules the deferred refresh. Call after a co-save load restores made spells.
		static void Schedule();

	private:
		static void Pump(int generation);
		static void Run();

		static int s_delay;//frames left before the refresh fires
		static int s_generation;//bumped per Schedule() so a newer load supersedes an older
	};

}
