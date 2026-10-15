#include "AchievementData.h"
#include "AchievementContract.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <unordered_map>

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

		std::string Lower(std::string text) {
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		std::optional<json> ReadJson(const fs::path& path) {
			std::ifstream in(path);
			if (!in) {
				Log::WARN("AchievementData: {} cannot be opened - skipped", path.generic_string());
				return std::nullopt;
			}
			try {
				auto parsed = json::parse(in, nullptr, true, true);
				if (!parsed.is_object()) {
					Log::WARN("AchievementData: {} is not a JSON object - skipped", path.generic_string());
					return std::nullopt;
				}
				return parsed;
			}
			catch (const std::exception& e) {
				Log::WARN("AchievementData: {} does not parse ({}) - skipped", path.generic_string(), e.what());
				return std::nullopt;
			}
		}

		//A string field: fallback when absent; nullopt when present but not a string.
		std::optional<std::string> Text(const json& object, const char* key, const std::string& fallback) {
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
			auto parsed = ReadJson(path);
			if (!parsed) {
				return std::nullopt;
			}
			const auto id = Text(*parsed, "id", "");
			if (!id || !std::regex_match(*id, kIdRe)) {
				Log::WARN("AchievementData: {} has no valid \"id\" (a letter, then letters, digits or _) - category skipped", path.generic_string());
				return std::nullopt;
			}
			Category category;
			category.id = *id;
			category.displayName = Text(*parsed, "displayName", *id).value_or(*id);
			category.priority = Number(*parsed, "priority", 100.0);
			return category;
		}

		std::optional<Achievement> ParseAchievement(const fs::path& path, const std::string& categoryId) {
			auto parsed = ReadJson(path);
			if (!parsed) {
				return std::nullopt;
			}
			const auto& j = *parsed;
			const auto id = Text(j, "id", "");
			if (!id || !std::regex_match(*id, kIdRe)) {
				Log::WARN("AchievementData: {} has no valid \"id\" - skipped", path.generic_string());
				return std::nullopt;
			}
			Achievement a;
			a.id = *id;
			a.categoryId = categoryId;
			a.priority = Number(j, "priority", 100.0);
			auto hidden = j.find("hidden");
			a.hidden = hidden != j.end() && hidden->is_boolean() && hidden->get<bool>();
			const auto name = Text(j, "name", *id);
			const auto description = Text(j, "description", "");
			const auto icon = Text(j, "icon", "");
			if (!name || !description || !icon) {
				Log::WARN("AchievementData: {}: \"name\", \"description\" and \"icon\" must be strings - skipped", path.generic_string());
				return std::nullopt;
			}
			a.name = *name;
			a.description = *description;
			a.icon = *icon;
			const auto lockedName = Text(j, "lockedName", a.hidden ? kHiddenText : a.name);
			const auto lockedDescription = Text(j, "lockedDescription", a.hidden ? kHiddenText : a.description);
			if (!lockedName || !lockedDescription) {
				Log::WARN("AchievementData: {}: \"lockedName\" and \"lockedDescription\" must be strings - skipped", path.generic_string());
				return std::nullopt;
			}
			a.lockedName = *lockedName;
			a.lockedDescription = *lockedDescription;

			auto progress = j.find("progress");
			if (progress != j.end() && !progress->is_null()) {
				const auto countGlobal = progress->is_object() ? Text(*progress, "countGlobal", "") : std::nullopt;
				const auto totalGlobal = progress->is_object() ? Text(*progress, "totalGlobal", "") : std::nullopt;
				const double total = progress->is_object() ? Number(*progress, "total", 0.0) : 0.0;
				if (!countGlobal || countGlobal->empty() || !totalGlobal || (totalGlobal->empty() && total <= 0.0)) {
					Log::WARN("AchievementData: {}: \"progress\" needs \"countGlobal\" and a positive \"total\" or a \"totalGlobal\" - shown without progress",
						path.generic_string());
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
				return Lower(a.id) < Lower(b.id);
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
			Log::ERROR("AchievementData: {} not found - no achievements", root.generic_string());
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
				Log::INFO("AchievementData: folder {} has no {} - skipped", entry.path().generic_string(), AchievementContract::CategoryFileName);
				continue;
			}
			if (auto category = ParseCategory(file)) {
				folders.push_back({ std::move(*category), entry.path(), Lower(entry.path().filename().string()) });
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
			const auto key = Lower(folder.category.id);
			if (auto [it, inserted] = categoryOwner.try_emplace(key, folder.path.generic_string()); !inserted) {
				Log::WARN("AchievementData: duplicate category id '{}': {} skipped ({} wins)", folder.category.id, folder.path.generic_string(), it->second);
				continue;
			}

			std::vector<fs::path> files;
			for (const auto& entry : fs::directory_iterator(folder.path, ec)) {
				if (entry.is_regular_file(ec) && Lower(entry.path().extension().string()) == ".json" &&
					Lower(entry.path().filename().string()) != AchievementContract::CategoryFileName) {
					files.push_back(entry.path());
				}
			}
			std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
				return Lower(a.filename().string()) < Lower(b.filename().string());
			});

			Category category = std::move(folder.category);
			std::unordered_map<std::string, bool> seen;
			for (const auto& file : files) {
				auto achievement = ParseAchievement(file, category.id);
				if (!achievement) {
					continue;
				}
				const auto idKey = Lower(achievement->id);
				bool taken = seen.contains(idKey) || s_index.contains(idKey);
				if (taken) {
					Log::WARN("AchievementData: duplicate achievement id '{}' in {} - skipped (the first definition wins)", achievement->id, file.generic_string());
					continue;
				}
				seen.emplace(idKey, true);
				category.entries.push_back(std::move(*achievement));
			}
			if (category.entries.empty()) {
				Log::WARN("AchievementData: category '{}' ({}) has no achievements - not listed", category.id, folder.path.generic_string());
				continue;
			}
			SortByPriority(category.entries);
			const auto categoryIndex = s_categories.size();
			for (std::size_t i = 0; i < category.entries.size(); ++i) {
				s_index.emplace(Lower(category.entries[i].id), std::make_pair(categoryIndex, i));
			}
			s_count += category.entries.size();
			s_categories.push_back(std::move(category));
		}
		Log::INFO("AchievementData: {} achievement(s) in {} categor{} from {}", s_count, s_categories.size(),
			s_categories.size() == 1 ? "y" : "ies", root.generic_string());
	}

	const std::vector<Category>& AchievementData::Categories() {
		return s_categories;
	}

	const Achievement* AchievementData::Find(std::string_view id) {
		auto it = s_index.find(Lower(std::string(id)));
		if (it == s_index.end()) {
			return nullptr;
		}
		return &s_categories[it->second.first].entries[it->second.second];
	}

	std::size_t AchievementData::Count() {
		return s_count;
	}

}
