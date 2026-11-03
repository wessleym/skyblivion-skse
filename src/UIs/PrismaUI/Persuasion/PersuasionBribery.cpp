#include "PersuasionBribery.h"

#include "Disposition/DispositionSystem.h"

#include <algorithm>

namespace Persuasion
{
	namespace
	{
		// The player's gold by item count. Not Actor::GetGoldAmount(): that CommonLib helper reads a bad
		// pointer on 1.6.1170, and the quote runs on every point of change.
		int PlayerGold(RE::PlayerCharacter* a_player)
		{
			auto* goldForm = RE::TESForm::LookupByID(0x0000000F);
			auto* gold = goldForm ? goldForm->As<RE::TESBoundObject>() : nullptr;
			return a_player && gold ? std::max(0, a_player->GetItemCount(gold)) : 0;
		}

		struct Priced
		{
			BribeAttempt out;
			BribeEconomy::Params params;
			float penalty{ 1.0f };
			float playerSpeech{ 0 }, npcSpeech{ 0 };
			int playerLevel{ 0 }, npcLevel{ 0 };
			bool ok{ false };
		};

		// price and gain at a_disposition under the conversation's rules
		Priced PriceOf(RE::Actor* a_actor, float a_disposition, const Effective& a_eff)
		{
			Priced r;
			r.out.disposition = a_disposition;
			if (!a_eff.On(P::BribeEnabled)) {
				r.out.reason = BribeAttempt::Reason::kDisabled;
				return r;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* avNpc = a_actor ? REBridge::AVOwner(a_actor) : nullptr;
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
				outcome.gain = std::max(r.params.minGain, static_cast<int>(static_cast<float>(outcome.gain) * r.penalty + 0.5f));
			}
			r.out.price = outcome.price;
			r.out.gain = outcome.gain;
			//No bribe when it would raise disposition by less than a point: the exact gain (before
			//MinGain), the penalty and the room left under the maximum all count.
			const float room = r.params.maxDisposition - a_disposition;
			if (std::min(outcome.exactGain * r.penalty, room) < 1.0f) {
				r.out.reason = BribeAttempt::Reason::kTooSmall;
				return r;
			}
			r.ok = true;
			return r;
		}
	}

	BribeAttempt Bribery::Quote(RE::Actor* a_actor, float a_disposition, const Effective& a_eff)
	{
		Priced r = PriceOf(a_actor, a_disposition, a_eff);
		if (r.ok && PlayerGold(RE::PlayerCharacter::GetSingleton()) < r.out.price) {
			r.out.reason = BribeAttempt::Reason::kNoGold;
		}
		return r.out;
	}

	BribeAttempt Bribery::Attempt(RE::Actor* a_actor, float a_disposition, const Effective& a_eff)
	{
		Priced r = PriceOf(a_actor, a_disposition, a_eff);
		if (!r.ok) {
			r.out.gain = 0;
			return r.out;
		}
		BribeAttempt out = r.out;
		out.gain = 0;
		const auto& params = r.params;
		const float penalty = r.penalty;
		const float playerSpeech = r.playerSpeech, npcSpeech = r.npcSpeech;
		const int playerLevel = r.playerLevel, npcLevel = r.npcLevel;
		auto* player = RE::PlayerCharacter::GetSingleton();
		BribeEconomy::Outcome outcome;
		outcome.price = r.out.price;
		outcome.gain = r.out.gain;
		if (PlayerGold(player) < outcome.price) {
			out.reason = BribeAttempt::Reason::kNoGold;
			return out;
		}
		auto* goldForm = RE::TESForm::LookupByID(0x0000000F);
		auto* gold = goldForm ? goldForm->As<RE::TESBoundObject>() : nullptr;
		if (!gold) {
			Log::ERROR("Bribery::Attempt: Gold001 (0x0000000F) form missing");
			out.reason = BribeAttempt::Reason::kError;
			return out;
		}
		player->RemoveItem(gold, outcome.price, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

		out.gain = outcome.gain;
		out.disposition = std::min(params.maxDisposition, a_disposition + static_cast<float>(outcome.gain));
		out.success = true;
		DispositionSystem::SetDispositionActorValue(a_actor, out.disposition, true);

		Log::INFO("bribe: price={}, gain={}, disposition={} -> {} (playerLevel={}, npcLevel={}, playerSpeech={}, npcSpeech={}, costMult={}, gainMult={}, penalty={})",
			out.price, out.gain, a_disposition, out.disposition, playerLevel, npcLevel, playerSpeech, npcSpeech,
			params.costMult, params.gainMult, penalty);
		return out;
	}
}
