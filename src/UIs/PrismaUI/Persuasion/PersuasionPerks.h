#pragma once
// Perk and skill modifiers for the persuasion wheel.
//
// Data\SKSE\Plugins\SKYBPersuasionPerks.ini: each [section] is a condition on the player, its
// lines change persuasion settings while the condition holds. Applied to a copy of the settings
// when the wheel opens (PersuasionRules::Resolve), in file order:
//
//   [Perk:<EditorID>]            the player has the perk (editor ID through po3's Tweaks)
//   [Perk:0x00ABCD~Plugin.esp]   the perk by form ID (no load-order byte) and plugin
//   [Mastery:<Skill>:<rank>]     the player has rank 1..4 of that skill's SKYBMasteryPerkList entry
//   [Skill:<ActorValue><cmp><n>] the player's current skill compared with n (>=, >, <=, <, =)
//
//   Section.Key=value   set      Section.Key*=value   multiply      Section.Key+=value   add
//
// Keys are the names of SKYBPersuasion.ini (PersuasionSettingsTable.h). The file is re-read when
// its write time changes, checked with the settings INI each time the wheel opens. Without the
// file, the built-in rules apply (kDefaultRules in PersuasionPerks.cpp).
#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "PersuasionSettings.h"

namespace RE { class PlayerCharacter; }

namespace Persuasion
{
	class Perks
	{
	public:
		static void Load();
		static void ReloadIfChanged();

		// Applies every rule the player meets to a_values. Returns how many rules held.
		static std::size_t Apply(Settings::Values& a_values);

		static constexpr std::string_view kIniPath = "Data/SKSE/Plugins/SKYBPersuasionPerks.ini";

	private:
		enum class Kind { kPerkEditorID, kPerkFormID, kMastery, kSkill };

		struct Op
		{
			std::size_t index;  // into kSettingDefs
			char        op;     // '=', '*', '+'
			float       value;
		};

		struct Rule
		{
			std::string      header;   // as written, for the log
			Kind             kind;
			std::string      name;     // editor ID, plugin-less form text, skill or actor value name
			std::string      plugin;   // kPerkFormID
			std::uint32_t    formID{ 0 };
			int              rank{ 0 };  // kMastery, 1-based
			std::string      cmp;      // kSkill
			float            threshold{ 0.0f };
			std::vector<Op>  ops;
		};

		static bool ParseHeader(std::string_view a_text, Rule& a_rule, std::size_t a_lineNo);
		static bool Holds(const Rule& a_rule, RE::PlayerCharacter* a_player);

		static inline std::vector<Rule>                 s_rules;
		static inline std::filesystem::file_time_type   s_time{};
		static inline bool                              s_seen = false;
	};
}
