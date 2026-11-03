#include "PersuasionView.h"
#include "BribeEconomy.h"
#include "PersuasionContract.h"
#include "DialogueSession.h"
#include "PersuasionBribery.h"
#include "PersuasionPreferences.h"
#include "PersuasionRules.h"
#include "Disposition/DispositionSystem.h"
#include "Actors/ActorExpression.h"

#include "../PrismaViewHandle.h"

#include <nlohmann/json.hpp>
#include <string>

namespace Persuasion {

	RE::FormID PersuasionView::s_currentTargetFormID = 0;

	namespace {
		//The rules of the conversation in progress: settings, the settings menu's overrides and the
		//player's perk and skill modifiers, resolved when the wheel opens.
		Effective s_effective{};

	}

	void PersuasionView::Initialize(const PrismaUIService& service) {
		PrismaFeatureView::Initialize(service, kHtmlPath, kVerifyJsFunc, &PersuasionView::RegisterListeners);
	}

	void PersuasionView::RegisterListeners(const PrismaViewHandle& view) {
		view.RegisterListener(PersuasionContract::Listener::PersuasionWedgeHover, OnWedgeHover);
		view.RegisterListener(PersuasionContract::Listener::PersuasionClose, OnCloseFromJS);
		view.RegisterListener(PersuasionContract::Listener::PersuasionDispositionChanged, OnDispositionChanged);
		view.RegisterListener(PersuasionContract::Listener::PersuasionBribe, OnBribe);
	}

	RE::Actor* PersuasionView::GetCurrentActorOrWarn(const char* context) {
		if (s_currentTargetFormID == 0) {
			Log::WARN("{}: s_currentTargetFormID was 0.", context);
			return nullptr;
		}
		auto* actor = RE::TESForm::LookupByID<RE::Actor>(s_currentTargetFormID);
		if (!actor) {
			Log::WARN("{}: actor was null for {:08X}.", context, s_currentTargetFormID);
		}
		return actor;
	}

	void PersuasionView::Open(RE::Actor* target) {
		if (!Ready("PersuasionView::Open")) {
			return;
		}
		if (!target) {
			Log::WARN("PersuasionView::Open: Argument target was null.");
			return;
		}
		s_currentTargetFormID = target->GetFormID();

		auto displayName = target->GetDisplayFullName();
		std::string name = displayName ? displayName : "Name Not Found";
		int disposition = DispositionSystem::GetDispositionActorValue(target);
		auto prefs = PersuasionPreferences::GetRandomPreferences(target);

		nlohmann::json preferences = nlohmann::json::object();
		for (const auto& [action, pref] : prefs) {
			preferences[std::string(action)] = std::string(pref);
		}
		//Settings, menu overrides and this player's perk rules, as they stand right now.
		s_effective = Rules::Resolve();
		const nlohmann::json payload = {
			{ "name", name },
			{ "disposition", disposition },
			{ "preferences", preferences },
			{ "rules", Rules::ToJson(s_effective) }
		};

		DialogueSession::Start(target);

		s_handle.InvokeByFunctionName(PersuasionContract::JsFunc::PersuasionInit, payload);
		s_handle.Show();
		//pauseGame=false (default) keeps the world running so the NPC can show facial reactions and even say voice lines if eventually implemented.
		s_handle.Focus();
	}

	void PersuasionView::Close() {
		s_handle.UnfocusAndHide();

		if (s_currentTargetFormID == 0) {
			Log::WARN("PersuasionView::Close: s_currentTargetFormID was 0.");
			return;
		}

		if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(s_currentTargetFormID)) {
			ActorExpression::Reset(actor);
		}

		DialogueSession::End(s_currentTargetFormID);

		s_currentTargetFormID = 0;
	}

	//When argument is an empty string, the user has moved away from the wedge.
	void PersuasionView::OnWedgeHover(const char* preference) {
		if (!preference) {
			Log::WARN("PersuasionView::OnWedgeHover: preference was null.");
			return;
		}
		auto* actor = GetCurrentActorOrWarn("PersuasionView::OnWedgeHover");
		if (!actor) {
			return;
		}
		if (const auto* info = PersuasionPreferences::GetPreference(preference)) {
			ActorExpression::Set(actor, info->expression, info->strength);
		}
		else {
			ActorExpression::Reset(actor);
		}
	}

	void PersuasionView::OnDispositionChanged(const char* disposition) {
		if (!disposition) {
			Log::WARN("PersuasionView::OnDispositionChanged: disposition was null.");
			return;
		}
		auto* actor = GetCurrentActorOrWarn("PersuasionView::OnDispositionChanged");
		if (!actor) {
			return;
		}
		float dispositionFloat = std::strtof(disposition, nullptr);
		SetDisposition(actor, dispositionFloat);
	}

	//For now, closeMethod is not utilized.
	void PersuasionView::OnCloseFromJS(const char* closeMethod) {
		Log::DEBUG("PersuasionView::OnCloseFromJS: {}", closeMethod ? closeMethod : "(null)");
		Close();
	}

	void PersuasionView::OnBribe(const char* disposition) {
		if (!disposition) {
			Log::WARN("PersuasionView::OnBribe: disposition was null.");
			return;
		}
		auto* actor = GetCurrentActorOrWarn("PersuasionView::OnBribe");
		if (!actor) {
			return;
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			Log::WARN("PersuasionView::OnBribe: Player was null.");
			return;
		}

		//One bribe under the conversation's rules, shared with the Scaleform menu.
		const float before = std::strtof(disposition, nullptr);
		const BribeAttempt attempt = Bribery::Attempt(actor, before, s_effective);
		if (attempt.success) {
			SendBribeResult(true, attempt.price, attempt.gain, attempt.disposition, "");
			return;
		}
		switch (attempt.reason) {
		case BribeAttempt::Reason::kDisabled:
			SendBribeResult(false, 0, 0, before, "No bribes");
			break;
		case BribeAttempt::Reason::kNoGold:
			SendBribeResult(false, attempt.price, 0, before, "Not Enough Gold");
			break;
		default:
			SendBribeResult(false, attempt.price, 0, before, "Bribe failed");
			break;
		}
	}

	void PersuasionView::SendBribeResult(bool success, int price, int gain, float disposition, const char* reason) {
		const nlohmann::json payload = {
			{ "success", success },
			{ "price", price },
			{ "gain", gain },
			{ "disposition", static_cast<int>(disposition) },
			{ "reason", reason ? reason : "" }
		};
		s_handle.InvokeByFunctionName(PersuasionContract::JsFunc::PersuasionBribeResult, payload);
	}

	void PersuasionView::SetDisposition(RE::Actor* actor, float value) {
		DispositionSystem::SetDispositionActorValue(actor, value, true);
	}

}
