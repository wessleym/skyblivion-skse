#pragma once
// Reads an INI file: [section] headers and key=value lines, in file order. Blank lines are skipped, and ; or #
// starts a comment anywhere on a line. Sections, keys and values arrive trimmed, as written.
#include "Platform/Text.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

namespace IniFile
{
	struct Entry
	{
		std::string section;
		std::string key;
		std::string value;
		std::size_t line = 0;
	};

	// Calls a_onEntry(const Entry&) for every key=value line. False when the file cannot be opened. A line that
	// is neither a section nor key=value is logged under a_source and skipped.
	template <class F>
	bool Read(const std::filesystem::path& a_path, std::string_view a_source, F&& a_onEntry)
	{
		std::ifstream in{ a_path };
		if (!in) {
			return false;
		}
		Entry entry;
		std::string line;
		while (std::getline(in, line)) {
			++entry.line;
			const auto text = Text::Trim(std::string_view(line).substr(0, line.find_first_of(";#")));
			if (text.empty()) {
				continue;
			}
			if (text.front() == '[') {
				entry.section = Text::Trim(std::string_view(text).substr(1, text.find(']') - 1));
				continue;
			}
			const auto eq = text.find('=');
			if (eq == std::string::npos) {
				Log::WARN("{}: {} line {} is not key=value: '{}'", a_source, Text::Utf8(a_path), entry.line, text);
				continue;
			}
			entry.key = Text::Trim(std::string_view(text).substr(0, eq));
			entry.value = Text::Trim(std::string_view(text).substr(eq + 1));
			a_onEntry(std::as_const(entry));
		}
		return true;
	}
}
