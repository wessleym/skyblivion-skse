#pragma once
// The persuasion wheel's tunables, read from Data\SKSE\Plugins\SKYBPersuasion.ini; the built-in
// defaults come from PersuasionSettingsTable.h. The file is read at kDataLoaded and re-read whenever
// its write time changes (checked on every save load and every wheel open). Values are not saved.
//
// OVERRIDES: the settings menu (Progression > Persuasion) owns a few values per character. Its store
// calls SetOverride on every game load and change; an override replaces the INI value for that key
// until it is cleared, and survives INI reloads.
//
// PERKS: SKYBPersuasionPerks.ini (PersuasionPerks) changes a copy of these values per
// conversation; see PersuasionRules::Resolve. Nothing here knows about the player.
#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

#include "PersuasionSettingsTable.h"

namespace Persuasion
{
	class Settings
	{
	public:
		static constexpr std::size_t kCount = static_cast<std::size_t>(P::kCount);
		using Values = std::array<float, kCount>;

		// kDataLoaded: read the INI (and the perks file through PersuasionPerks).
		static void Resolve();
		// Re-read the INI if it changed on disk since the last read (the perks file too).
		static void ReloadIfChanged();

		// The settings menu's store: set or clear one override.
		static bool SetOverride(std::string_view a_section, std::string_view a_key, float a_value);
		static bool ClearOverride(std::string_view a_section, std::string_view a_key);

		[[nodiscard]] static float Get(P a_id) { return s_values[static_cast<std::size_t>(a_id)]; }
		[[nodiscard]] static bool  On(P a_id) { return Get(a_id) >= 0.5f; }
		// Every value, INI with the menu's overrides laid over: the starting point of a conversation's rules.
		[[nodiscard]] static const Values& All() { return s_values; }

		// ("Decay", "WhileOpen") -> the index for At() / the enum, or -1 when the table has no such row.
		[[nodiscard]] static int   Find(std::string_view a_section, std::string_view a_key);
		[[nodiscard]] static float At(int a_index) { return a_index >= 0 ? s_values[static_cast<std::size_t>(a_index)] : 0.0f; }
		[[nodiscard]] static const SettingDef& Def(std::size_t a_index) { return kSettingDefs[a_index]; }

		static constexpr std::string_view kIniPath = "Data/SKSE/Plugins/SKYBPersuasion.ini";

	private:
		static void Load();
		static void Recompose();  // s_values = INI values with the overrides laid over them

		static inline Values                                      s_values{};
		static inline Values                                      s_ini{};
		static inline std::array<std::optional<float>, kCount>    s_overrides{};
		static inline std::filesystem::file_time_type             s_iniTime{};
		static inline bool                                        s_iniSeen = false;
	};
}
