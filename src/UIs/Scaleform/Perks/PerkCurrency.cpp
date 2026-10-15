#include "PerkCurrency.h"
#include "PerkDataStore.h"
#include "PerksContract.h"
#include "Papyrus/ModEvents.h"

#include <algorithm>
#include <charconv>

namespace PerksView {

	void PerkCurrency::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("PerkCurrency: No mod-event source. Perk purchases will not be paid for.");
			return;
		}
		Log::INFO("PerkCurrency: Registered.");
	}

	//"HEX|Plugin" names a BGSPerk that is loaded.
	bool PerkCurrency::RankExists(const std::string& rankRef) {
		const auto bar = rankRef.find('|');
		if (bar == std::string::npos || bar == 0 || bar + 1 >= rankRef.size()) {
			return false;
		}
		RE::FormID localId = 0;
		const auto* begin = rankRef.data();
		const auto [end, error] = std::from_chars(begin, begin + bar, localId, 16);
		if (error != std::errc{} || end != begin + bar) {
			return false;
		}
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		return dataHandler && dataHandler->LookupForm<RE::BGSPerk>(localId, rankRef.substr(bar + 1));
	}

	//Payload: "treeId^perkId^HEX|Plugin^currencyType[^globalEditorId]".
	void PerkCurrency::Deduct(const std::string& payload) {
		const auto fields = ModEvents::Split(payload, '^');
		if (fields.size() < 4) {
			Log::WARN("PerkCurrency: Malformed purchase '{}'.", payload);
			return;
		}
		const std::string& treeId = fields[0];
		const std::string& perkId = fields[1];
		const std::string& currencyType = fields[3];

		//Nothing is paid for a rank whose perk record does not exist: Papyrus cannot add it either.
		if (!RankExists(fields[2])) {
			Log::WARN("PerkCurrency: Rank '{}' of '{}' (tree '{}') does not resolve to a perk. Nothing deducted.", fields[2], perkId, treeId);
			return;
		}

		if (currencyType == "perkPoints") {
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				Log::WARN("PerkCurrency: No player. '{}' not paid for.", perkId);
				return;
			}
			auto& pool = REBridge::GameStats(player).perkCount;
			if (pool == 0) {
				Log::WARN("PerkCurrency: No perk points left. '{}' not paid for.", perkId);
				return;
			}
			--pool;
			Log::INFO("PerkCurrency: '{}' (tree '{}') paid with a perk point. {} left.", perkId, treeId, static_cast<int>(pool));
			return;
		}

		if (currencyType != "globalVariable") {
			Log::WARN("PerkCurrency: Unknown currency '{}' for '{}'. Nothing deducted.", currencyType, perkId);
			return;
		}

		//The payload's editor ID, else the tree definition's.
		std::string editorID = fields.size() >= 5 ? fields[4] : std::string{};
		if (editorID.empty()) {
			PerkDataStore::LoadOnce();
			if (const auto* tree = PerkDataStore::FindTree(treeId)) {
				editorID = tree->globalEditorId;
			}
		}
		if (editorID.empty()) {
			Log::WARN("PerkCurrency: No currency global for '{}' (tree '{}'). Nothing deducted.", perkId, treeId);
			return;
		}
		auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(editorID);
		if (!global) {
			Log::WARN("PerkCurrency: Currency global '{}' not resolved. Nothing deducted. Is po3 Tweaks installed?", editorID);
			return;
		}
		const float before = global->value;
		global->value = std::max(0.0f, before - 1.0f);
		Log::INFO("PerkCurrency: '{}' (tree '{}') paid from global '{}': {} -> {}.", perkId, treeId, editorID, before, global->value);
	}

	RE::BSEventNotifyControl PerkCurrency::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		//Game state belongs on the main thread.
		if (a_event && a_event->eventName == PerksContract::ModEvent::Acquire) {
			SKSE::GetTaskInterface()->AddTask([payload = ModEvents::StrArg(*a_event)] { Deduct(payload); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
