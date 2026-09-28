#include "SigilSystem.h"
#include "SigilContract.h"
#include "SigilEnchanting.h"
#include "SoulGems.h"
#include "Papyrus/ModEvents.h"

#include <algorithm>

namespace Sigil {

	namespace {
		const char* ApplyResultText(SigilEnchanting::Result result) {
			switch (result) {
			case SigilEnchanting::Result::Enchanted:
				return SigilContract::ApplyResult::Enchanted;
			case SigilEnchanting::Result::AlreadyEnchanted:
				return SigilContract::ApplyResult::AlreadyEnchanted;
			default:
				return SigilContract::ApplyResult::Missing;
			}
		}
	}

	void SigilSystem::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("SigilSystem: No mod-event source. Sigil requests will not be heard.");
			return;
		}
		Log::INFO("SigilSystem: Registered.");
	}

	//Payload: "gemsFormListID^tier". Only the tier is used; the list is the psc's own.
	void SigilSystem::ConsumeGem(const std::string& payload) {
		const auto fields = ModEvents::Split(payload, '^');
		bool consumed = false;
		if (fields.size() >= 2) {
			const int tier = ModEvents::ParseInt(fields[1]);
			consumed = SoulGems::ConsumeTierGem(RE::PlayerCharacter::GetSingleton(), tier);
			Log::INFO("SigilSystem: Gem consume (tier {}): {}.", tier, consumed ? "consumed" : "none found");
		}
		else {
			Log::WARN("SigilSystem: Malformed gem consume payload '{}'.", payload);
		}
		ModEvents::Send(SigilContract::Answer::GemConsumed, consumed ? "1" : "0");
	}

	//Payload: "gearID^enchantmentID^charge^customName". The SWF scrubs separators out of the typed name.
	void SigilSystem::ApplyEnchant(const std::string& payload) {
		Log::INFO("SigilSystem: Enchant request '{}'.", payload);
		const auto fields = ModEvents::Split(payload, '^');
		auto result = SigilEnchanting::Result::Missing;
		if (fields.size() >= 4) {
			const auto charge = static_cast<std::uint16_t>(std::clamp(ModEvents::ParseInt(fields[2]), 0, 0xFFFF));
			result = SigilEnchanting::Apply(RE::PlayerCharacter::GetSingleton(),
				ModEvents::ParseFormID(fields[0]), ModEvents::ParseFormID(fields[1]), charge, fields[3]);
		}
		else {
			Log::WARN("SigilSystem: Malformed enchant payload '{}'.", payload);
		}
		ModEvents::Send(SigilContract::Answer::Applied, ApplyResultText(result));
	}

	RE::BSEventNotifyControl SigilSystem::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		//Inventory changes belong on the main thread.
		if (a_event->eventName == SigilContract::ModEvent::ConsumeGem) {
			SKSE::GetTaskInterface()->AddTask([payload = ModEvents::StrArg(*a_event)] { ConsumeGem(payload); });
		}
		else if (a_event->eventName == SigilContract::ModEvent::ApplyEnchant) {
			SKSE::GetTaskInterface()->AddTask([payload = ModEvents::StrArg(*a_event)] { ApplyEnchant(payload); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
