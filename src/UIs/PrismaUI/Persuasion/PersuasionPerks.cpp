#include "PersuasionPerks.h"

#include "Platform/FormCache.h"
#include "Settings/PluginIni.h"
#include "UIs/Scaleform/Skills/SkillData.h"

#include <algorithm>
#include <charconv>
#include <sstream>

namespace Persuasion
{
	namespace
	{
		// Used when SkyblivionSKSE.ini is missing or has no [PersuasionPerks.*] section, the same rules the
		// shipped file holds (there each header carries the "PersuasionPerks." prefix): Speechcraft 100 halves the
		// bribe cost, the first Speechcraft mastery stops decay while the wheel is open, and the
		// Journeyman and Master masteries give one and two free turns of the wheel per conversation.
		// Person of Interest raises the bonus of positive responses in the disposition mini game:
		// each of its four ranks makes loved and liked answers worth 10% more (+40% at rank 4), and
		// bribes 10 / 20 / 30 / 40% cheaper in total (each rank's factor is the step from the one before).
		// Reputation: the wheel's gains grow 0.25% per point of Fame and bribes cost 0.25% less per point
		// of Infamy, each counted up to 100 points. No stack of rules takes the bribe price below a
		// quarter of what the settings ask.
		constexpr std::string_view kDefaultRules =
			"[Skill:Speechcraft>=100]\nBribe.CostMult*=0.5\n"
			"[Mastery:Speechcraft:1]\nDecay.WhileOpen=0\n"
			"[Mastery:Speechcraft:2]\nGeneral.FreeRotations=1\n[Mastery:Speechcraft:4]\nGeneral.FreeRotations=2\n"
			"[Perk:0x09D03C~Skyblivion.esm]\nResponse.LoveMult+=0.1\nResponse.LikeMult+=0.1\nBribe.CostMult*=0.9\n"
			"[Perk:0x09D03B~Skyblivion.esm]\nResponse.LoveMult+=0.1\nResponse.LikeMult+=0.1\nBribe.CostMult*=0.888889\n"
			"[Perk:0x1A8CDA~Skyblivion.esm]\nResponse.LoveMult+=0.1\nResponse.LikeMult+=0.1\nBribe.CostMult*=0.875\n"
			"[Perk:0x28F5CB~Skyblivion.esm]\nResponse.LoveMult+=0.1\nResponse.LikeMult+=0.1\nBribe.CostMult*=0.857143\n"
			"[Perk:0x1A8CE3~Skyblivion.esm]\nGeneral.GainMult*=0.0025 per Fame up to 100\n"
			"Bribe.CostMult*=-0.0025 per Infamy up to 100\n"
			"[Always]\nBribe.CostMult>=0.25\n";

		RE::ActorValue ActorValueNamed(const std::string& a_name)
		{
			const auto av = REBridge::ActorValueByName(a_name.c_str());
			//The game spells the skill "Speechcraft" but speaks of "Speech" as often as not.
			if (av == RE::ActorValue::kNone && Text::IEquals(a_name, "Speech")) {
				return RE::ActorValue::kSpeech;
			}
			return av;
		}
	}

	bool Perks::ParseHeader(std::string_view a_text, Rule& a_rule, std::size_t a_lineNo)
	{
		a_rule.header = std::string(a_text);
		if (Text::IEquals(Text::Trim(a_text), "Always")) {
			a_rule.kind = Kind::kAlways;
			return true;
		}
		const auto colon = a_text.find(':');
		if (colon == std::string_view::npos) {
			Log::WARN("persuasion perks: line {}: section [{}] has no kind (Perk:, Mastery:, Skill:, Always) - ignored", a_lineNo, a_text);
			return false;
		}
		const auto kind = Text::Trim(a_text.substr(0, colon));
		const auto rest = Text::Trim(a_text.substr(colon + 1));
		if (rest.empty()) {
			Log::WARN("persuasion perks: line {}: section [{}] names nothing - ignored", a_lineNo, a_text);
			return false;
		}
		if (Text::IEquals(kind, "Perk")) {
			const auto tilde = rest.find('~');
			if (tilde != std::string_view::npos && rest.size() > 2 && rest[0] == '0' && (rest[1] == 'x' || rest[1] == 'X')) {
				const auto hex = rest.substr(2, tilde - 2);
				std::uint32_t id = 0;
				const auto [ptr, err] = std::from_chars(hex.data(), hex.data() + hex.size(), id, 16);
				if (err != std::errc{} || ptr != hex.data() + hex.size()) {
					Log::WARN("persuasion perks: line {}: [{}] is not 0x<hex>~<plugin> - ignored", a_lineNo, a_text);
					return false;
				}
				a_rule.kind = Kind::kPerkFormID;
				a_rule.formID = id & 0x00FFFFFF;
				a_rule.plugin = Text::Trim(rest.substr(tilde + 1));
				return !a_rule.plugin.empty();
			}
			a_rule.kind = Kind::kPerkEditorID;
			a_rule.name = std::string(rest);
			return true;
		}
		if (Text::IEquals(kind, "Mastery")) {
			const auto c2 = rest.find(':');
			int rank = 0;
			if (c2 == std::string_view::npos || !Text::Parse(rest.substr(c2 + 1), rank) || rank < 1) {
				Log::WARN("persuasion perks: line {}: [{}] must be Mastery:<Skill>:<rank 1..> - ignored", a_lineNo, a_text);
				return false;
			}
			a_rule.kind = Kind::kMastery;
			a_rule.name = Text::Trim(rest.substr(0, c2));
			a_rule.rank = rank;
			return true;
		}
		if (Text::IEquals(kind, "Skill")) {
			static constexpr std::array<std::string_view, 5> kCmps{ ">=", "<=", ">", "<", "=" };
			for (const auto cmp : kCmps) {
				const auto at = rest.find(cmp);
				if (at == std::string_view::npos) {
					continue;
				}
				float n = 0.0f;
				if (!Text::Parse(rest.substr(at + cmp.size()), n)) {
					break;
				}
				a_rule.kind = Kind::kSkill;
				a_rule.name = Text::Trim(rest.substr(0, at));
				a_rule.cmp = std::string(cmp);
				a_rule.threshold = n;
				return !a_rule.name.empty();
			}
			Log::WARN("persuasion perks: line {}: [{}] must be Skill:<ActorValue><cmp><number> - ignored", a_lineNo, a_text);
			return false;
		}
		Log::WARN("persuasion perks: line {}: unknown section kind [{}] - ignored", a_lineNo, a_text);
		return false;
	}

