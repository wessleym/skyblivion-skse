#include "Platform/JsonFile.h"
#include "Platform/Text.h"

#include <fstream>

namespace JsonFile
{
	std::optional<nlohmann::json> ReadObject(const std::filesystem::path& a_path, std::string_view a_source)
	{
		std::ifstream in(a_path);
		if (!in) {
			Log::WARN("{}: {} cannot be opened - skipped", a_source, Text::Utf8(a_path));
			return std::nullopt;
		}
		try {
			auto parsed = nlohmann::json::parse(in, nullptr, true, true);
			if (!parsed.is_object()) {
				Log::WARN("{}: {} is not a JSON object - skipped", a_source, Text::Utf8(a_path));
				return std::nullopt;
			}
			return parsed;
		} catch (const std::exception& e) {
			Log::WARN("{}: {} does not parse ({}) - skipped", a_source, Text::Utf8(a_path), e.what());
			return std::nullopt;
		}
	}
}
