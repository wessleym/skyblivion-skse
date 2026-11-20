#pragma once

// The line format of Skyblivion's string files (see TranslationService.h), kept free of
// game headers. TranslationService is the only runtime user.

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace Translation::StringsFormat {
	inline void TrimInPlace(std::string& text) {
		const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
		text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
		text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(), text.end());
	}

	// \n \t \\ are expanded; every other backslash is kept literally, so
	// ordinary prose needs no escaping.
	inline std::string ExpandEscapes(std::string_view raw) {
		std::string out;
		out.reserve(raw.size());
		for (std::size_t i = 0; i < raw.size(); ++i) {
			if (raw[i] == '\\' && i + 1 < raw.size()) {
				switch (raw[i + 1]) {
					case 'n':
						out.push_back('\n');
						++i;
						continue;
					case 't':
						out.push_back('\t');
						++i;
						continue;
					case '\\':
						out.push_back('\\');
						++i;
						continue;
					default:
						break;
				}
			}
			out.push_back(raw[i]);
		}
		return out;
	}

	enum class Line { kSkip, kEntry, kNoSeparator, kEmptyKey };

	// One line, BOM and trailing '\r' already removed. On kEntry, key has its
	// leading '$' and value is the raw text (escapes NOT expanded yet).
	//
	// Tab is the separator and everything after it is the value VERBATIM,
	// edge spaces included, as in the engine's own translation files.
	// Menus concatenate around them: the character sheet shows "Fame : " +
	// number, the class menu "+5" + " To the highlighted attributes". The line
	// is not trimmed first: that would remove those spaces and make "$KEY<TAB>"
	// lose its tab and be skipped. '=' is accepted when
	// no tab is present, so a hand-edited line without a tab still parses; that
	// form IS trimmed, so "KEY = Value" yields key "KEY" and value "Value".
	inline Line Parse(const std::string& line, std::string& key, std::string& value) {
		// Blank and comment lines are judged after any indentation.
		const auto start = line.find_first_not_of(" \t\n\v\f\r");
		if (start == std::string::npos || line[start] == '#' || line[start] == ';') {
			return Line::kSkip;
		}
		if (const auto tab = line.find('\t', start); tab != std::string::npos) {
			key = line.substr(start, tab - start);
			value = line.substr(tab + 1);
		} else if (const auto equalsSign = line.find('=', start); equalsSign != std::string::npos) {
			key = line.substr(start, equalsSign - start);
			value = line.substr(equalsSign + 1);
			TrimInPlace(value);
		} else {
			return Line::kNoSeparator;
		}
		TrimInPlace(key);
		if (key.empty()) {
			return Line::kEmptyKey;
		}
		if (key.front() != '$') {
			key.insert(key.begin(), '$');
		}
		return Line::kEntry;
	}
}
