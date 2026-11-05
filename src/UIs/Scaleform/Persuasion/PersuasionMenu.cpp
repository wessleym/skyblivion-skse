#include "UIs/Scaleform/Persuasion/PersuasionMenu.h"

#include "Actors/ActorExpression.h"
#include "Disposition/DispositionSystem.h"
#include "Platform/MainThread.h"
#include "UIs/PrismaUI/Persuasion/DialogueSession.h"
#include "UIs/PrismaUI/Persuasion/PersuasionBribery.h"
#include "UIs/PrismaUI/Persuasion/PersuasionPreferences.h"
#include "UIs/PrismaUI/Persuasion/PersuasionRules.h"

#include <nlohmann/json.hpp>
#include <string>
#include "UIs/Scaleform/ScaleformUtility.h"

// <Windows.h>, which this plugin's CommonLib build includes, maps PlaySound to PlaySoundA.
#ifdef PlaySound
#	undef PlaySound
#endif

namespace Persuasion
{
	namespace
	{
		//The NPC being persuaded, as a handle: null once the reference is gone, and it keeps the actor from
		//being unloaded under the menu. Main thread only (the callbacks run as tasks).
		RE::ActorHandle s_target;
		Effective       s_effective{};  // the conversation's rules, resolved on Ready

		RE::Actor* TargetOrWarn(const char* a_context)
		{
			if (!s_target) {
				Log::WARN("{}: no persuasion target", a_context);
				return nullptr;
			}
			RE::ActorPtr actor = s_target.get();
			if (!actor) {
				Log::WARN("{}: the persuasion target is gone", a_context);
			}
			return actor.get();
		}

		// nlohmann::json -> GFxValue, strings owned by the movie.
		void ToGFx(RE::GFxMovieView* a_movie, const nlohmann::json& a_json, RE::GFxValue& a_out)
		{
			if (a_json.is_object()) {
				a_movie->CreateObject(&a_out);
				for (const auto& [key, value] : a_json.items()) {
					RE::GFxValue member;
					ToGFx(a_movie, value, member);
					a_out.SetMember(key.c_str(), member);
				}
			} else if (a_json.is_array()) {
				a_movie->CreateArray(&a_out);
				for (const auto& value : a_json) {
					RE::GFxValue element;
					ToGFx(a_movie, value, element);
					a_out.PushBack(element);
				}
			} else if (a_json.is_boolean()) {
				a_out = RE::GFxValue(a_json.get<bool>());
			} else if (a_json.is_number()) {
				a_out = RE::GFxValue(a_json.get<double>());
			} else if (a_json.is_string()) {
				const std::string text = a_json.get<std::string>();
				a_movie->CreateString(&a_out, text.c_str());
			} else {
				a_out = RE::GFxValue();
			}
		}