	void Perks::Load()
	{
		s_rules.clear();

		std::size_t ops = 0;
		bool        sectionValid = false;
		//A section header: the condition, as written after "PersuasionPerks." (the whole header in the built-in rules).
		const auto onSection = [&](const IniFile::Entry& a_header) {
			Rule rule;
			sectionValid = ParseHeader(a_header.section, rule, a_header.line);
			if (sectionValid) {
				s_rules.push_back(std::move(rule));
			}
		};
		//Section.Key<op>value, op one of "=", "*=", "+=", ">=", "<=": the key arrives with the operator's first
		//character still on it.
		const auto onEntry = [&](const IniFile::Entry& a_entry) {
			if (!sectionValid) {
				return;
			}
			const auto lineNo = a_entry.line;
			char       op = '=';
			auto       target = a_entry.key;
			if (!target.empty() && std::string_view("*+><").find(target.back()) != std::string_view::npos) {
				op = target.back();
				target = Text::Trim(std::string_view(target).substr(0, target.size() - 1));
			}
			if (target.empty()) {
				Log::WARN("persuasion perks: line {} is not Section.Key=value: '{}={}'", lineNo, a_entry.key, a_entry.value);
				return;
			}
			const auto dot = target.find('.');
			if (dot == std::string::npos) {
				Log::WARN("persuasion perks: line {}: '{}' must be Section.Key", lineNo, target);
				return;
			}
			std::string_view value = a_entry.value;
			//"<number> per <ActorValue> [up to <points>]": the number counts once for each point.
			std::string per;
			float       upTo = 0.0f;
			if (const auto at = Text::Lower(value).find(" per "); at != std::string::npos) {
				auto scale = Text::Trim(value.substr(at + 5));
				value = value.substr(0, at);
				if (const auto cap = Text::Lower(scale).find(" up to "); cap != std::string::npos) {
					if (!Text::Parse(std::string_view(scale).substr(cap + 7), upTo) || upTo <= 0.0f) {
						Log::WARN("persuasion perks: line {}: 'up to' needs a number above 0: '{}={}'", lineNo, a_entry.key, a_entry.value);
						return;
					}
					scale = Text::Trim(std::string_view(scale).substr(0, cap));
				}
				if (scale.empty() || op == '>' || op == '<') {
					Log::WARN("persuasion perks: line {}: 'per' needs an actor value and one of =, *=, +=: '{}={}'", lineNo, a_entry.key,
						a_entry.value);
					return;
				}
				per = std::move(scale);
			}
			float v = 0.0f;
			if (!Text::Parse(value, v)) {
				Log::WARN("persuasion perks: line {}: '{}' is not a number", lineNo, Text::Trim(value));
				return;
			}
			const int idx = Settings::Find(std::string_view(target).substr(0, dot), std::string_view(target).substr(dot + 1));
			if (idx < 0) {
				Log::WARN("persuasion perks: line {}: unknown setting {} - ignored", lineNo, target);
				return;
			}
			s_rules.back().ops.push_back(Op{ static_cast<std::size_t>(idx), op, v, std::move(per), upTo });
			++ops;
		};

		const bool fromFile = PluginIni::Has(kIniFeature);
		s_generation = PluginIni::Generation();
		if (fromFile) {
			PluginIni::Read(kIniFeature, onSection, onEntry);
		} else {
			Log::INFO("persuasion perks: {} has no [{}.*] section - using the built-in rules", PluginIni::kFileName, kIniFeature);
			std::istringstream builtIn{ std::string(kDefaultRules) };
			IniFile::Parse(builtIn, "the built-in rules", "persuasion perks", onSection, onEntry);
		}
		Log::INFO("persuasion perks: {} read: {} section(s), {} change(s)", fromFile ? PluginIni::Where(kIniFeature) : "built-in rules",
			s_rules.size(), ops);
	}

