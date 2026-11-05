#include "AchievementStore.h"
#include "AchievementContract.h"
#include "Storage/SafeFileWrite.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string_view>

namespace Achievements {

	namespace {
		//Guards everything below: unlocks arrive from the Papyrus VM, the menu reads from UI tasks.
		std::mutex g_lock;
		nlohmann::json g_store;
		bool g_loaded = false;
		//False when the file on disk could be neither read nor moved aside: a save would destroy it.
		bool g_canSave = true;

		std::optional<std::filesystem::path> StorePath() {
			auto folder = SKSE::log::log_directory();
			if (!folder) {
				return std::nullopt;
			}
			return *folder / AchievementContract::RecordFileName;
		}

		//Null when the file cannot be opened, does not parse, or lacks the achievements object.
		std::optional<nlohmann::json> ReadStoreFile(const std::filesystem::path& path, std::string& problem) {
			std::ifstream inputFile(path);
			if (!inputFile) {
				problem = "cannot be opened";
				return std::nullopt;
			}
			try {
				auto parsed = nlohmann::json::parse(inputFile, nullptr, true, true);
				if (!parsed.contains("achievements") || !parsed["achievements"].is_object()) {
					problem = "has no achievements object";
					return std::nullopt;
				}
				return parsed;
			}
			catch (const std::exception& e) {
				problem = e.what();
				return std::nullopt;
			}
		}

		//Local time for a file name, or UTC when the time zone database is unavailable.
		std::string FileNameStamp() {
			const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
			try {
				return std::format("{:%Y%m%d-%H%M%S}", std::chrono::zoned_time(std::chrono::current_zone(), now));
			}
			catch (const std::exception&) {
				return std::format("{:%Y%m%d-%H%M%S}Z", now);
			}
		}

		//Renames an unreadable file to SKYBAchievements.unreadable-<time>.json so no save can overwrite it.
		//False when it could not be moved, including when that name is already taken.
		bool SetAside(const std::filesystem::path& path) {
			const auto backup = path.parent_path() /
				std::format("{}.unreadable-{}{}", path.stem().string(), FileNameStamp(), path.extension().string());
			std::error_code error;
			if (std::filesystem::exists(backup, error) || error) {
				Log::ERROR("AchievementStore: Cannot set the file aside: {} already exists or cannot be checked.", backup.filename().string());
				return false;
			}
			std::filesystem::rename(path, backup, error);
			if (error) {
				Log::ERROR("AchievementStore: Cannot set the file aside as {} ({}).", backup.filename().string(), error.message());
				return false;
			}
			Log::WARN("AchievementStore: Set the unreadable file aside as {}. Starting a new file; merge the old one back by hand.", backup.filename().string());
			return true;
		}

		//Never leaves a save able to overwrite a file it has not read:
		//an unreadable file is set aside, and when that fails, saving is off for the session.
		void EnsureLoaded() {
			if (g_loaded) {
				return;
			}
			g_loaded = true;
			g_store = { { "version", 1 }, { "achievements", nlohmann::json::object() } };
			const auto path = StorePath();
			if (!path) {
				return;
			}
			std::error_code error;
			const bool exists = std::filesystem::exists(*path, error);
			if (error) {
				g_canSave = false;
				Log::ERROR("AchievementStore: Cannot check {} ({}). Saving is off this session so the file is not overwritten.",
					path->filename().string(), error.message());
				return;
			}
			if (!exists) {
				return;
			}
			std::string problem;
			if (auto parsed = ReadStoreFile(*path, problem)) {
				g_store = std::move(*parsed);
				return;
			}
			Log::WARN("AchievementStore: {} is unreadable: {}.", path->filename().string(), problem);
			if (!SetAside(*path)) {
				g_canSave = false;
				Log::ERROR("AchievementStore: Saving is off this session so the file is not overwritten.");
			}
		}

		//Writes a temporary file and renames it over the store, so a failed or interrupted write leaves the old file intact.
		void SaveStore() {
			const auto count = g_store["achievements"].size();
			if (!g_canSave) {
				Log::WARN("AchievementStore: Saving is off this session (see the earlier error). {} achievement(s) kept in memory only.", count);
				return;
			}
			const auto path = StorePath();
			if (!path) {
				Log::WARN("AchievementStore: No SKSE log folder. Cannot save.");
				return;
			}
			try {
				//Replaces bytes that are not UTF-8, such as a character name in a Windows code page, instead of throwing.
				const auto serializedStore = g_store.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
				if (SafeFileWrite::Write(*path, serializedStore, "AchievementStore")) {
					Log::INFO("AchievementStore: Saved {} achievement(s).", count);
				}
			}
			catch (const std::exception& e) {
				Log::WARN("AchievementStore: Save failed ({}).", e.what());
			}
		}

		std::string NowStamp() {
			const auto now = std::chrono::system_clock::now();
			try {
				return std::format("{:%Y-%m-%d %H:%M}", std::chrono::zoned_time(std::chrono::current_zone(), now));
			}
			catch (const std::exception&) {
				return std::format("{:%Y-%m-%d %H:%M} UTC", std::chrono::floor<std::chrono::minutes>(now));
			}
		}
	}

	void AchievementStore::Record(const std::string& id, const std::string& character) {
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		auto& achievements = g_store["achievements"];
		if (achievements.contains(id)) {
			return;
		}
		achievements[id] = { { "first", NowStamp() }, { "character", character } };
		Log::INFO("AchievementStore: New cross-save unlock '{}' (character '{}').", id, character);
		SaveStore();
	}

	std::unordered_map<std::string, AchievementStore::Entry> AchievementStore::All() {
		std::scoped_lock lock(g_lock);
		EnsureLoaded();
		std::unordered_map<std::string, Entry> entries;
		for (const auto& [key, value] : g_store["achievements"].items()) {
			Entry entry;
			if (value.is_object()) {
				// a hand-edited file may hold any type here: anything but a string reads as blank
				const auto text = [&value](const char* a_key) {
					const auto it = value.find(a_key);
					return it != value.end() && it->is_string() ? it->get<std::string>() : std::string{};
				};
				entry.first = text("first");
				entry.character = text("character");
			}
			entries.emplace(key, std::move(entry));
		}
		return entries;
	}

	std::string AchievementStore::Now() {
		return NowStamp();
	}

}