		// Into the movie: on the main thread, next frame.
		void Invoke(const char* a_function, nlohmann::json a_payload)
		{
			MainThread::Post([function = std::string(a_function), payload = std::move(a_payload)]() {
				auto* ui = RE::UI::GetSingleton();
				auto menu = ui ? ui->GetMenu(PersuasionMenu::MENU_NAME) : nullptr;
				auto movie = menu ? menu->uiMovie : nullptr;
				if (!movie) {
					return;
				}
				RE::GFxValue value;
				ToGFx(movie.get(), payload, value);
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
		auto scaleform = RE::BSScaleformManager::GetSingleton();
		const bool loaded = scaleform->LoadMovieEx(this, FILE_NAME, [this](RE::GFxMovieDef* a_def) {
			fxDelegate.reset(new RE::FxDelegate());
			fxDelegate->RegisterHandler(this);
			a_def->SetState(RE::GFxState::StateType::kExternalInterface, fxDelegate.get());
			fxDelegate->Release();
		});
		if (!loaded) {
			//Closed at once: a menu without its movie would sit invisible and modal.
			Log::ERROR("persuasion menu: Interface\\{}.swf failed to load - closing", FILE_NAME);
			if (auto queue = RE::UIMessageQueue::GetSingleton()) {
				queue->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
			}
		}

		depthPriority = 5;
		inputContext = Context::kMenuMode;
		//No kPausesGame: the world runs so the NPC's face answers the wedge under the cursor,
		//as with the PrismaUI view. Over the dialogue menu, which DialogueSession hides meanwhile.
		menuFlags.set(RE::UI_MENU_FLAGS::kDisablePauseMenu, RE::UI_MENU_FLAGS::kModal,
					  RE::UI_MENU_FLAGS::kUsesMenuContext, RE::UI_MENU_FLAGS::kTopmostRenderedMenu,
					  RE::UI_MENU_FLAGS::kRequiresUpdate, RE::UI_MENU_FLAGS::kUpdateUsesCursor);
		auto input = RE::BSInputDeviceManager::GetSingleton();
		if (!input || !input->IsGamepadEnabled()) {
			menuFlags.set(RE::UI_MENU_FLAGS::kUsesCursor);
		}
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
		auto queue = RE::UIMessageQueue::GetSingleton();
		auto ui = RE::UI::GetSingleton();
		if (!queue || !ui || !a_target) {
			Log::WARN("PersuasionMenu::Open: no target or no UI");
			return;
		}
		if (ui->IsMenuOpen(MENU_NAME)) {
			Log::WARN("PersuasionMenu::Open: already open");
			return;
		}
		if (!ScaleformUtility::MovieExists(FILE_NAME)) {
			Log::ERROR("persuasion menu: Interface\\{}.swf not found - not opened", FILE_NAME);
			return;
		}
		s_target = a_target->GetHandle();
		//Holds the NPC in conversation and hides the dialogue menu while the wheel is up.
		DialogueSession::Start(a_target);
		queue->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
		Log::INFO("persuasion menu: opening for {:08X}", a_target->GetFormID());
	}

	void PersuasionMenu::Close()
	{
		if (auto queue = RE::UIMessageQueue::GetSingleton()) {
			queue->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
		}
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
		if (a_message.type == RE::UI_MESSAGE_TYPE::kHide) {
			const RE::ActorHandle target = s_target;
			s_target.reset();
			if (target) {
				SKSE::GetTaskInterface()->AddTask([target] {
					if (RE::ActorPtr actor = target.get()) {
						ActorExpression::Reset(actor.get());
					}
					//Reopens the dialogue so its topics refresh.
					DialogueSession::End(target);
				});
			}
		}
		return RE::IMenu::ProcessMessage(a_message);
	}

	void PersuasionMenu::OnReady(const RE::FxDelegateArgs&)
	{
		SKSE::GetTaskInterface()->AddTask([] {
			auto* actor = TargetOrWarn("PersuasionMenu::OnReady");
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
			auto input = RE::BSInputDeviceManager::GetSingleton();
			nlohmann::json payload = {
				{ "name", name },
				{ "disposition", disposition },
				{ "preferences", preferences },
				{ "rules", Rules::ToJson(s_effective) },
				{ "gamepad", input && input->IsGamepadEnabled() }
			};
			Invoke("_root.wheel.SetPersuasion", std::move(payload));
		});
	}

	//When the argument is an empty string, the cursor and focus left every wedge.
	void PersuasionMenu::OnHover(const RE::FxDelegateArgs& a_params)
	{
		const std::string preference = a_params.GetArgCount() > 0 && a_params[0].IsString() ? a_params[0].GetString() : "";
		SKSE::GetTaskInterface()->AddTask([preference] {
			auto* actor = TargetOrWarn("PersuasionMenu::OnHover");
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
		SKSE::GetTaskInterface()->AddTask([value] {
			if (auto* actor = TargetOrWarn("PersuasionMenu::OnDisposition")) {
				DispositionSystem::SetDispositionActorValue(actor, value, true);
			}
		});
	}

	void PersuasionMenu::OnBribe(const RE::FxDelegateArgs& a_params)
	{
		if (a_params.GetArgCount() < 1 || !a_params[0].IsNumber()) {
			return;
		}
		const float disposition = static_cast<float>(a_params[0].GetNumber());
		SKSE::GetTaskInterface()->AddTask([disposition] {
			auto* actor = TargetOrWarn("PersuasionMenu::OnBribe");
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
		SKSE::GetTaskInterface()->AddTask([disposition] {
			auto* actor = TargetOrWarn("PersuasionMenu::OnBribeQuote");
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
