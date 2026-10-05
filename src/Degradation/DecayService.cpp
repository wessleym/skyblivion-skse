// Moved from SKYBDegradation.dll into Skyblivion.dll, 2026-10-05 (fold-in, UI integration workspace
// ledger L-0058). Source of record until then: game\native\SKYBDegradation\src in that workspace.
#include "Degradation/DecayService.h"

#include "Degradation/Condition.h"
#include "Degradation/Settings.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace SKYBDeg
{
	using Condition::Kind;
	using Flag = RE::TESHitEvent::Flag;

	void DecayService::Register()
	{
		if (auto holder = RE::ScriptEventSourceHolder::GetSingleton()) {
			holder->AddEventSink<RE::TESHitEvent>(this);
			Log::INFO("decay: listening for hit events");
		}
	}

	RE::BSEventNotifyControl DecayService::ProcessEvent(const RE::TESHitEvent* a_event,
														RE::BSTEventSource<RE::TESHitEvent>*)
	{
		if (!a_event || !Settings::On(G::Enabled)) {
			return RE::BSEventNotifyControl::kContinue;
		}
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return RE::BSEventNotifyControl::kContinue;
		}
		auto cause = a_event->cause.get();
		auto target = a_event->target.get();
		if (cause == player && target != player) {
			OnPlayerAttack(*a_event, player);
		} else if (target == player && cause != player) {
			OnPlayerStruck(*a_event, player);
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	float DecayService::EventMult(const RE::TESHitEvent& a_event)
	{
		float mult = 1.0f;
		if (a_event.flags.any(Flag::kPowerAttack)) {
			mult *= Settings::Get(G::PowerAttackMult);
		}
		if (a_event.flags.any(Flag::kBashAttack)) {
			mult *= Settings::Get(G::BashMult);
		}
		if (a_event.flags.any(Flag::kSneakAttack)) {
			mult *= Settings::Get(G::SneakAttackMult);
		}
		return mult;
	}

	void DecayService::OnPlayerAttack(const RE::TESHitEvent& a_event, RE::PlayerCharacter* a_player)
	{
		// Enchantment / poison follow-up events carry the ENCH/ALCH as source,
		// so only the weapon's own hit event passes this test (no double wear).
		auto source = RE::TESForm::LookupByID(a_event.source);
		if (Condition::Classify(source) != Kind::kWeapon) {
			return;
		}
		auto weap = source->As<RE::TESBoundObject>();
		RE::ExtraDataList* list = nullptr;
		if (a_player->GetEquippedObject(false) == source) {
			list = Condition::FindWornList(weap, false);
		}
		if (!list && a_player->GetEquippedObject(true) == source) {
			list = Condition::FindWornList(weap, true);
		}
		if (!list) {
			return;  // thrown/unequipped mid-swing: nothing to wear
		}
		const float rate = Settings::Get(G::WeapRate) * Settings::Get(G::WeapRateMult) * EventMult(a_event);
		Degrade(weap, list, rate, "attack");
	}

	void DecayService::OnPlayerStruck(const RE::TESHitEvent& a_event, RE::PlayerCharacter* a_player)
	{
		auto source = a_event.source ? RE::TESForm::LookupByID(a_event.source) : nullptr;
		const bool weaponHit = !source || source->Is(RE::FormType::Weapon);  // creatures often send none
		if (!weaponHit && !Settings::On(G::ArmorFromSpells)) {
			return;
		}

		if (a_event.flags.any(Flag::kHitBlocked)) {
			// shield first, else whatever blocked with the right hand
			if (auto left = a_player->GetEquippedObject(true); Condition::Classify(left) == Kind::kShield) {
				auto obj = left->As<RE::TESBoundObject>();
				if (auto list = Condition::FindWornList(obj, false)) {  // armor is always ExtraWorn
					const float rate = Settings::Get(G::BlockRate) * Settings::Get(G::ArmorRateMult) * EventMult(a_event);
					Degrade(obj, list, rate, "block");
				}
				return;
			}
			if (auto right = a_player->GetEquippedObject(false); Condition::Classify(right) == Kind::kWeapon) {
				auto obj = right->As<RE::TESBoundObject>();
				if (auto list = Condition::FindWornList(obj, false)) {
					const float rate = Settings::Get(G::BlockRate) * Settings::Get(G::WeapRateMult) * EventMult(a_event);
					Degrade(obj, list, rate, "block");
				}
			}
			return;
		}

		auto pieces = Condition::WornArmor();
		std::erase_if(pieces, [](const Condition::WornPiece& a_piece) { return a_piece.kind != Kind::kArmor; });
		if (pieces.empty()) {
			return;
		}
		float scale = 1.0f;
		const float ref = Settings::Get(G::ArmorDamageRef);
		if (ref > 0.0f && source) {
			if (auto weap = source->As<RE::TESObjectWEAP>()) {
				scale = std::clamp(static_cast<float>(weap->GetAttackDamage()) / ref,
								   Settings::Get(G::ArmorDamageScaleMin), Settings::Get(G::ArmorDamageScaleMax));
			}
		}
		const float rate = Settings::Get(G::ArmorRate) * Settings::Get(G::ArmorRateMult) * EventMult(a_event) * scale;
		std::shuffle(pieces.begin(), pieces.end(), Condition::Rng());
		const auto n = std::min<std::size_t>(pieces.size(),
											 static_cast<std::size_t>(std::max(0.0f, Settings::Get(G::ArmorPiecesPerHit))));
		for (std::size_t i = 0; i < n; ++i) {
			Degrade(pieces[i].obj, pieces[i].list, rate, "struck");
		}
	}

	float DecayService::Degrade(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_list, float a_rate, const char* a_why)
	{
		const float q = Condition::GetHealth(a_list);
		const float floor = Settings::Get(G::QualityMin);
		if (!a_list || q <= floor || a_rate <= 0.0f) {
			return q;
		}
		const float loss = a_rate * Settings::Get(G::RateMult) * Condition::MaterialMult(a_obj) * Condition::CurveMult(q) *
						   Condition::WearSkillMult();
		if (loss <= 0.0f) {
			return q;
		}
		const float nq = std::max(floor, q - loss);
		Condition::SetHealth(a_list, nq);
		Log::DEBUG("decay {}: '{}' {:.2f}% -> {:.2f}% (x{:.2f} material, x{:.2f} curve)", a_why, a_obj->GetName(),
					  q * 100.0f, nq * 100.0f, Condition::MaterialMult(a_obj), Condition::CurveMult(q));

		const float warn = Settings::Get(G::WarnAt);
		if (Settings::On(G::Notify) && q >= warn && nq < warn) {
			const char* name = a_list->GetDisplayName(a_obj);
			RE::SendHUDMessage::ShowHUDMessage(std::format("{} is badly worn ({:.0f}%).", name ? name : a_obj->GetName(), nq * 100.0f).c_str());
		} else if (Settings::On(G::Notify) && nq <= floor && q > floor) {
			const char* name = a_list->GetDisplayName(a_obj);
			RE::SendHUDMessage::ShowHUDMessage(std::format("{} can wear no further ({:.0f}%).", name ? name : a_obj->GetName(), nq * 100.0f).c_str());
		}
		return nq;
	}
}
