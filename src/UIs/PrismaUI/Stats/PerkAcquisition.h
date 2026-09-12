#pragma once

#include "PerkIndex.h"

namespace Stats {

	//Spends one perk point. The view owns rank order and prerequisites.
	//This checks only what the game owns: an available point, and whether the perk is held.
	class PerkAcquisition {
	public:
		//True only when the perk was granted. Refusals are logged with the reason.
		static bool Acquire(const PerkReference& a_perk);
	};

}
