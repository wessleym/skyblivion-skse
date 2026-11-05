#include "ScrollCastXP.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace ScrollCraft {

	namespace {
		std::string Trim(std::string a_text)
		{
			const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
			a_text.erase(a_text.begin(), std::find_if(a_text.begin(), a_text.end(), notSpace));
			a_text.erase(std::find_if(a_text.rbegin(), a_text.rend(), notSpace).base(), a_text.end());
			return a_text;
		}

		//1 + the highest Minimum Skill Level of the item's effects / 25, clamped 1..5 (Novice 0 .. Master 100)
		int Tier(const RE::MagicItem* a_item)
		{
			std::int32_t highest = 0;
			for (const auto* effect : a_item->effects) {
				if (effect && effect->baseEffect) {
					highest = std::max(highest, effect->baseEffect->GetMinimumSkillLevel());
				}
			}
			return std::clamp(1 + static_cast<int>(highest) / 25, 1, 5);
		}

		//the staff in either hand whose enchantment this is, or null
		RE::TESObjectWEAP* StaffWith(RE::PlayerCharacter* a_player, const RE::EnchantmentItem* a_enchantment)
		{
			for (const bool left : { false, true }) {
				auto* form = a_player->GetEquippedObject(left);
				auto* weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr;
				if (weapon && weapon->IsStaff() && weapon->formEnchanting == a_enchantment) {
					return weapon;
				}
			}
			return nullptr;
		}
	}

	void ScrollCastXP::Reload()
	{
		bool enabled = true;
		float scrollScale = 0.02f;
		float staffScale = 0.02f;
		bool log = true;
		std::ifstream file(IniFile);
		if (!file) {
			Log::WARN("ScrollCastXP: {} not found; defaults (Enabled 1, ScrollScale 0.02, StaffScale 0.02)", IniFile);
		} else {
			std::string line;
			std::string section;
			while (std::getline(file, line)) {
				line = Trim(line);
				if (line.empty() || line[0] == ';' || line[0] == '#') {
					continue;
				}
				if (line.front() == '[' && line.back() == ']') {
					section = line.substr(1, line.size() - 2);
					continue;
				}
				const auto eq = line.find('=');
				if (section != "CastingXP" || eq == std::string::npos) {
					continue;
				}
				const auto key = Trim(line.substr(0, eq));
				auto value = Trim(line.substr(eq + 1));
				const auto comment = value.find(';');
				if (comment != std::string::npos) {
					value = Trim(value.substr(0, comment));
				}
				try {
					if (key == "Enabled") {
						enabled = std::stof(value) != 0.0f;
					} else if (key == "ScrollScale") {
						scrollScale = std::clamp(std::stof(value), 0.0f, 10.0f);
					} else if (key == "StaffScale") {
						staffScale = std::clamp(std::stof(value), 0.0f, 10.0f);
					} else if (key == "LogCasts") {
						log = std::stof(value) != 0.0f;
					}
				} catch (...) {
					Log::WARN("ScrollCastXP: {}: '{}' is not a number for {}; kept the default", IniFile, value, key);
				}
			}
		}
		m_enabled = enabled;
		m_scrollScale = scrollScale;
		m_staffScale = staffScale;
		m_log = log;
		Log::INFO("ScrollCastXP: Enabled {}, ScrollScale {:.3f}, StaffScale {:.3f}, LogCasts {}", m_enabled ? 1 : 0,
				  m_scrollScale, m_staffScale, m_log ? 1 : 0);
	}

	void ScrollCastXP::Register()
	{
		Reload();
		if (m_registered) {
			return;
		}
		if (auto* events = RE::ScriptEventSourceHolder::GetSingleton()) {
			events->AddEventSink<RE::TESSpellCastEvent>(this);
			m_registered = true;
			Log::INFO("ScrollCastXP: Registered.");
		} else {
			Log::WARN("ScrollCastXP: no script event source; scrolls and staves will not train Enchanting.");
		}
	}

	RE::BSEventNotifyControl ScrollCastXP::ProcessEvent(const RE::TESSpellCastEvent* a_event,
														RE::BSTEventSource<RE::TESSpellCastEvent>*)
	{
		if (!m_enabled || !a_event || !a_event->object) {
			return RE::BSEventNotifyControl::kContinue;
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || a_event->object.get() != player) {
			return RE::BSEventNotifyControl::kContinue;
		}
		auto* form = RE::TESForm::LookupByID(a_event->spell);
		if (!form) {
			return RE::BSEventNotifyControl::kContinue;
		}
		const char* kind = nullptr;
		const char* name = nullptr;
		float scale = 0.0f;
		int tier = 0;
		if (auto* scroll = form->As<RE::ScrollItem>()) {
			kind = "scroll";
			name = scroll->GetName();
			scale = m_scrollScale;
			tier = Tier(scroll);
		} else if (auto* enchantment = form->As<RE::EnchantmentItem>()) {
			auto* staff = StaffWith(player, enchantment);
			if (!staff) {
				return RE::BSEventNotifyControl::kContinue;  //not a staff (a weapon's on-hit enchantment, say)
			}
			kind = "staff";
			name = staff->GetName();
			scale = m_staffScale;
			tier = Tier(enchantment);
		} else {
			return RE::BSEventNotifyControl::kContinue;
		}
		if (scale <= 0.0f) {
			return RE::BSEventNotifyControl::kContinue;
		}
		const float uses = scale * static_cast<float>(tier);
		player->AddSkillExperience(RE::ActorValue::kEnchanting, uses);
		if (m_log) {
			Log::INFO("ScrollCastXP: {} '{}' cast: Enchanting +{:.3f} use(s) (tier {} x {:.3f})", kind, name ? name : "", uses,
					  tier, scale);
		}
		return RE::BSEventNotifyControl::kContinue;
	}
}
