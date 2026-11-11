#include "Actors/ActorExpression.h"

#include <utility>

namespace {
	//The dialogue phonemes and their blended result, zeroed: the mouth closes. An expression
	//override freezes the face's dialogue channels at whatever they held (the mouth of a line
	//the pick interrupted stays open under the wheel; the team's WatchForFrozenMouth saw it),
	//so every override, and its clearing, starts from a closed mouth, which is also what
	//SKSE's own SetExpressionOverride / ClearExpressionOverride do (they Reset the face first).
	void ZeroPhonemes(RE::BSFaceGenAnimationData& faceGen) {
		faceGen.Reset(0.0f, false, true, false, false);
		faceGen.phoneme1.Reset();
		faceGen.phoneme3.Reset();
	}

	RE::BSFaceGenAnimationData* FaceGenOrWarn(RE::Actor* actor) {
		RE::BSFaceGenAnimationData* faceGen = actor ? actor->GetFaceGenAnimationData() : nullptr;
		if (!faceGen) {
			Log::WARN("Persuasion expression: face animation data unavailable for actor {:08X}",
				actor ? actor->formID : 0u);
		}
		return faceGen;
	}
}

void ActorExpression::SetNow(RE::Actor* actor, Expression expression, float value) {
	RE::BSFaceGenAnimationData* faceGen = FaceGenOrWarn(actor);
	if (!faceGen) {
		return;
	}
	RE::BSSpinLockGuard guard{ faceGen->lock };
	//Calling both expression3.SetValue and SetExpressionOverride seemed necessary to get visible facial reactions.
	faceGen->expression3.SetValue(std::to_underlying(Expression::Anger), 0.0f);
	faceGen->expression3.SetValue(std::to_underlying(Expression::Happy), 0.0f);
	faceGen->expression3.SetValue(std::to_underlying(Expression::MoodAnger), 0.0f);
	faceGen->expression3.SetValue(std::to_underlying(Expression::MoodHappy), 0.0f);
	if (value <= 0.0f) {
		faceGen->ClearExpressionOverride();
		ZeroPhonemes(*faceGen);
	}
	else {
		faceGen->SetExpressionOverride(std::to_underlying(expression), value);
		faceGen->exprOverride = true;
		ZeroPhonemes(*faceGen);
		const float normalized = value / 100.0f;
		faceGen->expression3.SetValue(std::to_underlying(expression), normalized);
		//Add even more expression with moods:
		if (expression == Expression::Happy) {
			faceGen->expression3.SetValue(std::to_underlying(Expression::MoodHappy), normalized);
		}
		else if (expression == Expression::Anger) {
			faceGen->expression3.SetValue(std::to_underlying(Expression::MoodAnger), normalized);
		}
	}
}

void ActorExpression::CloseMouthNow(RE::Actor* actor) {
	RE::BSFaceGenAnimationData* faceGen = FaceGenOrWarn(actor);
	if (!faceGen) {
		return;
	}
	RE::BSSpinLockGuard guard{ faceGen->lock };
	ZeroPhonemes(*faceGen);
}

//Resets expression then sets a new expression.
void ActorExpression::Set(RE::Actor* actor, Expression expression, float value) {
	if (!actor) {
		return;
	}
	auto* taskInterface = SKSE::GetTaskInterface();

	assert(taskInterface);

	RE::ActorHandle handle = actor->GetHandle();

	//Run on main game thread:
	taskInterface->AddTask([handle, expression, value]() {
		RE::ActorPtr actor = REBridge::Lookup(handle);
		if (!actor) {
			return;
		}
		SetNow(actor.get(), expression, value);
		});
}

void ActorExpression::Reset(RE::Actor* actor) {
	Set(actor, Expression::Anger, 0.0f);
}
