#include "Actors/ActorExpression.h"

#include <utility>

//Resets expression then sets a new expression.
void ActorExpression::Set(RE::Actor* actor, Expression expression, float value) {
	if (!actor) {
		return;
	}
	auto* taskInterface = SKSE::GetTaskInterface();
	if (!taskInterface) {
		Log::WARN("Persuasion expression: SKSE TaskInterface unavailable");
		return;
	}
	const RE::FormID formID = actor->GetFormID();
	//Run on main game thread:
	taskInterface->AddTask([formID, expression, value]() {
		auto* a = RE::TESForm::LookupByID<RE::Actor>(formID);
		if (!a) {
			return;
		}
		auto* faceGen = a->GetFaceGenAnimationData();
		if (!faceGen) {
			Log::WARN("Persuasion expression: face animation data unavailable for actor {:08X}",
				formID);
			return;
		}
		//Calling both expression3.SetValue and SetExpressionOverride seemed necessary to get visible facial reactions.
		faceGen->expression3.SetValue(std::to_underlying(Expression::Anger), 0.0f);
		faceGen->expression3.SetValue(std::to_underlying(Expression::Happy), 0.0f);
		faceGen->expression3.SetValue(std::to_underlying(Expression::MoodAnger), 0.0f);
		faceGen->expression3.SetValue(std::to_underlying(Expression::MoodHappy), 0.0f);
		if (value <= 0.0f) {
			faceGen->ClearExpressionOverride();
		}
		else {
			faceGen->SetExpressionOverride(std::to_underlying(expression), value);
			faceGen->exprOverride = true;
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
		});
}

void ActorExpression::Reset(RE::Actor* actor) {
	Set(actor, Expression::Anger, 0.0f);
}
