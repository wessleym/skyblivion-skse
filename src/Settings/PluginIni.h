#pragma once
// SkyblivionSKSE.ini (Data\SKSE\Plugins): the one file Skyblivion.dll takes its settings from.
//
// A feature owns the sections that carry its name: [Feature] and [Feature.Group]. It asks for them with
// PluginIni::Read("Feature", ...) and receives its lines in file order, the section given without the
// feature's name: "" for [Feature], "Group" for [Feature.Group]. Which keys exist, what a value means and
// what the built-in default is stay with the feature; a missing file, section or key leaves it on its
// defaults.
//
// A new feature adds one row to kFeatures and its sections to the shipped file. It opens no file of its own.
//
// The file is read at plugin load (Load) and again whenever its write time has changed (Refresh, called by
// the features that take changes without a restart). Generation() counts the readings, so a feature can tell
// whether the values it holds are still the file's.
#include "Platform/IniFile.h"
#include "Platform/Text.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace PluginIni
{
	inline constexpr std::string_view kFolder = "Data/SKSE/Plugins/";
	inline constexpr std::string_view kFileName = "SkyblivionSKSE.ini";

	struct Feature
	{
		std::string_view name;        // the section name, and the start of [name.Group]
		std::string_view formerFile;  // the file of its own it read before 2026-11-17; named in the log if still present
	};

	inline constexpr std::array kFeatures{
		Feature{ "HoverCursor", "SKYBHoverCursor.ini" },
		Feature{ "DialogueServices", "SKYBDialogueServices.ini" },
		Feature{ "ScrollCrafting", "SKYBScrollCrafting.ini" },
		Feature{ "PerkSpells", "SKYBPerkSpells.ini" },
		Feature{ "SpellReflect", "SKYBSpellReflect.ini" },
		Feature{ "Degradation", "SKYBDegradation.ini" },
		Feature{ "Persuasion", "SKYBPersuasion.ini" },
		Feature{ "PersuasionPerks", "SKYBPersuasionPerks.ini" },
	};

	struct Section
	{
		std::string                 feature;  // as kFeatures spells it
		IniFile::Entry              header;   // section = the group ("" for [Feature]); line = the header's line
		std::vector<IniFile::Entry> lines;    // key=value lines, section = the group
	};

	struct File
	{
		bool                 found = false;
		std::uint32_t        generation = 0;
		std::vector<Section> sections;  // in file order; sections of no known feature are left out
	};

	// Plugin load: reads the file, and names in the log every former per-feature file still in the folder.
	void Load();
	// Reads the file again when its write time changed (or it appeared or went away).
	void Refresh();
	// The file as last read (read now if Load has not run yet).
	[[nodiscard]] std::shared_ptr<const File> Current();
	// 0 before the first reading, then one more each time the file is read.
	[[nodiscard]] std::uint32_t Generation();
	// Whether the file has a section of this feature, even an empty one.
	[[nodiscard]] bool Has(std::string_view a_feature);
	// "SkyblivionSKSE.ini [Feature]", for log lines.
	[[nodiscard]] std::string Where(std::string_view a_feature);

	// a_onSection(const IniFile::Entry&) for each of the feature's section headers, a_onEntry(const IniFile::Entry&)
	// for each key=value line under it. False when the file is missing.
	template <class S, class F>
	bool Read(std::string_view a_feature, S&& a_onSection, F&& a_onEntry)
	{
		const auto file = Current();
		for (const auto& section : file->sections) {
			if (!Text::IEquals(section.feature, a_feature)) {
				continue;
			}
			a_onSection(section.header);
			for (const auto& line : section.lines) {
				a_onEntry(line);
			}
		}
		return file->found;
	}

	template <class F>
	bool Read(std::string_view a_feature, F&& a_onEntry)
	{
		return Read(a_feature, [](const IniFile::Entry&) {}, a_onEntry);
	}
}
