#include "AchievementStore.h"
#include "AchievementContract.h"
#include "Papyrus/ModEvents.h"
#include "Storage/SafeFileWrite.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>

namespace Achievements {

	namespace {
		nlohmann::json g_store;
		bool g_loaded = false;
		//False when the file on disk could be neither read nor moved aside: a save would destroy it.
		bool g_canSave = true;

		std::optional<std::filesystem::path> StorePath() {
			auto folder = SKSE::log::log_directory();
			if (!folder) {
				return std::nullopt;
			}
			return *folder / AchievementContract::FileName;
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

	void AchievementStore::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("AchievementStore: No mod-event source. Achievement syncs will not be heard.");
			return;
		}
		Log::INFO("AchievementStore: Registered.");
	}

	void AchievementStore::Sync(const std::string& csv) {
		EnsureLoaded();
		auto& achievements = g_store["achievements"];

		std::string characterName;
		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			if (const char* name = player->GetName()) {
				characterName = name;
			}
		}

		int added = 0;
		for (const auto& row : ModEvents::Split(csv, ';')) {
			const auto separatorIndex = row.find('|');
			if (separatorIndex == std::string::npos || separatorIndex == 0) {
				continue;
			}
			//Only rows flagged "1" are unlocked.
			if (std::string_view(row).substr(separatorIndex + 1, 1) != "1") {
				continue;
			}
			const std::string key = row.substr(0, separatorIndex);
			if (!achievements.contains(key)) {
				achievements[key] = { { "first", NowStamp() }, { "character", characterName } };
				++added;
				Log::INFO("AchievementStore: New cross-save unlock '{}' (character '{}').", key, characterName);
			}
		}

		if (added > 0) {
			SaveStore();
		}
	}

	std::unordered_set<std::string> AchievementStore::UnlockedKeys() {
		EnsureLoaded();
		std::unordered_set<std::string> keys;
		for (const auto& [key, value] : g_store["achievements"].items()) {
			keys.insert(key);
		}
		return keys;
	}

	void AchievementStore::AnswerFileRequest() {
		EnsureLoaded();
		std::string keys;
		for (const auto& [key, value] : g_store["achievements"].items()) {
			if (!keys.empty()) {
				keys += ',';
			}
			keys += key;
		}
		const auto count = g_store["achievements"].size();
		ModEvents::Send(AchievementContract::Answer::FileData, keys, static_cast<float>(count));
		Log::INFO("AchievementStore: Answered a file data request ({} key(s)).", count);
	}

	RE::BSEventNotifyControl AchievementStore::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		if (a_event->eventName == AchievementContract::ModEvent::Sync) {
			SKSE::GetTaskInterface()->AddTask([csv = ModEvents::StrArg(*a_event)] { Sync(csv); });
		}
		else if (a_event->eventName == AchievementContract::ModEvent::RequestFileData) {
			SKSE::GetTaskInterface()->AddTask([] { AnswerFileRequest(); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
