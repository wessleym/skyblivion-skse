#include "PerksMenu.h"
#include "PerkDataStore.h"
#include "PerksContract.h"
#include "Papyrus/ModEvents.h"

namespace PerksView {

	bool PerksMenu::IsAvailable() {
		//The game's own file system, so an archived SWF counts as well as a loose one.
		RE::BSResourceNiBinaryStream swf{ PerksContract::Menu::SwfFile };
		if (!swf.good()) {
			return false;
		}
		PerkDataStore::LoadOnce();
		return !PerkDataStore::Trees().empty();
	}

	void PerksMenu::Open() {
		ModEvents::Send(PerksContract::ModEvent::OpenCustomMenu, PerksContract::Menu::Tab);
		Log::INFO("PerksMenu: Sent {} '{}'.", PerksContract::ModEvent::OpenCustomMenu, PerksContract::Menu::Tab);
	}

}
