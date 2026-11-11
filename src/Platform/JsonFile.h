#pragma once
#include <filesystem>
#include <optional>
#include <string_view>

#include <nlohmann/json.hpp>

namespace JsonFile
{
	// The file's JSON object (comments allowed). nullopt, logged under a_source with the path, when the
	// file cannot be opened, does not parse, or holds something other than an object.
	[[nodiscard]] std::optional<nlohmann::json> ReadObject(const std::filesystem::path& a_path, std::string_view a_source);
}
