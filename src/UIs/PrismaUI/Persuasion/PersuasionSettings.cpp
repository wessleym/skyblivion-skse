#include "PersuasionSettings.h"
#include "PersuasionPerks.h"

namespace Persuasion
{
	void Settings::ReloadIfChanged()
	{
		s_table.ReloadIfChanged();
		Perks::ReloadIfChanged();
	}

	void Settings::Resolve()
	{
		s_table.Load();
		Perks::Load();
	}
}
