// Moved from SKYBDegradation.dll into Skyblivion.dll, 2026-10-05 (fold-in, UI integration workspace
// ledger L-0058). Source of record until then: game\native\SKYBDegradation\src in that workspace.
#include "Degradation/Settings.h"
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

namespace SKYBDeg
{
	namespace
	{
		std::string_view Trim(std::string_view a_s)
		{
			constexpr std::string_view ws = " \t\r\n";
			const auto b = a_s.find_first_not_of(ws);
			if (b == std::string_view::npos) {
				return {};
			}
			return a_s.substr(b, a_s.find_last_not_of(ws) - b + 1);
		}

		bool IEquals(std::string_view a, std::string_view b)
		{
			return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
					   return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
				   });
		}
	}

	int Settings::Find(std::string_view a_section, std::string_view a_key)
	{
		for (std::size_t i = 0; i < std::size(kSettingDefs); ++i) {
			if (IEquals(a_section, kSettingDefs[i].section) && IEquals(a_key, kSettingDefs[i].key)) {
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	void Settings::Load()
	{
		for (std::size_t i = 0; i < s_ini.size(); ++i) {
			s_ini[i] = kSettingDefs[i].fallback;
		}

		const std::filesystem::path path{ kIniPath };
		std::error_code ec;
		const auto time = std::filesystem::last_write_time(path, ec);
		std::ifstream in(path);
		if (ec || !in) {
			s_iniSeen = false;
			Log::WARN("settings: {} not found - every value uses its built-in default", kIniPath);
			Recompose();
			return;
		}
		s_iniSeen = true;
		s_iniTime = time;

		std::string section;
		std::string line;
		std::size_t lineNo = 0, read = 0, changed = 0;
		while (std::getline(in, line)) {
			++lineNo;
			auto s = Trim(line);
			if (s.empty() || s.front() == ';' || s.front() == '#') {
				continue;
			}
			if (s.front() == '[') {
				const auto close = s.find(']');
				section = std::string(Trim(s.substr(1, close == std::string_view::npos ? std::string_view::npos : close - 1)));
				continue;
			}
			const auto eq = s.find('=');
			if (eq == std::string_view::npos) {
				Log::WARN("settings: line {} is not key=value: '{}'", lineNo, s);
				continue;
			}
			const auto key = Trim(s.substr(0, eq));
			auto value = s.substr(eq + 1);
			if (const auto c = value.find_first_of(";#"); c != std::string_view::npos) {
				value = value.substr(0, c);
			}
			value = Trim(value);
			const int idx = Find(section, key);
			if (idx < 0) {
				Log::WARN("settings: unknown key [{}] {} (line {}) - ignored", section, key, lineNo);
				continue;
			}
			float v = 0.0f;
			const auto [ptr, err] = std::from_chars(value.data(), value.data() + value.size(), v);
			if (err != std::errc{} || ptr != value.data() + value.size()) {
				Log::WARN("settings: [{}] {} = '{}' is not a number (line {}) - default {} kept", section, key, value, lineNo,
							 kSettingDefs[idx].fallback);
				continue;
			}
			s_ini[static_cast<std::size_t>(idx)] = v;
			++read;
			if (v != kSettingDefs[idx].fallback) {
				++changed;
				Log::INFO("settings: [{}] {} = {} (default {})", section, key, v, kSettingDefs[idx].fallback);
			}
		}
		Log::INFO("settings: {} read {} of {} values, {} differ from the defaults", kIniPath, read, s_ini.size(), changed);
		Recompose();
	}

	void Settings::Recompose()
	{
		std::size_t n = 0;
		for (std::size_t i = 0; i < s_values.size(); ++i) {
			s_values[i] = s_overrides[i] ? *s_overrides[i] : s_ini[i];
			n += s_overrides[i] ? 1 : 0;
		}
		if (n) {
			Log::INFO("settings: {} value(s) set by the settings menu override the INI", n);
		}
	}

	bool Settings::SetOverride(std::string_view a_section, std::string_view a_key, float a_value)
	{
		const int idx = Find(a_section, a_key);
		if (idx < 0) {
			Log::WARN("settings menu: unknown key [{}] {} - ignored", a_section, a_key);
			return false;
		}
		const auto i = static_cast<std::size_t>(idx);
		if (!s_overrides[i] || *s_overrides[i] != a_value) {
			Log::INFO("settings menu: [{}] {} = {} (INI {})", kSettingDefs[i].section, kSettingDefs[i].key, a_value, s_ini[i]);
		}
		s_overrides[i] = a_value;
		s_values[i] = a_value;
		return true;
	}

	bool Settings::ClearOverride(std::string_view a_section, std::string_view a_key)
	{
		const int idx = Find(a_section, a_key);
		if (idx < 0) {
			return false;
		}
		const auto i = static_cast<std::size_t>(idx);
		if (s_overrides[i]) {
			Log::INFO("settings menu: [{}] {} back to the INI value {}", kSettingDefs[i].section, kSettingDefs[i].key, s_ini[i]);
		}
		s_overrides[i].reset();
		s_values[i] = s_ini[i];
		return true;
	}

	void Settings::ReloadIfChanged()
	{
		std::error_code ec;
		const auto time = std::filesystem::last_write_time(std::filesystem::path{ kIniPath }, ec);
		const bool present = !ec;
		if (present == s_iniSeen && (!present || time == s_iniTime)) {
			return;
		}
		Log::INFO("settings: {} changed on disk - reloading", kIniPath);
		Load();
	}

	void Settings::Resolve()
	{
		Load();

		auto dh = RE::TESDataHandler::GetSingleton();
		s_pluginLoaded = dh && dh->LookupModByName(kPlugin) != nullptr;
		if (!s_pluginLoaded) {
			Log::WARN("{} is not active: no repair tool is known (the hammer will not open the menu)", kPlugin);
		}

		s_tools = dh ? dh->LookupForm<RE::BGSListForm>(0x800, kPlugin) : nullptr;
		s_hammers.clear();
		if (!s_tools) {
			if (s_pluginLoaded) {
				Log::ERROR("FLST 000800 (SKYBDegRepairTools) missing from {}", kPlugin);
			}
			return;
		}
		Log::INFO("repair tools list {:08X}: {} entr{}", s_tools->GetFormID(), s_tools->forms.size(),
					 s_tools->forms.size() == 1 ? "y" : "ies");
		for (std::uint32_t i = 0; i < s_tools->forms.size(); ++i) {
			auto form = s_tools->forms[i] ? s_tools->forms[i]->As<RE::TESBoundObject>() : nullptr;
			if (!form) {
				Log::ERROR("  tool entry {} is not an item - skipped", i);
				continue;
			}
			if (i >= kHammerKeys.size()) {
				Log::ERROR("  tool {:08X} '{}' (entry {}) has no [Hammers] keys - skipped", form->GetFormID(), form->GetName(), i);
				continue;
			}
			Hammer h{ form, kHammerKeys[i], Find("Hammers", std::format("{}Break", kHammerKeys[i])),
					  Find("Hammers", std::format("{}Tier", kHammerKeys[i])) };
			if (h.breakChance < 0 || h.tier < 0) {
				Log::ERROR("  Settings.csv has no [Hammers] {}Break / {}Tier - tool {:08X} skipped", h.key, h.key, form->GetFormID());
				continue;
			}
			Log::INFO("  hammer {} '{}' ({:08X}) = [Hammers] {}: break {:.2f}, tier {:.1f}", s_hammers.size(), form->GetName(),
						 form->GetFormID(), h.key, At(h.breakChance), At(h.tier));
			s_hammers.push_back(h);
		}
	}
}
