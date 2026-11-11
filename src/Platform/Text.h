#pragma once
// Small text helpers shared by the plugin's INI, JSON and data-file readers.
#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Text
{
	// A copy, so it can be kept whatever a_s points into.
	inline std::string Trim(std::string_view a_s)
	{
		constexpr std::string_view ws = " \t\r\n";
		const auto b = a_s.find_first_not_of(ws);
		if (b == std::string_view::npos) {
			return {};
		}
		return std::string(a_s.substr(b, a_s.find_last_not_of(ws) - b + 1));
	}

	inline std::string Lower(std::string_view a_text)
	{
		std::string out(a_text);
		std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return out;
	}

	inline bool IEquals(std::string_view a, std::string_view b)
	{
		return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
				   return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
			   });
	}

	inline bool IEndsWith(std::string_view a_text, std::string_view a_tail)
	{
		return a_text.size() >= a_tail.size() && IEquals(a_text.substr(a_text.size() - a_tail.size()), a_tail);
	}

	// a_text cut at every a_separator, each piece trimmed. Empty pieces are dropped, unless a_keepEmpty (so that
	// positions line up).
	inline std::vector<std::string> Split(std::string_view a_text, char a_separator, bool a_keepEmpty = false)
	{
		std::vector<std::string> pieces;
		while (true) {
			const auto at = a_text.find(a_separator);
			if (auto piece = Trim(a_text.substr(0, at)); a_keepEmpty || !piece.empty()) {
				pieces.push_back(std::move(piece));
			}
			if (at == std::string_view::npos) {
				return pieces;
			}
			a_text.remove_prefix(at + 1);
		}
	}

	// The whole of a_text (spaces trimmed) as a number.
	template <class T>
	bool Parse(std::string_view a_text, T& a_out)
	{
		const auto t = Trim(a_text);
		const auto [ptr, err] = std::from_chars(t.data(), t.data() + t.size(), a_out);
		return err == std::errc{} && ptr == t.data() + t.size();
	}

	// 1 / true / yes / on, 0 / false / no / off (any case); a_fallback for anything else.
	inline bool ParseBool(std::string_view a_text, bool a_fallback)
	{
		const auto v = Lower(Trim(a_text));
		if (v == "1" || v == "true" || v == "yes" || v == "on") {
			return true;
		}
		if (v == "0" || v == "false" || v == "no" || v == "off") {
			return false;
		}
		return a_fallback;
	}

	// A path as UTF-8, with forward slashes. path::string() and generic_string() throw when a name
	// does not fit the ANSI code page.
	inline std::string Utf8(const std::filesystem::path& a_path)
	{
		const auto text = a_path.generic_u8string();
		return { reinterpret_cast<const char*>(text.data()), text.size() };
	}

	// The file's write time, or nullopt when it is missing.
	inline std::optional<std::filesystem::file_time_type> WriteTime(std::string_view a_path)
	{
		std::error_code ec;
		const auto      time = std::filesystem::last_write_time(std::filesystem::path{ a_path }, ec);
		return ec ? std::nullopt : std::optional{ time };
	}
}
