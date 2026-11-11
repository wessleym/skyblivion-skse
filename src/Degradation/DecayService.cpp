#include "Degradation/DecayService.h"

#include "Degradation/Breakage.h"
#include "Degradation/Condition.h"
#include "Degradation/Settings.h"
#include "Platform/Text.h"
#include <algorithm>
#include <format>
#include <string>

namespace SKYBDeg
{
	using Condition::Kind;
	using Flag = RE::TESHitEvent::Flag;

	namespace
	{
		//Whether the player's current attack is the left hand's, for when both hands hold the same weapon.
		//Left-hand attack events end in "LeftHand" ("attackStartLeftHand"); "attackPowerStartLeft" is a right-hand
		//sideways power attack. A dual-wield attack strikes with both, so its hits alternate between the copies.
		bool StruckWithLeft(RE::PlayerCharacter* a_player)
		{
			auto* process = a_player->GetActorRuntimeData().currentProcess;
			auto* high = process ? process->high : nullptr;
			auto* attack = high ? high->attackData.get() : nullptr;
			const std::string event = Text::Lower(attack && attack->event.c_str() ? attack->event.c_str() : "");
			if (event.find("dual") != std::string::npos) {
				static bool s_left = false;  //hit events never run concurrently (measured on AE and VR)
				s_left = !s_left;
				return s_left;
			}
			return event.find("lefthand") != std::string::npos;
		}
	}

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
		const bool inRight = a_player->GetEquippedObject(false) == source;
		const bool inLeft = a_player->GetEquippedObject(true) == source;
		//Both hands holding the same weapon: the hit names only the weapon, so the striking hand comes from the attack.
		if (!inRight && !inLeft) {
			return;  // thrown/unequipped mid-swing: nothing to wear
		}
		const bool left = inLeft && (!inRight || StruckWithLeft(a_player));
		RE::ExtraDataList* list = Condition::FindWornList(weap, left);
		if (!list && inRight && inLeft) {
			list = Condition::FindWornList(weap, !left);
		}
		if (!list) {
			return;  // no worn instance of it found
		}
		const float rate = Settings::Get(G::WeapRate) * Settings::Get(G::WeapRateMult) * EventMult(a_event);
		Degrade(weap, list, rate, "attack");
		Breakage::OnUse(weap, list, a_event.flags.any(Flag::kPowerAttack, Flag::kBashAttack), false);
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
					Breakage::OnUse(obj, list, a_event.flags.any(Flag::kPowerAttack, Flag::kBashAttack), true);
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
						   Condition::WearSkillMult() * Condition::MasteryWearMult(a_obj);
		if (loss <= 0.0f) {
			return q;
		}
		const float nq = std::max(floor, q - loss);
		Condition::SetHealth(a_obj, a_list, nq);
		Log::DEBUG("decay {}: '{}' {:.2f}% -> {:.2f}% (x{:.2f} material, x{:.2f} curve)", a_why, a_obj->GetName(),
					  q * 100.0f, nq * 100.0f, Condition::MaterialMult(a_obj), Condition::CurveMult(q));

		const float warn = Settings::Get(G::WarnAt);
		if (Settings::On(G::Notify) && q >= warn && nq < warn) {
			const char* name = REBridge::DisplayName(a_list, a_obj);
			RE::SendHUDMessage::ShowHUDMessage(std::format("{} is badly worn ({:.0f}%).", name ? name : a_obj->GetName(), nq * 100.0f).c_str());
		} else if (Settings::On(G::Notify) && nq <= floor && q > floor) {
			const char* name = REBridge::DisplayName(a_list, a_obj);
			RE::SendHUDMessage::ShowHUDMessage(std::format("{} can wear no further ({:.0f}%).", name ? name : a_obj->GetName(), nq * 100.0f).c_str());
		}
		return nq;
	}
}
