#include "PersuasionView.h"
#include "BribeEconomy.h"
#include "PersuasionContract.h"
#include "DialogueSession.h"
#include "PersuasionPreferences.h"
#include "Disposition/DispositionSystem.h"
#include "Actors/ActorExpression.h"
#include "Resources.h"

#include "../PrismaViewHandle.h"

#include <nlohmann/json.hpp>
#include <string>

namespace Persuasion {

	RE::ActorHandle PersuasionView::s_currentTarget = {};

	namespace {
		struct BribeInputs {
			float disposition;
			float playerSpeech;
			float npcSpeech;
			int   playerLevel;
			int   npcLevel;
		};

		// Get the bribe inputs. Returns false and logs if the speech ActorValue owners can't be found.
		bool GetBribeInputs(RE::Actor* actor, RE::PlayerCharacter* player, float disposition, BribeInputs& out) {
			auto* avNpc = REBridge::AVOwner(actor);
			auto* avPlayer = REBridge::AVOwner(player);
			if (!avNpc || !avPlayer) {
				Log::WARN("PersuasionView::OnBribe: ActorValueOwner was null.");
				return false;
			}
			out.disposition = disposition;
			out.playerSpeech = avPlayer->GetActorValue(RE::ActorValue::kSpeech);
			out.npcSpeech = avNpc->GetActorValue(RE::ActorValue::kSpeech);
			out.playerLevel = player->GetLevel();
			out.npcLevel = actor->GetLevel();
			return true;
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

	RE::Actor* PersuasionView::GetCurrentActorOrWarn(const char* context) {
		//This can be empty: A late JavaScript event may arrive after Close() cleared the target.
		if (!s_currentTarget) {
			Log::WARN("{}: s_currentTarget was a null handle.", context);
			return nullptr;
		}

		RE::ActorPtr actor = s_currentTarget.get();

		if (!actor) {
			Log::WARN("{}: actor was null for handle {:08X}.", context, s_currentTarget.native_handle());
		}
		return actor.get();
	}

	void PersuasionView::Open(RE::Actor* target) {
		if (!Ready("PersuasionView::Open")) {
			return;
		}
		if (!target) {
			Log::WARN("PersuasionView::Open: Argument target was null.");
			return;
		}
		s_currentTarget = target->GetHandle();

		auto displayName = target->GetDisplayFullName();
		std::string name = displayName ? displayName : "Name Not Found";
		int disposition = DispositionSystem::GetDispositionActorValue(target);
		auto prefs = PersuasionPreferences::GetRandomPreferences(target);

		nlohmann::json preferences = nlohmann::json::object();
		for (const auto& [action, pref] : prefs) {
			preferences[std::string(action)] = std::string(pref);
		}
		const nlohmann::json payload = {
			{ "name", name },
			{ "disposition", disposition },
			{ "preferences", preferences }
		};

		DialogueSession::Start(target);

		s_handle.InvokeByFunctionName(PersuasionContract::JsFunc::PersuasionInit, payload);
		s_handle.Show();
		//pauseGame=false (default) keeps the world running so the NPC can show facial reactions and even say voice lines if eventually implemented.
		s_handle.Focus();
	}

	void PersuasionView::Close() {
		s_handle.UnfocusAndHide();

		if (!s_currentTarget) {
			Log::WARN("PersuasionView::Close: s_currentTarget was a null handle.");
			return;
		}

		if (RE::ActorPtr actor = s_currentTarget.get()) {
			ActorExpression::Reset(actor.get());
		}

		DialogueSession::End(s_currentTarget);

		s_currentTarget = RE::ActorHandle{};
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

		BribeInputs inputs;
		if (!GetBribeInputs(actor, player, std::strtof(disposition, nullptr), inputs)) {
			return;
		}

		const BribeEconomy::Outcome outcome = BribeEconomy::Calculate(inputs.disposition, inputs.playerSpeech, inputs.npcSpeech, inputs.playerLevel, inputs.npcLevel);

		if (player->GetGoldAmount() < outcome.price) {
			SendBribeResult(false, outcome.price, 0, inputs.disposition, "Not Enough Gold");
			return;
		}
		player->RemoveItem(Resources::gold, outcome.price, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

		const float newDisposition = std::min(BribeEconomy::kDispositionMax, inputs.disposition + static_cast<float>(outcome.gain));
		SetDisposition(actor, newDisposition);
		SendBribeResult(true, outcome.price, outcome.gain, newDisposition, "");

		Log::INFO("PersuasionView::OnBribe: price={}, gain={}, disposition={} -> {} (playerLevel={}, npcLevel={}, playerSpeech={}, npcSpeech={})",
			outcome.price, outcome.gain, inputs.disposition, newDisposition, inputs.playerLevel, inputs.npcLevel, inputs.playerSpeech, inputs.npcSpeech);
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

	//PrismaUI calls back from its own thread, so change the actor value on the main thread.
	void PersuasionView::SetDisposition(RE::Actor* actor, float value) {
		auto* taskInterface = SKSE::GetTaskInterface();
		assert(taskInterface);
		RE::ActorHandle handle = actor->GetHandle();
		taskInterface->AddTask([handle, value]() {
			if (RE::ActorPtr actor = handle.get()) {
				DispositionSystem::SetDispositionActorValue(actor.get(), value, true);
			}
			});
	}

}
