#include "SettingsService.h"
#include "SettingsContract.h"
#include "Storage/SafeFileWrite.h"

#include <cctype>
#include <fstream>

namespace Settings {

	namespace {
		std::string Trim(std::string_view text) {
			const auto begin = text.find_first_not_of(" \t\r");
			if (begin == std::string_view::npos) {
				return {};
			}
			const auto end = text.find_last_not_of(" \t\r");
			return std::string(text.substr(begin, end - begin + 1));
		}

		bool EqualsIgnoreCase(std::string_view left, std::string_view right) {
			if (left.size() != right.size()) {
				return false;
			}
			for (std::size_t i = 0; i < left.size(); ++i) {
				if (std::tolower(static_cast<unsigned char>(left[i])) != std::tolower(static_cast<unsigned char>(right[i]))) {
					return false;
				}
			}
			return true;
		}
	}

	std::filesystem::path SettingsService::Path() {
		auto folder = SKSE::log::log_directory();
		return folder ? *folder / SettingsContract::FileName : std::filesystem::path();
	}

	void SettingsService::Register() {
		Load();
		Log::INFO("SettingsService: Registered ({} section(s) in {}).", s_order.size(), SettingsContract::FileName);
	}

	void SettingsService::Load() {
		std::scoped_lock lock(s_lock);
		s_order.clear();
		s_sectionEntries.clear();
		const auto path = Path();
		if (path.empty()) {
			return;
		}
		std::ifstream inputFile(path);
		std::string line;
		std::string section;
		while (std::getline(inputFile, line)) {
			const auto trimmedLine = Trim(line);
			if (trimmedLine.empty() || trimmedLine[0] == '#' || trimmedLine[0] == ';') {
				continue;
			}
			if (trimmedLine.front() == '[' && trimmedLine.back() == ']') {
				section = Trim(std::string_view(trimmedLine).substr(1, trimmedLine.size() - 2));
				if (!s_sectionEntries.contains(section)) {
					s_order.push_back(section);
					s_sectionEntries[section];
				}
				continue;
			}
			const auto equalsIndex = trimmedLine.find('=');
			if (equalsIndex == std::string::npos || section.empty()) {
				continue;
			}
			s_sectionEntries[section].emplace_back(Trim(std::string_view(trimmedLine).substr(0, equalsIndex)), Trim(std::string_view(trimmedLine).substr(equalsIndex + 1)));
		}
	}

	void SettingsService::Save() {
		const auto path = Path();
		if (path.empty()) {
			return;
		}
		std::string fileContents = "# Skyblivion settings - written by Skyblivion.dll. Edit while the game is closed.\n";
		for (const auto& section : s_order) {
			fileContents += '[' + section + "]\n";
			for (const auto& [key, value] : s_sectionEntries.at(section)) {
				fileContents += key + '=' + value + '\n';
			}
		}
		SafeFileWrite::Write(path, fileContents, "SettingsService");
	}

	std::string SettingsService::GetString(std::string_view section, std::string_view key, std::string_view defaultValue) {
		std::scoped_lock lock(s_lock);
		for (const auto& [name, rows] : s_sectionEntries) {
			if (!EqualsIgnoreCase(name, section)) {
				continue;
			}
			for (const auto& [rowKey, value] : rows) {
				if (EqualsIgnoreCase(rowKey, key)) {
					return value;
				}
			}
		}
		return std::string(defaultValue);
	}

	int SettingsService::GetInt(std::string_view section, std::string_view key, int defaultValue) {
		const auto valueText = GetString(section, key, "");
		if (valueText.empty()) {
			return defaultValue;
		}
		try {
			return std::stoi(valueText);
		}
		catch (...) {
			return defaultValue;
		}
	}

	void SettingsService::SetString(std::string_view section, std::string_view key, std::string_view value) {
		{
			std::scoped_lock lock(s_lock);
			const std::string* sectionName = nullptr;
			for (const auto& name : s_order) {
				if (EqualsIgnoreCase(name, section)) {
					sectionName = &name;
				}
			}
			if (!sectionName) {
				s_order.emplace_back(section);
				sectionName = &s_order.back();
				s_sectionEntries[*sectionName];
			}
			auto& rows = s_sectionEntries[*sectionName];
			bool replaced = false;
			for (auto& [rowKey, rowValue] : rows) {
				if (EqualsIgnoreCase(rowKey, key)) {
					rowValue = std::string(value);
					replaced = true;
				}
			}
			if (!replaced) {
				rows.emplace_back(std::string(key), std::string(value));
			}
			Save();
		}
		Log::INFO("SettingsService: [{}] {}={}", section, key, value);
	}

	bool SettingsService::RemoveKey(std::string_view section, std::string_view key) {
		bool removed = false;
		{
			std::scoped_lock lock(s_lock);
			for (auto& [name, rows] : s_sectionEntries) {
				if (!EqualsIgnoreCase(name, section)) {
					continue;
				}
				if (std::erase_if(rows, [&](const auto& row) { return EqualsIgnoreCase(row.first, key); }) > 0) {
					removed = true;
				}
			}
			if (removed) {
				Save();
			}
		}
		if (removed) {
			Log::INFO("SettingsService: [{}] {} removed", section, key);
		}
		return removed;
	}

	void SettingsService::SetInt(std::string_view section, std::string_view key, int value) {
		SetString(section, key, std::to_string(value));
	}

}
