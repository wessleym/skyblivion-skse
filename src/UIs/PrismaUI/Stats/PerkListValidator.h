#pragma once

namespace Stats {

	//Compares the perks the view registered against Skyblivion.esm's SKYBPerks<Attribute> lists,
	//indexed by SKYBPerkTrees.
	//Those lists carry no structure, but state attribute membership independently of the trees.
	//
	//Logs only: neither source overrules the other; added perks would make disagreements routine.
	class PerkListValidator {
	public:
		//No-op when the lists are absent.
		static void Validate();
	};

}
