#include "UIs/Scaleform/Persuasion/PersuasionMenu.h"

#include "Actors/ActorExpression.h"
#include "Disposition/DispositionSystem.h"
#include "Platform/MainThread.h"
#include "UIs/PrismaUI/Persuasion/DialogueSession.h"
#include "UIs/PrismaUI/Persuasion/PersuasionBribery.h"
#include "UIs/PrismaUI/Persuasion/PersuasionPreferences.h"
#include "UIs/PrismaUI/Persuasion/PersuasionRules.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <atomic>
#include <string>

#include <nlohmann/json.hpp>

namespace Persuasion
{
	namespace
	{
		//The NPC being persuaded, as a handle: it resolves to null once the reference is gone (it does not keep
		//the actor loaded). Set by Open, cleared by kHide. Each movie callback copies it when it fires, so a task
		//that runs after the close still reaches the NPC the wheel was showing (the closing disposition).
		RE::ActorHandle s_target;
		Effective       s_effective{};  // the conversation's rules, resolved on Ready
		//Set by Close: the next hide is the wheel's own. Any other hide is the engine tearing the menu down
		//(a load, a 'coc', a reload after death; seen in game as a plain kHide).
		std::atomic<bool> s_hideRequested{ false };

		RE::ActorPtr TargetOrWarn(RE::ActorHandle a_target, const char* a_context)
		{
			if (!a_target) {
				Log::WARN("{}: no persuasion target", a_context);
				return {};
			}
			RE::ActorPtr actor = REBridge::Lookup(a_target);
			if (!actor) {
				Log::WARN("{}: the persuasion target is gone", a_context);
			}
			return actor;
		}

		// Into the movie: on the main thread, next frame.
		void Invoke(const char* a_function, nlohmann::json a_payload)
		{
			MainThread::Post([function = std::string(a_function), payload = std::move(a_payload)]() {
				auto movie = ScaleformUtility::OpenMenuMovie(PersuasionMenu::MENU_NAME);
				if (!movie) {
					return;
				}
				const RE::GFxValue value = ScaleformUtility::ToGFx(*movie, payload);
				movie->Invoke(function.c_str(), nullptr, &value, 1);
			});
		}

		const char* ReasonKey(BribeAttempt::Reason a_reason)
		{
			switch (a_reason) {
			case BribeAttempt::Reason::kDisabled: return "$SKYB_persuade_bribe_off";
			case BribeAttempt::Reason::kNoGold:   return "$SKYB_persuade_bribe_gold";
			case BribeAttempt::Reason::kTooSmall: return "$SKYB_persuade_bribe_small";
			case BribeAttempt::Reason::kError:    return "$SKYB_persuade_bribe_gold";
			default:                              return "";
			}
		}
	}

	// ============================================================== menu =====
	PersuasionMenu::PersuasionMenu()
	{
		//Not paused: the world runs so the NPC's face answers the wedge under the cursor,
		//as with the PrismaUI view. Over the dialogue menu, which DialogueSession hides meanwhile.
		ScaleformUtility::LoadMenuMovie(*this, FILE_NAME, 5, false, Close);
	}

	void PersuasionMenu::Register()
	{
		if (auto ui = RE::UI::GetSingleton()) {
			ui->Register(MENU_NAME, Creator);
			Log::INFO("persuasion menu registered as '{}'", MENU_NAME);
		}
	}

	void PersuasionMenu::Open(RE::Actor* a_target)
	{
		if (!a_target) {
			Log::WARN("PersuasionMenu::Open: no target");
			return;
		}
		if (!ScaleformUtility::CanShowMenu(MENU_NAME, FILE_NAME)) {
			return;
		}
		s_target = a_target->GetHandle();
		s_hideRequested = false;
		//Holds the NPC in conversation and hides the dialogue menu while the wheel is up.
		DialogueSession::Start(a_target, Close);
		ScaleformUtility::ShowMenu(MENU_NAME);
		Log::INFO("persuasion menu: opening for {:08X}", a_target->GetFormID());
	}

	void PersuasionMenu::Close()
	{
		s_hideRequested = true;
		ScaleformUtility::HideMenu(MENU_NAME);
	}

	void PersuasionMenu::Accept(RE::FxDelegateHandler::CallbackProcessor* a_cbReg)
	{
		a_cbReg->Process("SKYBPersuade_Ready", OnReady);
		a_cbReg->Process("SKYBPersuade_Hover", OnHover);
		a_cbReg->Process("SKYBPersuade_Disposition", OnDisposition);
		a_cbReg->Process("SKYBPersuade_Bribe", OnBribe);
		a_cbReg->Process("SKYBPersuade_BribeQuote", OnBribeQuote);
		a_cbReg->Process("SKYBPersuade_Close", OnClose);
		a_cbReg->Process("PlaySound", OnPlaySound);
	}

	RE::UI_MESSAGE_RESULTS PersuasionMenu::ProcessMessage(RE::UIMessage& a_message)
	{
		if (a_message.type == RE::UI_MESSAGE_TYPE::kHide || a_message.type == RE::UI_MESSAGE_TYPE::kForceHide) {
			//The wheel's own close (Close) reopens the dialogue so its topics refresh. A hide the engine sends
			//(a load, a 'coc', a reload after death) lets the NPC go without reopening anything.
			const bool ours = a_message.type == RE::UI_MESSAGE_TYPE::kHide && s_hideRequested.exchange(false);
			const RE::ActorHandle target = s_target;
			s_target.reset();
			if (!ours) {
				DialogueSession::Abort();
			}
			if (target) {
				SKSE::GetTaskInterface()->AddTask([target, ours] {
					if (RE::ActorPtr actor = REBridge::Lookup(target)) {
						ActorExpression::Reset(actor.get());
					}
					if (ours) {
						DialogueSession::End(target);
					}
				});
			}
		}
		return RE::IMenu::ProcessMessage(a_message);
	}

