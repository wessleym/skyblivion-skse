#include "PersuasionRules.h"
#include "PersuasionPerks.h"

#include <algorithm>
#include <cmath>

namespace Persuasion
{
	Effective Rules::Resolve()
	{
		Settings::ReloadIfChanged();

		Effective eff;
		eff.v = Settings::All();
		const auto held = Perks::Apply(eff.v);

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* owner = player ? REBridge::AVOwner(player) : nullptr;
		eff.playerSpeech = owner ? owner->GetActorValue(RE::ActorValue::kSpeech) : 0.0f;

		Log::INFO("persuasion rules: {} perk rule(s) hold; Speechcraft {}; factors L {} / l {} / d {} / H {}; gain x{} loss x{}; decay {} ({}/s, {}/turn); penalty {} (strength {}, curve {}, relief {}); bribe cost x{} gain x{}",
			held, eff.playerSpeech,
			eff.Get(P::LoveFactor) * eff.Get(P::LoveMult), eff.Get(P::LikeFactor) * eff.Get(P::LikeMult),
			eff.Get(P::DislikeFactor) * eff.Get(P::DislikeMult), eff.Get(P::HateFactor) * eff.Get(P::HateMult),
			eff.Get(P::GainMult), eff.Get(P::LossMult),
			eff.On(P::DecayWhileOpen) ? "on" : "off", eff.Get(P::DecayPerSecond) * eff.Get(P::DecayRateMult),
			eff.Get(P::DecayPerRotation) * eff.Get(P::DecayRateMult),
			eff.On(P::PenaltyEnabled) ? "on" : "off", eff.Get(P::PenaltyStrength), eff.Get(P::PenaltyCurve),
			eff.Get(P::PenaltySpeechRelief), eff.Get(P::BribeCostMult), eff.Get(P::BribeGainMult));
		return eff;
	}

	float Rules::PenaltyMultiplier(const Effective& a_eff, float a_disposition)
	{
		if (!a_eff.On(P::PenaltyEnabled)) {
			return 1.0f;
		}
		const float start = a_eff.Get(P::PenaltyStart);
		const float max = a_eff.Get(P::MaxDisposition);
		const float span = max - start;
		const float x = span > 0.0f ? std::clamp((a_disposition - start) / span, 0.0f, 1.0f) : 1.0f;
		const float curve = std::max(0.01f, a_eff.Get(P::PenaltyCurve));
		const float relief = std::clamp(a_eff.playerSpeech / 100.0f, 0.0f, 1.0f) * a_eff.Get(P::PenaltySpeechRelief);
		const float penalty = a_eff.Get(P::PenaltyStrength) * std::pow(x, curve) * (1.0f - std::clamp(relief, 0.0f, 1.0f));
		return std::clamp(1.0f - penalty, std::clamp(a_eff.Get(P::PenaltyFloor), 0.0f, 1.0f), 1.0f);
	}

	nlohmann::json Rules::ToJson(const Effective& a_eff)
	{
		const auto response = [&](P factor, P mult, P add) {
			return nlohmann::json{ { "factor", a_eff.Get(factor) * a_eff.Get(mult) }, { "add", a_eff.Get(add) } };
		};
		const auto approach = [&](P mult, P add) {
			return nlohmann::json{ { "mult", a_eff.Get(mult) }, { "add", a_eff.Get(add) } };
		};
		const float rate = a_eff.Get(P::DecayRateMult);
		const float relief = std::clamp(a_eff.playerSpeech / 100.0f, 0.0f, 1.0f) * a_eff.Get(P::PenaltySpeechRelief);
		return nlohmann::json{
			{ "responses", {
				{ "Love", response(P::LoveFactor, P::LoveMult, P::LoveAdd) },
				{ "Like", response(P::LikeFactor, P::LikeMult, P::LikeAdd) },
				{ "Dislike", response(P::DislikeFactor, P::DislikeMult, P::DislikeAdd) },
				{ "Hate", response(P::HateFactor, P::HateMult, P::HateAdd) } } },
			{ "approaches", {
				{ "Admire", approach(P::AdmireMult, P::AdmireAdd) },
				{ "Boast", approach(P::BoastMult, P::BoastAdd) },
				{ "Joke", approach(P::JokeMult, P::JokeAdd) },
				{ "Coerce", approach(P::CoerceMult, P::CoerceAdd) } } },
			{ "gainMult", a_eff.Get(P::GainMult) },
			{ "lossMult", a_eff.Get(P::LossMult) },
			{ "maxDisposition", a_eff.Get(P::MaxDisposition) },
			{ "closeOnMax", a_eff.On(P::CloseOnMax) },
			{ "maxRounds", static_cast<int>(std::max(0.0f, a_eff.Get(P::MaxRounds))) },
			{ "decay", {
				{ "whileOpen", a_eff.On(P::DecayWhileOpen) },
				{ "perSecond", std::max(0.0f, a_eff.Get(P::DecayPerSecond) * rate) },
				{ "perRotation", std::max(0.0f, a_eff.Get(P::DecayPerRotation) * rate) },
				{ "startDelay", std::max(0.0f, a_eff.Get(P::DecayStartDelay)) },
				{ "floor", a_eff.Get(P::DecayFloor) } } },
			{ "penalty", {
				{ "enabled", a_eff.On(P::PenaltyEnabled) },
				{ "start", a_eff.Get(P::PenaltyStart) },
				{ "strength", a_eff.Get(P::PenaltyStrength) },
				{ "curve", std::max(0.01f, a_eff.Get(P::PenaltyCurve)) },
				{ "relief", std::clamp(relief, 0.0f, 1.0f) },
				{ "floor", std::clamp(a_eff.Get(P::PenaltyFloor), 0.0f, 1.0f) } } },
			{ "bribeEnabled", a_eff.On(P::BribeEnabled) }
		};
	}

	BribeEconomy::Params Rules::BribeParams(const Effective& a_eff)
	{
		BribeEconomy::Params p;
		p.priceCurve = a_eff.Get(P::BribePriceCurve);
		p.gainCurve = a_eff.Get(P::BribeGainCurve);
		p.npcLevelMult = a_eff.Get(P::BribeNpcLevelMult);
		p.scale = a_eff.Get(P::BribeScale);
		p.speechcraftMult = a_eff.Get(P::BribeSpeechcraftMult);
		p.priceScale = a_eff.Get(P::BribePriceScale);
		p.gainScale = a_eff.Get(P::BribeGainScale);
		p.dispositionPriceDivisor = a_eff.Get(P::BribeDispositionPriceDivisor);
		p.costMult = a_eff.Get(P::BribeCostMult);
		p.gainMult = a_eff.Get(P::BribeGainMult);
		p.minPrice = static_cast<int>(std::max(0.0f, a_eff.Get(P::BribeMinPrice)));
		p.minGain = static_cast<int>(std::max(0.0f, a_eff.Get(P::BribeMinGain)));
		p.maxDisposition = a_eff.Get(P::MaxDisposition);
		return p;
	}
}
