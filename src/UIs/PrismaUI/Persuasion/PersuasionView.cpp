#include "PersuasionView.h"
#include "BribeEconomy.h"
#include "PersuasionContract.h"
#include "DialogueSession.h"
#include "PersuasionPreferences.h"
#include "Disposition/DispositionSystem.h"
#include "Actors/ActorExpression.h"

#include "../PrismaViewHandle.h"

#include <nlohmann/json.hpp>
#include <string>

namespace Persuasion {

	RE::FormID PersuasionView::s_currentTargetFormID = 0;

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

		// Removes gold from the player. Returns false and logs if the gold form is missing.
		bool ChargeGold(RE::PlayerCharacter* player, int price) {
			auto* goldForm = RE::TESForm::LookupByID(0x0000000F);//gold
			auto* goldBoundObj = goldForm ? goldForm->As<RE::TESBoundObject>() : nullptr;
			if (!goldBoundObj) {
				Log::ERROR("PersuasionView::OnBribe: Gold001 (0x0000000F) Form Missing");
				return false;
			}
			player->RemoveItem(goldBoundObj, price, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
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

		BribeInputs inputs;
		if (!GetBribeInputs(actor, player, std::strtof(disposition, nullptr), inputs)) {
			return;
		}

		const BribeEconomy::Outcome outcome = BribeEconomy::Calculate(inputs.disposition, inputs.playerSpeech, inputs.npcSpeech, inputs.playerLevel, inputs.npcLevel);

		if (player->GetGoldAmount() < outcome.price) {
			SendBribeResult(false, outcome.price, 0, inputs.disposition, "Not Enough Gold");
			return;
		}
		if (!ChargeGold(player, outcome.price)) {
			return;
		}

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

	void PersuasionView::SetDisposition(RE::Actor* actor, float value) {
		DispositionSystem::SetDispositionActorValue(actor, value, true);
	}

}