	void PersuasionMenu::OnReady(const RE::FxDelegateArgs&)
	{
		SKSE::GetTaskInterface()->AddTask([target = s_target] {
			const RE::ActorPtr actorPtr = TargetOrWarn(target, "PersuasionMenu::OnReady");
			auto* actor = actorPtr.get();
			if (!actor) {
				Close();
				return;
			}
			//Settings, menu overrides and this player's perk rules, as they stand right now.
			s_effective = Rules::Resolve();

			const char* displayName = actor->GetDisplayFullName();
			const std::string name = displayName ? displayName : actor->GetName();
			const int disposition = DispositionSystem::GetDispositionActorValue(actor);
			nlohmann::json preferences = nlohmann::json::object();
			for (const auto& [action, pref] : PersuasionPreferences::GetRandomPreferences(actor)) {
				preferences[std::string(action)] = std::string(pref);
			}
			nlohmann::json payload = {
				{ "name", name },
				{ "disposition", disposition },
				{ "preferences", preferences },
				{ "rules", Rules::ToJson(s_effective) },
				{ "gamepad", ScaleformUtility::GamepadActive() }
			};
			Invoke("_root.wheel.SetPersuasion", std::move(payload));
		});
	}

	//When the argument is an empty string, the cursor and focus left every wedge.
	void PersuasionMenu::OnHover(const RE::FxDelegateArgs& a_params)
	{
		const std::string preference = a_params.GetArgCount() > 0 && a_params[0].IsString() ? a_params[0].GetString() : "";
		SKSE::GetTaskInterface()->AddTask([target = s_target, preference] {
			const RE::ActorPtr actorPtr = TargetOrWarn(target, "PersuasionMenu::OnHover");
			auto* actor = actorPtr.get();
			if (!actor) {
				return;
			}
			if (const auto* info = PersuasionPreferences::GetPreference(preference)) {
				ActorExpression::Set(actor, info->expression, info->strength);
			} else {
				ActorExpression::Reset(actor);
			}
		});
	}

	void PersuasionMenu::OnDisposition(const RE::FxDelegateArgs& a_params)
	{
		if (a_params.GetArgCount() < 1 || !a_params[0].IsNumber()) {
			return;
		}
		const float value = static_cast<float>(a_params[0].GetNumber());
		SKSE::GetTaskInterface()->AddTask([target = s_target, value] {
			if (const RE::ActorPtr actor = TargetOrWarn(target, "PersuasionMenu::OnDisposition"); actor && !actor->IsDead()) {
				DispositionSystem::SetDispositionActorValue(actor.get(), value, true);
			}
		});
	}

	void PersuasionMenu::OnBribe(const RE::FxDelegateArgs& a_params)
	{
		if (a_params.GetArgCount() < 1 || !a_params[0].IsNumber()) {
			return;
		}
		const float disposition = static_cast<float>(a_params[0].GetNumber());
		SKSE::GetTaskInterface()->AddTask([target = s_target, disposition] {
			const RE::ActorPtr actorPtr = TargetOrWarn(target, "PersuasionMenu::OnBribe");
			auto* actor = actorPtr.get();
			if (!actor) {
				return;
			}
			const BribeAttempt attempt = Bribery::Attempt(actor, disposition, s_effective);
			Invoke("_root.wheel.SetBribeResult", nlohmann::json{
				{ "success", attempt.success },
				{ "price", attempt.price },
				{ "gain", attempt.gain },
				{ "disposition", static_cast<int>(attempt.disposition) },
				{ "reason", ReasonKey(attempt.reason) }
			});
		});
	}

	//The Bribe button's price at the disposition the wheel shows: nothing is taken.
	void PersuasionMenu::OnBribeQuote(const RE::FxDelegateArgs& a_params)
	{
		if (a_params.GetArgCount() < 1 || !a_params[0].IsNumber()) {
			return;
		}
		const float disposition = static_cast<float>(a_params[0].GetNumber());
		SKSE::GetTaskInterface()->AddTask([target = s_target, disposition] {
			const RE::ActorPtr actorPtr = TargetOrWarn(target, "PersuasionMenu::OnBribeQuote");
			auto* actor = actorPtr.get();
			if (!actor) {
				return;
			}
			const BribeAttempt quote = Bribery::Quote(actor, disposition, s_effective);
			Invoke("_root.wheel.SetBribeQuote", nlohmann::json{
				{ "disposition", disposition },
				{ "price", quote.price },
				{ "gain", quote.gain },
				{ "enabled", quote.reason != BribeAttempt::Reason::kDisabled && quote.reason != BribeAttempt::Reason::kError },
				{ "useful", quote.reason != BribeAttempt::Reason::kTooSmall },
				{ "affordable", quote.reason == BribeAttempt::Reason::kNone }
			});
		});
	}

	void PersuasionMenu::OnClose(const RE::FxDelegateArgs&)
	{
		Close();
	}

	void PersuasionMenu::OnPlaySound(const RE::FxDelegateArgs& a_params)
	{
		if (a_params.GetArgCount() >= 1 && a_params[0].IsString()) {
			RE::PlaySound(a_params[0].GetString());
		}
	}
}
