#include "PersuasionPerks.h"

#include "Platform/FormCache.h"
#include "UIs/Scaleform/Skills/SkillData.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>

namespace Persuasion
{
	namespace
	{
		// Used when the INI is missing, the same rules the shipped file holds: Speechcraft 100 halves the
		// bribe cost, and the first Speechcraft mastery stops decay while the wheel is open.
		// Person of Interest raises the bonus of positive responses in the disposition mini game:
		// each of its four ranks makes loved and liked answers worth 10% more (+40% at rank 4)
		constexpr std::string_view kDefaultRules =
			"[Skill:Speechcraft>=100]\nBribe.CostMult*=0.5\n"
			"[Mastery:Speechcraft:1]\nDecay.WhileOpen=0\n"
			"[Perk:0x09D03C~Skyblivion.esm]\nResponse.LoveMult+=0.1\nResponse.LikeMult+=0.1\n[Perk:0x09D03B~Skyblivion.esm]\nResponse.LoveMult+=0.1\nResponse.LikeMult+=0.1\n[Perk:0x1A8CDA~Skyblivion.esm]\nResponse.LoveMult+=0.1\nResponse.LikeMult+=0.1\n[Perk:0x28F5CB~Skyblivion.esm]\nResponse.LoveMult+=0.1\nResponse.LikeMult+=0.1\n";

		std::string_view Trim(std::string_view a_s)
		{
			constexpr std::string_view ws = " \t\r\n";
			const auto b = a_s.find_first_not_of(ws);
			if (b == std::string_view::npos) {
				return {};
			}
			return a_s.substr(b, a_s.find_last_not_of(ws) - b + 1);
		}

