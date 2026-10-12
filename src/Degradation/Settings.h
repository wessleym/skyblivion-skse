#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Degradation/SettingsTable.h"

// Every tunable of the degradation system lives in Data\SKSE\Plugins\SKYBDegradation.ini, one key
// per row of Settings.csv (which also gives the built-in defaults). The file is read at kDataLoaded
// and re-read when its write time changes (checked on every save load and menu open). Values are not
// stored in the save.
//
// Overrides: the settings menu owns the player-facing values (wear rate, stat floor, prices...) per
// character and calls SetOverride for each on every game load and change. An override replaces the
// INI value until it is cleared and survives INI reloads.
namespace SKYBDeg
{
	class Settings
	{
	public:
		// kDataLoaded: read the INI + resolve the tool list / hammer table.
		static void Resolve();
		// Re-read the INI if it changed on disk since the last read.
		static void ReloadIfChanged();

		// Set or clear one override (called by the settings menu's store).
		static bool SetOverride(std::string_view a_section, std::string_view a_key, float a_value);
		static bool ClearOverride(std::string_view a_section, std::string_view a_key);

		[[nodiscard]] static float Get(G a_id) { return s_values[static_cast<std::size_t>(a_id)]; }
		[[nodiscard]] static bool  On(G a_id) { return Get(a_id) >= 0.5f; }

		// Name lookup for the per-material rows: ("WeaponMaterialWear", "Steel").
		// Returns the row index for At(), or -1 when the table has no such row.
		[[nodiscard]] static int   Find(std::string_view a_section, std::string_view a_key);
		[[nodiscard]] static float At(int a_index) { return a_index >= 0 ? s_values[static_cast<std::size_t>(a_index)] : 0.0f; }

		// FLST 000800 SKYBDegRepairTools (items that open the repair menu).
		[[nodiscard]] static RE::BGSListForm* RepairTools() { return s_tools; }

		// The hammer tiers: tool i of SKYBDegRepairTools uses [Hammers]
		// <kHammerKeys[i]>Break / <kHammerKeys[i]>Tier.
		struct Hammer
		{
			RE::TESBoundObject* form{ nullptr };
			const char*         key{ nullptr };
			int                 breakChance{ -1 };  // row index; value 0 = never breaks
			int                 tier{ -1 };
		};
		static constexpr std::array<const char*, 5> kHammerKeys{ "Common", "Steel", "Dwarven", "Ebony", "Forgemaster" };
		[[nodiscard]] static const std::vector<Hammer>& Hammers() { return s_hammers; }
		[[nodiscard]] static bool             PluginLoaded() { return s_pluginLoaded; }

		//the tool list by editor ID: SKYBDegradation.esp, SKYBCombined.esp or the ESM
		static constexpr const char* kToolsList = "SKYBDegRepairTools";
		static constexpr std::string_view kIniPath = "Data/SKSE/Plugins/SKYBDegradation.ini";

	private:
		static void Load();
		static void Recompose();  // s_values = INI values with the overrides laid over them

		static inline std::array<float, static_cast<std::size_t>(G::kCount)> s_values{};
		static inline std::array<float, static_cast<std::size_t>(G::kCount)> s_ini{};
		static inline std::array<std::optional<float>, static_cast<std::size_t>(G::kCount)> s_overrides{};
		static inline std::filesystem::file_time_type                     s_iniTime{};
		static inline bool                                                s_iniSeen = false;
		static inline RE::BGSListForm*                                    s_tools = nullptr;
		static inline std::vector<Hammer>                                 s_hammers;
		static inline bool                                                s_pluginLoaded = false;
	};
}
