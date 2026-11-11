#include "SettingsService.h"
#include "SettingsContract.h"
#include "Storage/SafeFileWrite.h"
#include "Platform/IniFile.h"
#include "Platform/Text.h"

namespace Settings {

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
		IniFile::Read(path, "SettingsService", [](const IniFile::Entry& a_entry) {
			if (a_entry.section.empty()) {
				return;
			}
			if (!s_sectionEntries.contains(a_entry.section)) {
				s_order.push_back(a_entry.section);
			}
			s_sectionEntries[a_entry.section].emplace_back(a_entry.key, a_entry.value);
		});
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
			if (!Text::IEquals(name, section)) {
				continue;
			}
			for (const auto& [rowKey, value] : rows) {
				if (Text::IEquals(rowKey, key)) {
					return value;
				}
			}
		}
		return std::string(defaultValue);
	}

	int SettingsService::GetInt(std::string_view section, std::string_view key, int defaultValue) {
		int value = 0;
		return Text::Parse(GetString(section, key, ""), value) ? value : defaultValue;
	}

	void SettingsService::SetString(std::string_view section, std::string_view key, std::string_view value) {
		{
			std::scoped_lock lock(s_lock);
			const std::string* sectionName = nullptr;
			for (const auto& name : s_order) {
				if (Text::IEquals(name, section)) {
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
				if (Text::IEquals(rowKey, key)) {
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
				if (!Text::IEquals(name, section)) {
					continue;
				}
				if (std::erase_if(rows, [&](const auto& row) { return Text::IEquals(row.first, key); }) > 0) {
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