	void Perks::ReloadIfChanged()
	{
		PluginIni::Refresh();
		if (PluginIni::Generation() == s_generation) {
			return;
		}
		Log::INFO("persuasion perks: {} was read again - reloading", PluginIni::kFileName);
		Load();
	}

	bool Perks::Holds(const Rule& a_rule, RE::PlayerCharacter* a_player)
	{
		switch (a_rule.kind) {
		case Kind::kAlways:
			return true;
		//The perk records resolve once, on the first conversation (the form table logs a miss once).
		case Kind::kPerkEditorID: {
			const std::string source = "persuasion perks [" + a_rule.header + "]";
			auto* perk = FormCache::Get<RE::BGSPerk>(a_rule.name, source);
			return perk && a_player->HasPerk(perk);
		}
		case Kind::kPerkFormID: {
			const std::string source = "persuasion perks [" + a_rule.header + "]";
			auto* perk = FormCache::GetLocal<RE::BGSPerk>(a_rule.formID, a_rule.plugin, source);
			return perk && a_player->HasPerk(perk);
		}
		case Kind::kMastery: {
			const auto av = ActorValueNamed(a_rule.name);
			const auto index = Skills::SkillData::IndexOf(av);
			if (!index) {
				Log::WARN("persuasion perks: [{}]: {} is not one of the 18 skills", a_rule.header, a_rule.name);
				return false;
			}
			const auto perks = Skills::SkillData::MasteryPerks(*index, "persuasion perks");
			if (!perks) {
				return false;
			}
			const auto i = static_cast<std::size_t>(a_rule.rank - 1);
			if (i >= perks->size()) {
				Log::WARN("persuasion perks: [{}]: the {} list has {} perks, no rank {}", a_rule.header, a_rule.name, perks->size(), a_rule.rank);
				return false;
			}
			return (*perks)[i].owned;
		}
		case Kind::kSkill: {
			const auto av = ActorValueNamed(a_rule.name);
			if (av == RE::ActorValue::kNone) {
				Log::WARN("persuasion perks: [{}]: no actor value named {}", a_rule.header, a_rule.name);
				return false;
			}
			auto* owner = REBridge::AVOwner(a_player);
			if (!owner) {
				return false;
			}
			const float value = owner->GetActorValue(av);
			const float n = a_rule.threshold;
			if (a_rule.cmp == ">=") return value >= n;
			if (a_rule.cmp == "<=") return value <= n;
			if (a_rule.cmp == ">") return value > n;
			if (a_rule.cmp == "<") return value < n;
			return value == n;
		}
		}
		return false;
	}

	std::size_t Perks::Apply(Settings::Values& a_values)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || s_rules.empty()) {
			return 0;
		}
		//The limits (>=, <=) measure against the settings as they stood before the first rule.
		const Settings::Values start = a_values;
		std::size_t held = 0;
		for (const auto& rule : s_rules) {
			if (!Holds(rule, player)) {
				continue;
			}
			++held;
			for (const auto& op : rule.ops) {
				const auto& def = Settings::Def(op.index);
				float amount = op.value;
				if (!op.per.empty()) {
					const auto av = ActorValueNamed(op.per);
					auto* owner = REBridge::AVOwner(player);
					if (av == RE::ActorValue::kNone || !owner) {
						Log::WARN("persuasion perks: [{}]: [{}] {}: no actor value named {} - line skipped", rule.header,
							def.section, def.key, op.per);
						continue;
					}
					float points = std::max(0.0f, owner->GetActorValue(av));
					if (op.upTo > 0.0f) {
						points = std::min(points, op.upTo);
					}
					amount *= points;
				}
				float& slot = a_values[op.index];
				const float before = slot;
				switch (op.op) {
				case '*': slot *= op.per.empty() ? amount : 1.0f + amount; break;
				case '+': slot += amount; break;
				case '>': slot = std::max(slot, amount * start[op.index]); break;
				case '<': slot = std::min(slot, amount * start[op.index]); break;
				default:  slot = amount; break;
				}
				Log::INFO("persuasion perks: [{}]: [{}] {} {}= {}{} -> {} (was {})", rule.header, def.section, def.key,
					op.op == '=' ? "" : std::string(1, op.op), op.value, op.per.empty() ? "" : " per " + op.per, slot, before);
			}
		}
		return held;
	}
}
