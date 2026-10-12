#include "Degradation/Breakage.h"

#include "Degradation/Condition.h"
#include "Degradation/Settings.h"
#include "Platform/MainThread.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

// <Windows.h>, which this plugin's CommonLib build includes, maps PlaySound to PlaySoundA.
#ifdef PlaySound
#	undef PlaySound
#endif

namespace SKYBDeg::Breakage
{
	namespace
	{
		constexpr std::string_view kSuffix = " (Broken)";
		constexpr const char*      kBreakSound = "UILockpickingPickBreak";

		bool EndsWithSuffix(std::string_view a_name)
		{
			return a_name.size() > kSuffix.size() && a_name.substr(a_name.size() - kSuffix.size()) == kSuffix;
		}

		std::string NameOf(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list)
		{
			const char* name = a_list ? a_list->GetDisplayName(a_obj) : nullptr;
			return name && *name ? name : (a_obj->GetName() ? a_obj->GetName() : "");
		}

		void Mark(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list)
		{
			if (IsMarked(a_list)) {
				return;
			}
			const std::string name = NameOf(a_obj, a_list) + std::string(kSuffix);
			if (auto text = a_list->GetByType<RE::ExtraTextDisplayData>()) {
				text->SetName(name.c_str());
			} else {
				a_list->Add(new RE::ExtraTextDisplayData(name.c_str()));
			}
		}

		void Unmark(RE::ExtraDataList* a_list)
		{
			auto text = a_list ? a_list->GetByType<RE::ExtraTextDisplayData>() : nullptr;
			if (!text || !text->displayName.c_str()) {
				return;
			}
			std::string name = text->displayName.c_str();
			if (!EndsWithSuffix(name)) {
				return;
			}
			name.resize(name.size() - kSuffix.size());
			text->SetName(name.c_str());
		}

		// next frame, on the main thread: the hit / equip event that asked is still running
		void UnequipLater(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list)
		{
			MainThread::Post([a_obj, a_list] {
				auto player = RE::PlayerCharacter::GetSingleton();
				auto manager = RE::ActorEquipManager::GetSingleton();
				if (!player || !manager || !Condition::ListIsLive(a_obj, a_list)) {
					return;
				}
				if (!a_list->HasType<RE::ExtraWorn>() && !a_list->HasType<RE::ExtraWornLeft>()) {
					return;
				}
				manager->UnequipObject(player, a_obj, a_list, 1, nullptr, false, false, false);
			});
		}

		void Break(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list, float a_q, float a_chance)
		{
			const std::string before = NameOf(a_obj, a_list);
			Condition::SetHealth(a_list, std::min(a_q, Settings::Get(G::WeapBrokenCondition)));
			Mark(a_obj, a_list);
			Log::INFO("breakage: '{}' broke at {:.1f}% (chance {:.2f}%)", before, a_q * 100.0f, a_chance * 100.0f);
			RE::PlaySound(kBreakSound);
			RE::SendHUDMessage::ShowHUDMessage(std::format("{} has broken!", before).c_str());
			UnequipLater(a_obj, a_list);
		}
	}

	bool IsMarked(const RE::ExtraDataList* a_list)
	{
		auto text = a_list ? a_list->GetByType<RE::ExtraTextDisplayData>() : nullptr;
		return text && text->displayName.c_str() && EndsWithSuffix(text->displayName.c_str());
	}

	void OnUse(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list, bool a_power, bool a_block)
	{
		if (!a_obj || !a_list || !Settings::On(G::WeapBreakEnabled) || IsMarked(a_list)) {
			return;
		}
		const float q = Condition::GetHealth(a_list);
		const float threshold = Settings::Get(G::WeapBreakThreshold);
		if (q >= threshold) {
			return;
		}
		// 0 just below the threshold, 1 at the wear floor
		const float floor = Settings::Get(G::QualityMin);
		const float span = std::max(threshold - floor, 0.001f);
		const float depth = std::clamp((threshold - q) / span, 0.0f, 1.0f);
		const float top = Settings::Get(G::WeapBreakChanceTop);
		const float bottom = Settings::Get(G::WeapBreakChanceBottom);
		float chance = (top + (bottom - top) * depth) * Settings::Get(G::WeapBreakChanceMult);
		if (a_power) {
			chance *= Settings::Get(G::WeapBreakPowerMult);
		}
		if (a_block) {
			chance *= Settings::Get(G::WeapBreakBlockMult);
		}
		chance = std::clamp(chance, 0.0f, 1.0f);
		if (chance > 0.0f && Condition::Roll() < chance) {
			Break(a_obj, a_list, q, chance);
		}
	}

	void OnConditionSet(RE::ExtraDataList* a_list, float a_value)
	{
		if (a_value >= Settings::Get(G::WeapUsableAt) && IsMarked(a_list)) {
			Unmark(a_list);
			Log::INFO("breakage: a weapon repaired to {:.1f}% is usable again", a_value * 100.0f);
		}
	}

	void EquipGuard::Register()
	{
		if (auto holder = RE::ScriptEventSourceHolder::GetSingleton()) {
			holder->AddEventSink<RE::TESEquipEvent>(this);
			Log::INFO("breakage: watching equips");
		}
	}

	RE::BSEventNotifyControl EquipGuard::ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*)
	{
		if (!a_event || !a_event->equipped) {
			return RE::BSEventNotifyControl::kContinue;
		}
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player || a_event->actor.get() != player) {
			return RE::BSEventNotifyControl::kContinue;
		}
		auto form = RE::TESForm::LookupByID(a_event->baseObject);
		if (Condition::Classify(form) != Condition::Kind::kWeapon) {
			return RE::BSEventNotifyControl::kContinue;
		}
		auto obj = form->As<RE::TESBoundObject>();
		for (bool left : { false, true }) {
			auto list = Condition::FindWornList(obj, left);
			if (!list || !IsMarked(list)) {
				continue;
			}
			const float usable = Settings::Get(G::WeapUsableAt);
			if (Condition::GetHealth(list) >= usable) {
				Unmark(list);  // UsableAt was lowered since it broke
				continue;
			}
			RE::SendHUDMessage::ShowHUDMessage(
				std::format("{} is broken. Repair it to {:.0f}% to use it.", NameOf(obj, list), usable * 100.0f).c_str());
			UnequipLater(obj, list);
		}
		return RE::BSEventNotifyControl::kContinue;
	}
}