		bool IEquals(std::string_view a, std::string_view b)
		{
			return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
					   return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
				   });
		}

		bool ParseFloat(std::string_view a_text, float& a_out)
		{
			const auto t = Trim(a_text);
			const auto [ptr, err] = std::from_chars(t.data(), t.data() + t.size(), a_out);
			return err == std::errc{} && ptr == t.data() + t.size();
		}

		bool ParseInt(std::string_view a_text, int& a_out)
		{
			const auto t = Trim(a_text);
			const auto [ptr, err] = std::from_chars(t.data(), t.data() + t.size(), a_out);
			return err == std::errc{} && ptr == t.data() + t.size();
		}

		RE::ActorValue ActorValueNamed(const std::string& a_name)
		{
			auto* list = RE::ActorValueList::GetSingleton();
			if (!list) {
				return RE::ActorValue::kNone;
			}
			const auto av = list->LookupActorValueByName(a_name.c_str());
			//The game spells the skill "Speechcraft" but speaks of "Speech" as often as not.
			if (av == RE::ActorValue::kNone && IEquals(a_name, "Speech")) {
				return RE::ActorValue::kSpeech;
			}
			return av;
		}
	}

	bool Perks::ParseHeader(std::string_view a_text, Rule& a_rule, std::size_t a_lineNo)
	{
		a_rule.header = std::string(a_text);
		const auto colon = a_text.find(':');
		if (colon == std::string_view::npos) {
			Log::WARN("persuasion perks: line {}: section [{}] has no kind (Perk:, Mastery:, Skill:) - ignored", a_lineNo, a_text);
			return false;
		}
		const auto kind = Trim(a_text.substr(0, colon));
		const auto rest = Trim(a_text.substr(colon + 1));
		if (rest.empty()) {
			Log::WARN("persuasion perks: line {}: section [{}] names nothing - ignored", a_lineNo, a_text);
			return false;
		}
		if (IEquals(kind, "Perk")) {
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
				a_rule.plugin = std::string(Trim(rest.substr(tilde + 1)));
				return !a_rule.plugin.empty();
			}
			a_rule.kind = Kind::kPerkEditorID;
			a_rule.name = std::string(rest);
			return true;
		}
		if (IEquals(kind, "Mastery")) {
			const auto c2 = rest.find(':');
			int rank = 0;
			if (c2 == std::string_view::npos || !ParseInt(rest.substr(c2 + 1), rank) || rank < 1) {
				Log::WARN("persuasion perks: line {}: [{}] must be Mastery:<Skill>:<rank 1..> - ignored", a_lineNo, a_text);
				return false;
			}
			a_rule.kind = Kind::kMastery;
			a_rule.name = std::string(Trim(rest.substr(0, c2)));
			a_rule.rank = rank;
			return true;
		}
		if (IEquals(kind, "Skill")) {
			static constexpr std::array<std::string_view, 5> kCmps{ ">=", "<=", ">", "<", "=" };
			for (const auto cmp : kCmps) {
				const auto at = rest.find(cmp);
				if (at == std::string_view::npos) {
					continue;
				}
				float n = 0.0f;
				if (!ParseFloat(rest.substr(at + cmp.size()), n)) {
					break;
				}
				a_rule.kind = Kind::kSkill;
				a_rule.name = std::string(Trim(rest.substr(0, at)));
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

		const std::filesystem::path path{ kIniPath };
		std::error_code ec;
		const auto time = std::filesystem::last_write_time(path, ec);
		std::ifstream file(path);
		std::istringstream builtIn{ std::string(kDefaultRules) };
		const bool fromFile = !ec && file;
		s_seen = fromFile;
		if (fromFile) {
			s_time = time;
		} else {
			Log::INFO("persuasion perks: {} not found - using the built-in rules", kIniPath);
		}
		std::istream& in = fromFile ? static_cast<std::istream&>(file) : builtIn;

		std::string line;
		std::size_t lineNo = 0, ops = 0;
		bool sectionValid = false;
		while (std::getline(in, line)) {
			++lineNo;
			auto s = Trim(line);
			if (s.empty() || s.front() == ';' || s.front() == '#') {
				continue;
			}
			if (s.front() == '[') {
				const auto close = s.find(']');
				const auto header = Trim(s.substr(1, close == std::string_view::npos ? std::string_view::npos : close - 1));
				Rule rule;
				sectionValid = ParseHeader(header, rule, lineNo);
				if (sectionValid) {
					s_rules.push_back(std::move(rule));
				}
				continue;
			}
			if (!sectionValid) {
				continue;
			}
			//Section.Key<op>value, op one of "=", "*=", "+=".
			const auto eq = s.find('=');
			if (eq == std::string_view::npos || eq == 0) {
				Log::WARN("persuasion perks: line {} is not Section.Key=value: '{}'", lineNo, s);
				continue;
			}
			char op = '=';
			std::size_t keyEnd = eq;
			if (s[eq - 1] == '*' || s[eq - 1] == '+') {
				op = s[eq - 1];
				keyEnd = eq - 1;
			}
			const auto target = Trim(s.substr(0, keyEnd));
			const auto dot = target.find('.');
			if (dot == std::string_view::npos) {
				Log::WARN("persuasion perks: line {}: '{}' must be Section.Key", lineNo, target);
				continue;
			}
			auto value = s.substr(eq + 1);
			if (const auto c = value.find_first_of(";#"); c != std::string_view::npos) {
				value = value.substr(0, c);
			}
			float v = 0.0f;
			if (!ParseFloat(value, v)) {
				Log::WARN("persuasion perks: line {}: '{}' is not a number", lineNo, Trim(value));
				continue;
			}
			const int idx = Settings::Find(target.substr(0, dot), target.substr(dot + 1));
			if (idx < 0) {
				Log::WARN("persuasion perks: line {}: unknown setting {} - ignored", lineNo, target);
				continue;
			}
			s_rules.back().ops.push_back(Op{ static_cast<std::size_t>(idx), op, v });
			++ops;
		}
		Log::INFO("persuasion perks: {} read: {} section(s), {} change(s)", fromFile ? kIniPath : "built-in rules", s_rules.size(), ops);
	}

	void Perks::ReloadIfChanged()
	{
		std::error_code ec;
		const auto time = std::filesystem::last_write_time(std::filesystem::path{ kIniPath }, ec);
		const bool present = !ec;
		if (present == s_seen && (!present || time == s_time)) {
			return;
		}
		Log::INFO("persuasion perks: {} changed on disk - reloading", kIniPath);
		Load();
	}

	bool Perks::Holds(const Rule& a_rule, RE::PlayerCharacter* a_player)
	{
		switch (a_rule.kind) {
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
		std::size_t held = 0;
		for (const auto& rule : s_rules) {
			if (!Holds(rule, player)) {
				continue;
			}
			++held;
			for (const auto& op : rule.ops) {
				float& slot = a_values[op.index];
				const float before = slot;
				switch (op.op) {
				case '*': slot *= op.value; break;
				case '+': slot += op.value; break;
				default:  slot = op.value; break;
				}
				const auto& def = Settings::Def(op.index);
				Log::INFO("persuasion perks: [{}]: [{}] {} {}= {} -> {} (was {})", rule.header, def.section, def.key,
					op.op == '=' ? "" : std::string(1, op.op), op.value, slot, before);
			}
		}
		return held;
	}
}
