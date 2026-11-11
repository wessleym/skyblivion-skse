#include "ScrollCastXP.h"
#include "Platform/IniFile.h"
#include "Platform/Text.h"

#include <algorithm>
#include <string>

namespace ScrollCraft {

	namespace {
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
		const bool read = IniFile::Read(kIniPath, "ScrollCastXP", [&](const IniFile::Entry& a_entry) {
			if (a_entry.section != "CastingXP") {
				return;
			}
			float number = 0.0f;
			if (!Text::Parse(a_entry.value, number)) {
				Log::WARN("ScrollCastXP: {}: '{}' is not a number for {}; kept the default", kIniPath, a_entry.value, a_entry.key);
			} else if (a_entry.key == "Enabled") {
				enabled = number != 0.0f;
			} else if (a_entry.key == "ScrollScale") {
				scrollScale = std::clamp(number, 0.0f, 10.0f);
			} else if (a_entry.key == "StaffScale") {
				staffScale = std::clamp(number, 0.0f, 10.0f);
			} else if (a_entry.key == "LogCasts") {
				log = number != 0.0f;
			}
		});
		if (!read) {
			Log::WARN("ScrollCastXP: {} not found; defaults (Enabled 1, ScrollScale 0.02, StaffScale 0.02)", kIniPath);
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
