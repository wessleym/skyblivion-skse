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
	//Nothing else reads the file: StartMenuService and AchievementJournal go through this service.
	//
	//The toast setting is also copied into the SKYBAchieveNotifications global on every game load,
	//and the journal's toggle row sets it, so the file, the global and the journal agree.
	//
	//Callers run on the main thread and on mod-event sender threads, so access is locked.
	class SettingsService {
	public:
		//Loads the file and takes the mod-event sink. Call once, at kDataLoaded.
		static void Register();

		//Copies file values into game state. Call once a save is in (kPostLoadGame, or a frame after kNewGame).
		static void ApplyToGame();

		[[nodiscard]] static int GetInt(std::string_view section, std::string_view key, int defaultValue);
		static void SetInt(std::string_view section, std::string_view key, int value);
		[[nodiscard]] static std::string GetString(std::string_view section, std::string_view key, std::string_view defaultValue);
		static void SetString(std::string_view section, std::string_view key, std::string_view value);

	private:
		[[nodiscard]] static std::filesystem::path Path();
		static void Load();
		//Caller holds s_lock.
		static void Save();

		class Sink : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
				RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;
		};
		static inline Sink s_sink;

		static inline std::mutex s_lock;
		//Section names in file order, for the writer.
		static inline std::vector<std::string> s_order;
		static inline std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> s_sectionEntries;
	};

}
