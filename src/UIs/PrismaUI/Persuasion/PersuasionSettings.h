#pragma once
// The persuasion wheel's tunables: the [Persuasion.*] sections of Data\SKSE\Plugins\SkyblivionSKSE.ini
// (PluginIni); the built-in defaults come from PersuasionSettingsTable.h. They are taken at kDataLoaded and
// again whenever the file's write time changes (checked on every save load and every wheel open). Values
// are not saved.
//
// OVERRIDES: the settings menu (Progression > Persuasion) owns a few values per character. Its store
// calls SetOverride on every game load and change; an override replaces the INI value for that key
// and survives INI reloads.
//
// PERKS: the [PersuasionPerks.*] sections (PersuasionPerks) change a copy of these values per
// conversation; see PersuasionRules::Resolve. Nothing here knows about the player.
#include <string_view>

#include "PersuasionSettingsTable.h"
#include "Platform/IniTable.h"

namespace Persuasion
{
	class Settings
	{
		using Table = IniTable<kSettingDefs>;

	public:
		static constexpr std::size_t kCount = Table::kCount;
		using Values = Table::Values;

		// kDataLoaded: read the INI (and the perk rules through PersuasionPerks).
		static void Resolve();
		// Re-read the INI if it changed on disk since the last read (the perk rules too).
		static void ReloadIfChanged();

		// The settings menu's store: set one override.
		static bool SetOverride(std::string_view a_section, std::string_view a_key, float a_value) { return s_table.SetOverride(a_section, a_key, a_value); }

		[[nodiscard]] static float Get(P a_id) { return s_table.Get(static_cast<std::size_t>(a_id)); }
		[[nodiscard]] static bool  On(P a_id) { return Get(a_id) >= 0.5f; }
		// Every value, INI with the menu's overrides laid over: the starting point of a conversation's rules.
		[[nodiscard]] static const Values& All() { return s_table.All(); }

		// ("Decay", "WhileOpen") -> the index into Values / the enum, or -1 when the table has no such row.
		[[nodiscard]] static int               Find(std::string_view a_section, std::string_view a_key) { return Table::Find(a_section, a_key); }
		[[nodiscard]] static const SettingDef& Def(std::size_t a_index) { return kSettingDefs[a_index]; }

		// [Persuasion.<section>] in SkyblivionSKSE.ini
		static constexpr std::string_view kIniFeature = "Persuasion";

	private:
		static inline Table s_table{ kIniFeature, "persuasion" };
	};
}
