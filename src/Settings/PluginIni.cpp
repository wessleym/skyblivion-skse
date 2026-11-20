#include "Settings/PluginIni.h"

#include <filesystem>
#include <fstream>
#include <optional>

namespace PluginIni
{
	namespace
	{
		File                                           g_file;   // generation 0 until the first reading
		std::optional<std::filesystem::file_time_type> g_stamp;  // the write time of what g_file holds; nullopt: no file

		std::string Path()
		{
			return std::string(kFolder) + std::string(kFileName);
		}

		const Feature* FeatureNamed(std::string_view a_name)
		{
			for (const auto& feature : kFeatures) {
				if (Text::IEquals(feature.name, a_name)) {
					return &feature;
				}
			}
			return nullptr;
		}

		void ReadFile()
		{
			const auto path = Path();
			File       file;
			file.generation = g_file.generation + 1;
			g_stamp = Text::WriteTime(path);

			std::ifstream in{ std::filesystem::path{ path } };
			file.found = g_stamp.has_value() && static_cast<bool>(in);
			if (!file.found) {
				g_stamp.reset();
				Log::WARN("{}: not found in {} - every feature uses its built-in defaults", kFileName, kFolder);
				g_file = std::move(file);
				return;
			}

			Section*    current = nullptr;  // null: lines here belong to no feature
			std::size_t lines = 0;
			IniFile::Parse(
				in, path, kFileName,
				[&](const IniFile::Entry& a_header) {
					// [Feature] or [Feature.Group]: the feature's name ends at the first dot
					const auto  dot = a_header.section.find('.');
					const auto  name = Text::Trim(std::string_view(a_header.section).substr(0, dot));
					const auto* feature = FeatureNamed(name);
					if (!feature) {
						current = nullptr;
						Log::WARN("{}: line {}: [{}] belongs to no feature of this plugin - the section is ignored", kFileName,
							a_header.line, a_header.section);
						return;
					}
					current = &file.sections.emplace_back();
					current->feature = std::string(feature->name);
					current->header = a_header;
					current->header.section =
						dot == std::string::npos ? std::string() : Text::Trim(std::string_view(a_header.section).substr(dot + 1));
				},
				[&](const IniFile::Entry& a_entry) {
					if (a_entry.section.empty()) {
						Log::WARN("{}: line {}: '{}' stands above the first [section] - ignored", kFileName, a_entry.line, a_entry.key);
						return;
					}
					if (current) {
						auto& line = current->lines.emplace_back(a_entry);
						line.section = current->header.section;
						++lines;
					}
				});
			Log::INFO("{}: read {} - {} value(s) in {} section(s)", kFileName, path, lines, file.sections.size());
			g_file = std::move(file);
		}

		// The per-feature files this plugin read before it had one file. They are not merged in: two sources for
		// one value would leave nobody sure which one counts.
		void NameFormerFiles()
		{
			for (const auto& feature : kFeatures) {
				std::error_code ec;
				const auto      old = std::string(kFolder) + std::string(feature.formerFile);
				if (std::filesystem::exists(std::filesystem::path{ old }, ec)) {
					Log::WARN("{}: {} is still in {} and is IGNORED - its settings are read from [{}] in {} only. Delete the old file.",
						kFileName, feature.formerFile, kFolder, feature.name, kFileName);
				}
			}
		}
	}

	void Load()
	{
		ReadFile();
		NameFormerFiles();
	}

	void Refresh()
	{
		if (Text::WriteTime(Path()) != g_stamp) {
			Log::INFO("{}: changed on disk - reading it again", kFileName);
			ReadFile();
		}
	}

	const File& Current()
	{
		return g_file;
	}

	std::uint32_t Generation()
	{
		return g_file.generation;
	}

	std::string Where(std::string_view a_feature)
	{
		return std::string(kFileName) + " [" + std::string(a_feature) + "]";
	}
}
