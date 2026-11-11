#include "AchievementData.h"
#include "AchievementContract.h"
#include "Platform/JsonFile.h"
#include "Platform/Text.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <regex>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using nlohmann::json;

namespace Achievements {

	namespace {
		const std::regex kIdRe{ R"(^[A-Za-z][A-Za-z0-9_]*$)" };
		constexpr const char* kHiddenText = "???";

		bool s_loaded = false;
		std::vector<Category> s_categories;
		//Lowercased id -> (category index, entry index).
		std::unordered_map<std::string, std::pair<std::size_t, std::size_t>> s_index;
		std::size_t s_count = 0;

		//A string field: fallback when absent; nullopt when present but not a string.
		std::optional<std::string> StringField(const json& object, const char* key, const std::string& fallback) {
			auto it = object.find(key);
			if (it == object.end() || it->is_null()) {
				return fallback;
			}
			if (!it->is_string()) {
				return std::nullopt;
			}
			return it->get<std::string>();
		}

		double Number(const json& object, const char* key, double fallback) {
			auto it = object.find(key);
			return it != object.end() && it->is_number() ? it->get<double>() : fallback;
		}

		std::optional<Category> ParseCategory(const fs::path& path) {
			auto parsed = JsonFile::ReadObject(path, "AchievementData");
			if (!parsed) {
				return std::nullopt;
			}
			const auto id = StringField(*parsed, "id", "");
			if (!id || !std::regex_match(*id, kIdRe)) {
				Log::WARN("AchievementData: {} has no valid \"id\" (a letter, then letters, digits or _) - category skipped", Text::Utf8(path));
				return std::nullopt;
			}
			Category category;
			category.id = *id;
			category.displayName = StringField(*parsed, "displayName", *id).value_or(*id);
			category.priority = Number(*parsed, "priority", 100.0);
			return category;
		}

		std::optional<Achievement> ParseAchievement(const fs::path& path, const std::string& categoryId) {
			auto parsed = JsonFile::ReadObject(path, "AchievementData");
			if (!parsed) {
				return std::nullopt;
			}
			const auto& j = *parsed;
			const auto id = StringField(j, "id", "");
			if (!id || !std::regex_match(*id, kIdRe)) {
				Log::WARN("AchievementData: {} has no valid \"id\" - skipped", Text::Utf8(path));
				return std::nullopt;
			}
			Achievement a;
			a.id = *id;
			a.categoryId = categoryId;
			a.priority = Number(j, "priority", 100.0);
			auto hidden = j.find("hidden");
			a.hidden = hidden != j.end() && hidden->is_boolean() && hidden->get<bool>();
			const auto name = StringField(j, "name", *id);
			const auto description = StringField(j, "description", "");
			const auto icon = StringField(j, "icon", "");
			if (!name || !description || !icon) {
				Log::WARN("AchievementData: {}: \"name\", \"description\" and \"icon\" must be strings - skipped", Text::Utf8(path));
				return std::nullopt;
			}
			a.name = *name;
			a.description = *description;
			a.icon = *icon;
			const auto lockedName = StringField(j, "lockedName", a.hidden ? kHiddenText : a.name);
			const auto lockedDescription = StringField(j, "lockedDescription", a.hidden ? kHiddenText : a.description);
			if (!lockedName || !lockedDescription) {
				Log::WARN("AchievementData: {}: \"lockedName\" and \"lockedDescription\" must be strings - skipped", Text::Utf8(path));
				return std::nullopt;
			}
			a.lockedName = *lockedName;
			a.lockedDescription = *lockedDescription;

			auto progress = j.find("progress");
			if (progress != j.end() && !progress->is_null()) {
				const auto countGlobal = progress->is_object() ? StringField(*progress, "countGlobal", "") : std::nullopt;
				const auto totalGlobal = progress->is_object() ? StringField(*progress, "totalGlobal", "") : std::nullopt;
				const double total = progress->is_object() ? Number(*progress, "total", 0.0) : 0.0;
				if (!countGlobal || countGlobal->empty() || !totalGlobal || (totalGlobal->empty() && total <= 0.0)) {
					Log::WARN("AchievementData: {}: \"progress\" needs \"countGlobal\" and a positive \"total\" or a \"totalGlobal\" - shown without progress",
						Text::Utf8(path));
				}
				else {
					a.progress.present = true;
					a.progress.countGlobal = *countGlobal;
					a.progress.totalGlobal = *totalGlobal;
					a.progress.total = total;
				}
			}
			return a;
		}

