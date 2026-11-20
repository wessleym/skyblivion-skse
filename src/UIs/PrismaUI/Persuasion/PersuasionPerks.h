#pragma once
// Perk and skill modifiers for the persuasion wheel.
//
// The [PersuasionPerks.<condition>] sections of Data\SKSE\Plugins\SkyblivionSKSE.ini (PluginIni): each
// is a condition on the player, its lines change persuasion settings while the condition holds. Applied
// to a copy of the settings when the wheel opens (PersuasionRules::Resolve), in file order:
//
//   [PersuasionPerks.Perk:<EditorID>]            the player has the perk (editor ID through po3's Tweaks)
//   [PersuasionPerks.Perk:0x00ABCD~Plugin.esp]   the perk by form ID (no load-order byte) and plugin
//   [PersuasionPerks.Mastery:<Skill>:<rank>]     the player has rank 1..4 of that skill's SKYBMasteryPerkList entry
//   [PersuasionPerks.Skill:<ActorValue><cmp><n>] the player's current skill compared with n (>=, >, <=, <, =)
//   [PersuasionPerks.Always]                     no condition
//
//   Section.Key=value   set      Section.Key*=value   multiply      Section.Key+=value   add
//
// A value may count once for every point of an actor value: "<number> per <ActorValue>", with
// "up to <points>" to stop counting there (points below 0 count as 0). With += the product is
// added; with *= the setting is multiplied by 1 + the product, so a negative number lowers it:
//
//   General.GainMult*=0.0025 per Fame up to 100      x1.25 at 100 Fame or more
//   Bribe.CostMult*=-0.0025 per Infamy up to 100     x0.75 at 100 Infamy or more
//
// Limits, as a share of the value the setting had before any perk rule touched it:
//
//   Section.Key>=share   never below      Section.Key<=share   never above
//
// Keys are the names of the [Persuasion.*] settings (PersuasionSettingsTable.h). The rules are taken again
// when the file's write time changes, checked with the settings each time the wheel opens. While the file
// is missing or has no [PersuasionPerks.*] section, the built-in rules apply (kDefaultRules in
// PersuasionPerks.cpp); one such section, even an empty [PersuasionPerks.Always], replaces them all.
#include <cstdint>
#include <optional>
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

		// [PersuasionPerks.<condition>] in SkyblivionSKSE.ini
		static constexpr std::string_view kIniFeature = "PersuasionPerks";

	private:
		enum class Kind { kPerkEditorID, kPerkFormID, kMastery, kSkill, kAlways };

		struct Op
		{
			std::size_t index;  // into kSettingDefs
			char        op;     // '=', '*', '+', '>' (never below), '<' (never above)
			float       value;
			std::string    per;                                // "<value> per <ActorValue>": the actor value's name (for the log), else empty
			RE::ActorValue perValue{ RE::ActorValue::kNone };  // that actor value, resolved when the line is read
			float       upTo{ 0.0f };  // points of it counted at most; 0 = all of them
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

		static inline std::vector<Rule>                               s_rules;
		static inline std::uint32_t                                   s_generation = 0;  // PluginIni's reading the rules came from
	};
}
