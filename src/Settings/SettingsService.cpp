#include "SettingsService.h"
#include "SettingsContract.h"
#include "Papyrus/ModEvents.h"
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
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("SettingsService: No mod-event source. The journal's toast toggle will not reach the file.");
		}
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

	void SettingsService::SetInt(std::string_view section, std::string_view key, int value) {
		SetString(section, key, std::to_string(value));
	}

	//The Papyrus toast gate (SKYBAchievementFeed.ShowAchievementToast) reads this global,
	//and the journal's toggle row shows it.
	void SettingsService::ApplyToGame() {
		const int toasts = GetInt(SettingsContract::AchievementsSection, SettingsContract::ToastKey, 1);
		auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(SettingsContract::ToastGlobalEditorID);
		if (!global) {
			Log::WARN("SettingsService: {} not found. The toast setting lives in the file only.", SettingsContract::ToastGlobalEditorID);
			return;
		}
		if (static_cast<int>(global->value) != toasts) {
			global->value = static_cast<float>(toasts);
			Log::INFO("SettingsService: {} <- {}.", SettingsContract::ToastGlobalEditorID, toasts);
		}
	}

	//SKYBAchievementFeed.psc still sets the global itself; this keeps the file in step.
	RE::BSEventNotifyControl SettingsService::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (a_event && a_event->eventName == SettingsContract::ModEvent::AchievementNotifyToggle) {
			SetInt(SettingsContract::AchievementsSection, SettingsContract::ToastKey, a_event->numArg != 0.0f ? 1 : 0);
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