		template <class T>
		void SortByPriority(std::vector<T>& items) {
			std::stable_sort(items.begin(), items.end(), [](const T& a, const T& b) {
				if (a.priority != b.priority) {
					return a.priority < b.priority;
				}
				return Text::Lower(a.id) < Text::Lower(b.id);
			});
		}
	}

	void AchievementData::LoadOnce() {
		if (s_loaded) {
			return;
		}
		s_loaded = true;

		const fs::path root{ AchievementContract::DataRoot };
		std::error_code ec;
		if (!fs::is_directory(root, ec)) {
			Log::ERROR("AchievementData: {} not found - no achievements", Text::Utf8(root));
			return;
		}

		struct Folder {
			Category category;
			fs::path path;
			std::string nameLower;
		};
		std::vector<Folder> folders;
		for (const auto& entry : fs::directory_iterator(root, ec)) {
			if (!entry.is_directory(ec)) {
				continue;
			}
			const auto file = entry.path() / AchievementContract::CategoryFileName;
			if (!fs::is_regular_file(file, ec)) {
				Log::INFO("AchievementData: folder {} has no {} - skipped", Text::Utf8(entry.path()), AchievementContract::CategoryFileName);
				continue;
			}
			if (auto category = ParseCategory(file)) {
				folders.push_back({ std::move(*category), entry.path(), Text::Lower(Text::Utf8(entry.path().filename())) });
			}
		}
		std::stable_sort(folders.begin(), folders.end(), [](const Folder& a, const Folder& b) {
			if (a.category.priority != b.category.priority) {
				return a.category.priority < b.category.priority;
			}
			return a.nameLower < b.nameLower;
		});

		std::unordered_map<std::string, std::string> categoryOwner;  //lowercased category id -> folder
		for (auto& folder : folders) {
			const auto key = Text::Lower(folder.category.id);
			if (auto [it, inserted] = categoryOwner.try_emplace(key, Text::Utf8(folder.path)); !inserted) {
				Log::WARN("AchievementData: duplicate category id '{}': {} skipped ({} wins)", folder.category.id, Text::Utf8(folder.path), it->second);
				continue;
			}

			std::vector<fs::path> files;
			for (const auto& entry : fs::directory_iterator(folder.path, ec)) {
				if (entry.is_regular_file(ec) && Text::Lower(Text::Utf8(entry.path().extension())) == ".json" &&
					Text::Lower(Text::Utf8(entry.path().filename())) != AchievementContract::CategoryFileName) {
					files.push_back(entry.path());
				}
			}
			std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
				return Text::Lower(Text::Utf8(a.filename())) < Text::Lower(Text::Utf8(b.filename()));
			});

			Category category = std::move(folder.category);
			std::unordered_set<std::string> seen;
			for (const auto& file : files) {
				auto achievement = ParseAchievement(file, category.id);
				if (!achievement) {
					continue;
				}
				const auto idKey = Text::Lower(achievement->id);
				bool taken = seen.contains(idKey) || s_index.contains(idKey);
				if (taken) {
					Log::WARN("AchievementData: duplicate achievement id '{}' in {} - skipped (the first definition wins)", achievement->id, Text::Utf8(file));
					continue;
				}
				seen.insert(idKey);
				category.entries.push_back(std::move(*achievement));
			}
			if (category.entries.empty()) {
				Log::WARN("AchievementData: category '{}' ({}) has no achievements - not listed", category.id, Text::Utf8(folder.path));
				continue;
			}
			SortByPriority(category.entries);
			const auto categoryIndex = s_categories.size();
			for (std::size_t i = 0; i < category.entries.size(); ++i) {
				s_index.emplace(Text::Lower(category.entries[i].id), std::make_pair(categoryIndex, i));
			}
			s_count += category.entries.size();
			s_categories.push_back(std::move(category));
		}
		Log::INFO("AchievementData: {} achievement(s) in {} categor{} from {}", s_count, s_categories.size(),
			s_categories.size() == 1 ? "y" : "ies", Text::Utf8(root));
	}

	const std::vector<Category>& AchievementData::Categories() {
		return s_categories;
	}

	const Achievement* AchievementData::Find(std::string_view id) {
		auto it = s_index.find(Text::Lower(std::string(id)));
		if (it == s_index.end()) {
			return nullptr;
		}
		return &s_categories[it->second.first].entries[it->second.second];
	}

}
