#include "PersuasionView.h"
#include "BribeEconomy.h"
#include "PersuasionContract.h"
#include "Disposition/DispositionSystem.h"

#include "../PrismaViewHandle.h"

#include <nlohmann/json.hpp>
#include <array>
#include <random>
#include <string>
#include <string_view>
#include <utility>

namespace Persuasion {

	RE::FormID PersuasionView::s_currentTargetFormID = 0;

	namespace {
		// Invokes `Actor.SetDontMove(value)` on the target through the Papyrus VM. There is no
		// direct C++ entry point for this on RE::Actor in CommonLibSSE, so we dispatch through
		// the VM the same way a Papyrus script would call it.
		void SetActorDontMove(RE::Actor* actor, bool value) {
			if (!actor) {
				return;
			}
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm) {
				return;
			}
			auto policy = vm->GetObjectHandlePolicy();
			if (!policy) {
				return;
			}
			auto handle = policy->GetHandleForObject(actor->GetFormType(), actor);
			if (handle == policy->EmptyHandle()) {
				return;
			}
			auto* args = RE::MakeFunctionArguments(std::move(value));
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			vm->DispatchMethodCall2(handle, "Actor", "SetDontMove", args, callback);
		}

		//Strong reactions are required for them to be visible on the NPC's face.
		constexpr int kExpressionAnger = 0;//DialogueAnger
		constexpr int kExpressionHappy = 2;//DialogueHappy
		constexpr int kExpressionMoodAnger = 8;//MoodAnger
		constexpr int kExpressionMoodHappy = 10;//MoodHappy

		// Single source of truth tying each bridge preference token to the facial expression
		// it drives. `name` must match a PersuasionContract::Preference constant; `strength`
		// is on the 0..100 scale SetActorExpression expects.
		struct PreferenceInfo {
			std::string_view name;
			int              expressionId;
			float            strength;
		};

		constexpr std::array<PreferenceInfo, 4> kPreferences = { {
			{ "Love", kExpressionHappy, 100.0f},
			{ "Like",  kExpressionHappy, 50.0f },
			{ "Dislike", kExpressionAnger, 50.0f},
			{ "Hate", kExpressionAnger, 100.0f},
		} };

		const PreferenceInfo* GetPreference(std::string_view preferenceName) {
			for (const auto& info : kPreferences) {
				if (info.name == preferenceName) {
					return &info;
				}
			}
			return nullptr;
		}

		// Fixed action quadrants. Their count drives the preference shuffle below.
		constexpr std::array<std::string_view, 4> kActions = { "Admire", "Boast", "Joke", "Coerce" };
		constexpr std::size_t kActionCount = kActions.size();
		static_assert(kActionCount == kPreferences.size(), "each action must map to exactly one preference");

		//Pairs Love/Like/Dislike/Hate preferences with Admire/Boast/Joke/Coerce actions.
		//Randomly paired by Form ID to remain consistent between game sessions.
		std::array<std::pair<std::string_view, std::string_view>, kActionCount> GetRandomPreferences(RE::Actor* target) {
			std::array<int, kActionCount> idx{};
			for (std::size_t i = 0; i < kActionCount; ++i) {
				idx[i] = static_cast<int>(i);
			}
			std::mt19937 rng(target->GetFormID());
			for (int i = static_cast<int>(kActionCount) - 1; i > 0; --i) {
				std::uniform_int_distribution<int> dist(0, i);
				int j = dist(rng);
				std::swap(idx[i], idx[j]);
			}
			std::array<std::pair<std::string_view, std::string_view>, kActionCount> result;
			for (std::size_t i = 0; i < kActionCount; ++i) {
				result[i] = { kActions[i], kPreferences[idx[i]].name };
			}
			return result;
		}

		//Resets expression then sets a new expression.
		void SetActorExpression(RE::Actor* actor, int expressionId, float value) {
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
			taskInterface->AddTask([formID, expressionId, value]() {
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
				faceGen->expression3.SetValue(kExpressionAnger, 0.0f);
				faceGen->expression3.SetValue(kExpressionHappy, 0.0f);
				faceGen->expression3.SetValue(kExpressionMoodAnger, 0.0f);
				faceGen->expression3.SetValue(kExpressionMoodHappy, 0.0f);
				if (value <= 0.0f) {
					faceGen->ClearExpressionOverride();
				}
				else {
					faceGen->SetExpressionOverride(static_cast<std::uint32_t>(expressionId), value);
					faceGen->exprOverride = true;
					const float normalized = value / 100.0f;
					faceGen->expression3.SetValue(static_cast<std::uint32_t>(expressionId), normalized);
					//Add even more expression with moods:
					if (expressionId == kExpressionHappy) {
						faceGen->expression3.SetValue(kExpressionMoodHappy, normalized);
					}
					else if (expressionId == kExpressionAnger) {
						faceGen->expression3.SetValue(kExpressionMoodAnger, normalized);
					}
				}
				});
		}

