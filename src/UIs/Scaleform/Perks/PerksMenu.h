#pragma once

namespace PerksView {

	//Opens the SKYBPerksView perk menu through its existing entry point:
	//the SKYBUIOpenCustomMenu mod event with "PerkMenu", which Papyrus handles.
	class PerksMenu {
	public:
		//True when the SWF is installed, loose or in an archive, and at least one tree loaded.
		[[nodiscard]] static bool IsAvailable();

		static void Open();
	};

}
