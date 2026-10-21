#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Settings {

	//The Skyblivion settings file (see SettingsContract.h for its keys).
	//
	//A plain INI: sections, key=value, # or ; comments. Read once at kDataLoaded, held in memory,
	//and written through on every Set. Sections and keys compare case-insensitively, and unknown ones are kept.
	//A missing file or key reads as its default; the file is created on the first Set.
	//Nothing else reads the file: StartMenuService, AchievementService, the settings menu's store and the
	//hotkey service go through this service.
	//
	//Callers run on the main thread and on mod-event sender threads, so access is locked.
	class SettingsService {
	public:
		//Loads the file. Call once, at kDataLoaded.
		static void Register();

		[[nodiscard]] static int GetInt(std::string_view section, std::string_view key, int defaultValue);
		static void SetInt(std::string_view section, std::string_view key, int value);
		[[nodiscard]] static std::string GetString(std::string_view section, std::string_view key, std::string_view defaultValue);
		static void SetString(std::string_view section, std::string_view key, std::string_view value);
		//Deletes the key and writes the file. True if it was there.
		static bool RemoveKey(std::string_view section, std::string_view key);

	private:
		[[nodiscard]] static std::filesystem::path Path();
		static void Load();
		//Caller holds s_lock.
		static void Save();

		static inline std::mutex s_lock;
		//Section names in file order, for the writer.
		static inline std::vector<std::string> s_order;
		static inline std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> s_sectionEntries;
	};

}