		void ResetActorExpression(RE::Actor* actor) {
			SetActorExpression(actor, 0, 0);
		}
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
		auto prefs = GetRandomPreferences(target);

		nlohmann::json preferences = nlohmann::json::object();
		for (const auto& [action, pref] : prefs) {
			preferences[std::string(action)] = std::string(pref);
		}
		const nlohmann::json payload = {
			{ "name", name },
			{ "disposition", disposition },
			{ "preferences", preferences }
		};

		if (kConfiguredMode == CaptivityMode::Captive) {
			//Close dialogue menu but don't let NPC walk away.
			if (auto* mtm = RE::MenuTopicManager::GetSingleton()) {
				mtm->forceGoodbye = true;
			}
			SetActorDontMove(target, true);
		}

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

		auto* actor = RE::TESForm::LookupByID<RE::Actor>(s_currentTargetFormID);

		if (actor) {
			ResetActorExpression(actor);
			if (kConfiguredMode == CaptivityMode::Captive) {
				SetActorDontMove(actor, false);
				if (auto* player = RE::PlayerCharacter::GetSingleton()) {
					actor->ActivateRef(player, 0, nullptr, 1, false);//Resume dialogue
				}
			}
		}

		s_currentTargetFormID = 0;
	}

	//When argument is an empty string, the user has moved away from the wedge.
	void PersuasionView::OnWedgeHover(const char* preference) {
		if (!preference) {
			Log::WARN("PersuasionView::OnWedgeHover: preference was null.");
			return;
		}
		if (s_currentTargetFormID == 0) {
			Log::WARN("PersuasionView::OnWedgeHover: s_currentTargetFormID was 0.");
			return;
		}
		auto* actor = RE::TESForm::LookupByID<RE::Actor>(s_currentTargetFormID);
		if (!actor) {
			Log::WARN("PersuasionView::OnWedgeHover: actor was null for {:08X}.", s_currentTargetFormID);
			return;
		}
		if (const auto* info = GetPreference(preference)) {
			SetActorExpression(actor, info->expressionId, info->strength);
		}
		else {
			ResetActorExpression(actor);
		}
	}

	void PersuasionView::OnDispositionChanged(const char* disposition) {
		if (!disposition) {
			Log::WARN("PersuasionView::OnDispositionChanged: disposition was null.");
			return;
		}
		if (s_currentTargetFormID == 0) {
			Log::WARN("PersuasionView::OnDispositionChanged: s_currentTargetFormID was 0.");
			return;
		}
		auto* actor = RE::TESForm::LookupByID<RE::Actor>(s_currentTargetFormID);
		if (!actor) {
			Log::WARN("PersuasionView::OnDispositionChanged: Actor was null for {:08X}.", s_currentTargetFormID);
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
		if (s_currentTargetFormID == 0) {
			Log::WARN("PersuasionView::OnBribe: s_currentTargetFormID was 0.");
			return;
		}
		auto* actor = RE::TESForm::LookupByID<RE::Actor>(s_currentTargetFormID);
		if (!actor) {
			Log::WARN("PersuasionView::OnBribe: Actor was null for {:08X}.", s_currentTargetFormID);
			return;
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			Log::WARN("PersuasionView::OnBribe: Player was null.");
			return;
		}

		auto* avNpc = REBridge::AVOwner(actor);
		auto* avPlayer = REBridge::AVOwner(player);
		const float dispositionFloat = std::strtof(disposition, nullptr);
		const float playerSpeech = avPlayer->GetActorValue(RE::ActorValue::kSpeech);
		const float npcSpeech = avNpc->GetActorValue(RE::ActorValue::kSpeech);
		const int playerLevel = player->GetLevel();
		const int npcLevel = actor->GetLevel();

		const BribeEconomy::Outcome outcome = BribeEconomy::Calculate(dispositionFloat, playerSpeech, npcSpeech, playerLevel, npcLevel);

		const int playerGold = player->GetGoldAmount();
		if (playerGold < outcome.price) {
			SendBribeResult(false, outcome.price, 0, dispositionFloat, "Not Enough Gold");
			return;
		}

		auto* goldForm = RE::TESForm::LookupByID(0x0000000F);//gold
		auto* goldBoundObj = goldForm ? goldForm->As<RE::TESBoundObject>() : nullptr;
		if (!goldBoundObj) {
			Log::ERROR("PersuasionView::OnBribe: Gold001 (0x0000000F) Form Missing");
			return;
		}
		player->RemoveItem(goldBoundObj, outcome.price, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

		const float newDisposition = std::min(BribeEconomy::kDispositionMax, dispositionFloat + static_cast<float>(outcome.gain));
		SetDisposition(actor, newDisposition);
		SendBribeResult(true, outcome.price, outcome.gain, newDisposition, "");

		Log::INFO("PersuasionView::OnBribe: price={}, gain={}, disposition={} -> {} (playerLevel={}, npcLevel={}, playerSpeech={}, npcSpeech={})",
			outcome.price, outcome.gain, dispositionFloat, newDisposition, playerLevel, npcLevel, playerSpeech, npcSpeech);
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
