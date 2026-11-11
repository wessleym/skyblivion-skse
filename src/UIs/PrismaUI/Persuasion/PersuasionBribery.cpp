#include "PersuasionBribery.h"

#include "Disposition/DispositionSystem.h"
#include "Resources.h"

#include <algorithm>

namespace Persuasion
{
	namespace
	{
		struct Priced
		{
			BribeAttempt out;
			BribeEconomy::Params params;
			float penalty{ 1.0f };
			float playerSpeech{ 0 }, npcSpeech{ 0 };
			int playerLevel{ 0 }, npcLevel{ 0 };
		};

		// price and gain at a_disposition under the conversation's rules; out.reason kNone when a bribe can buy something
		Priced PriceOf(RE::Actor* a_actor, float a_disposition, const Effective& a_eff)
		{
			Priced r;
			r.out.disposition = a_disposition;
			if (!a_eff.On(P::BribeEnabled)) {
				r.out.reason = BribeAttempt::Reason::kDisabled;
				return r;
			}
			//The world runs under the wheel, so the NPC can be killed while it is open: no gold changes hands then.
			if (!a_actor || a_actor->IsDead()) {
				Log::INFO("Bribery: the persuasion target is dead or gone; no bribe.");
				r.out.reason = BribeAttempt::Reason::kError;
				return r;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* avNpc = REBridge::AVOwner(a_actor);
			auto* avPlayer = player ? REBridge::AVOwner(player) : nullptr;
			if (!avNpc || !avPlayer) {
				Log::WARN("Bribery: ActorValueOwner was null.");
				r.out.reason = BribeAttempt::Reason::kError;
				return r;
			}
			r.playerSpeech = avPlayer->GetActorValue(RE::ActorValue::kSpeech);
			r.npcSpeech = avNpc->GetActorValue(RE::ActorValue::kSpeech);
			r.playerLevel = static_cast<int>(player->GetLevel());
			r.npcLevel = static_cast<int>(a_actor->GetLevel());
			r.params = Rules::BribeParams(a_eff);
			BribeEconomy::Outcome outcome = BribeEconomy::Calculate(r.params, a_disposition, r.playerSpeech, r.npcSpeech, r.playerLevel, r.npcLevel);
			//[Penalty] OnBribe: the wheel's disposition penalty shrinks what gold buys too.
			if (a_eff.On(P::PenaltyOnBribe)) {
				r.penalty = Rules::PenaltyMultiplier(a_eff, a_disposition);
				outcome.gain = std::max(r.params.minGain, static_cast<int>(outcome.exactGain * r.penalty + 0.5f));  //rounded once
			}
			r.out.price = outcome.price;
			r.out.gain = outcome.gain;
			//No bribe when it would raise disposition by less than a point: the exact gain (before
			//MinGain), the penalty and the room left under the maximum all count.
			const float room = r.params.maxDisposition - a_disposition;
			if (std::min(outcome.exactGain * r.penalty, room) < 1.0f) {
				r.out.reason = BribeAttempt::Reason::kTooSmall;
			}
			return r;
		}
	}

	BribeAttempt Bribery::Quote(RE::Actor* a_actor, float a_disposition, const Effective& a_eff)
	{
		BribeAttempt out = PriceOf(a_actor, a_disposition, a_eff).out;
		if (out.reason == BribeAttempt::Reason::kNone && Resources::PlayerGold() < out.price) {
			out.reason = BribeAttempt::Reason::kNoGold;
		}
		return out;
	}

	BribeAttempt Bribery::Attempt(RE::Actor* a_actor, float a_disposition, const Effective& a_eff)
	{
		const Priced r = PriceOf(a_actor, a_disposition, a_eff);
		BribeAttempt out = r.out;
		out.gain = 0;
		if (out.reason != BribeAttempt::Reason::kNone) {
			return out;
		}
		if (Resources::PlayerGold() < out.price) {
			out.reason = BribeAttempt::Reason::kNoGold;
			return out;
		}
		RE::PlayerCharacter::GetSingleton()->RemoveItem(Resources::gold, out.price, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

		out.gain = r.out.gain;
		//A whole number: both hosts send it as one, and the actor gets the same value (else a wheel at 57.6
		//would show 58 after a +1 bribe, and its write-back on close would drop the fraction).
		out.disposition = std::min(r.params.maxDisposition, std::round(a_disposition + static_cast<float>(out.gain)));
		out.success = true;
		//Both hosts call this from an SKSE task. The disposition is written by a further task, so it lands after
		//any disposition write the host queued while this task was waiting (e.g. the pre-bribe value sent on
		//close) and is not overwritten by it. Both hosts show the returned value, not a read-back.
		auto* taskInterface = SKSE::GetTaskInterface();
		assert(taskInterface);
		const RE::ActorHandle handle = a_actor->GetHandle();
		const float newDisposition = out.disposition;
		taskInterface->AddTask([handle, newDisposition]() {
			if (RE::ActorPtr actor = REBridge::Lookup(handle)) {
				DispositionSystem::SetDispositionActorValue(actor.get(), newDisposition, true);
			}
		});

		Log::INFO("bribe: price={}, gain={}, disposition={} -> {} (playerLevel={}, npcLevel={}, playerSpeech={}, npcSpeech={}, costMult={}, gainMult={}, penalty={})",
			out.price, out.gain, a_disposition, out.disposition, r.playerLevel, r.npcLevel, r.playerSpeech, r.npcSpeech,
			r.params.costMult, r.params.gainMult, r.penalty);
		return out;
	}
}
