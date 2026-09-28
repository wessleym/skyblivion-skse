#include "PerksFeed.h"
#include "PerkDataStore.h"
#include "PerksContract.h"
#include "Papyrus/ModEvents.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <algorithm>
#include <chrono>
#include <unordered_set>

namespace PerksView {

	namespace {
		const char* CurrencyTypeName(CurrencyType type) {
			return type == CurrencyType::kGlobalVariable ? "globalVariable" : "perkPoints";
		}

		//1 while the global is non-zero, else 0. A global that does not resolve reads as hidden,
		//logged once per session per editor ID.
		double VisibilityValue(const std::string& editorID) {
			static std::unordered_set<std::string> reported;
			auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(editorID);
			if (!global) {
				if (reported.insert(editorID).second) {
					Log::WARN("PerksFeed: Visibility global '{}' not resolved. Hidden. Is po3 Tweaks installed?", editorID);
				}
				return 0.0;
			}
			return global->value != 0.0f ? 1.0 : 0.0;
		}

		//The highest contiguous rank held, walking ranks in order with HasPerk.
		//A rank record that does not resolve stops the walk, logged once per session.
		int OwnedRank(RE::PlayerCharacter* player, const Perk& perk) {
			static std::unordered_set<std::string> reported;
			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			int owned = 0;
			for (const auto& rank : perk.ranks) {
				auto* record = dataHandler ? dataHandler->LookupForm<RE::BGSPerk>(rank.rankId.formID, rank.rankId.plugin) : nullptr;
				if (!record) {
					if (reported.insert(rank.rankId.raw).second) {
						Log::WARN("PerksFeed: Rank '{}' of perk '{}' did not resolve. Treated as not owned.", rank.rankId.raw, perk.id);
					}
					break;
				}
				if (!player->HasPerk(record)) {
					break;
				}
				++owned;
			}
			return owned;
		}

		RE::GFxValue RequirementObject(RE::GFxMovieView& movie, const Requirement& requirement) {
			RE::GFxValue object;
			movie.CreateObject(&object);
			object.SetMember("type", RE::GFxValue(requirement.rawType.c_str()));
			object.SetMember("value", RE::GFxValue(requirement.value));
			object.SetMember("name", RE::GFxValue(requirement.name.c_str()));
			object.SetMember("perkId", RE::GFxValue(requirement.perkId.c_str()));
			object.SetMember("rank", RE::GFxValue(static_cast<double>(requirement.rank)));
			return object;
		}

		RE::GFxValue PerkObject(RE::GFxMovieView& movie, const Perk& perk) {
			RE::GFxValue object;
			movie.CreateObject(&object);
			object.SetMember("id", RE::GFxValue(perk.id.c_str()));
			object.SetMember("name", RE::GFxValue(perk.name.c_str()));
			object.SetMember("icon", RE::GFxValue(perk.icon.c_str()));
			object.SetMember("x", RE::GFxValue(perk.x));
			object.SetMember("y", RE::GFxValue(perk.y));
			object.SetMember("flavourText", RE::GFxValue(perk.flavourText.c_str()));

			RE::GFxValue prerequisites;
			movie.CreateArray(&prerequisites);
			for (const auto& prerequisite : perk.prerequisites) {
				prerequisites.PushBack(RE::GFxValue(prerequisite.c_str()));
			}
			object.SetMember("prerequisites", prerequisites);

			RE::GFxValue ranks;
			movie.CreateArray(&ranks);
			for (const auto& rank : perk.ranks) {
				RE::GFxValue rankObject;
				movie.CreateObject(&rankObject);
				rankObject.SetMember("rankId", RE::GFxValue(rank.rankId.raw.c_str()));
				rankObject.SetMember("description", RE::GFxValue(rank.description.c_str()));
				RE::GFxValue requirements;
				movie.CreateArray(&requirements);
				for (const auto& requirement : rank.requirements) {
					requirements.PushBack(RequirementObject(movie, requirement));
				}
				rankObject.SetMember("requirements", requirements);
				ranks.PushBack(rankObject);
			}
			object.SetMember("ranks", ranks);
			return object;
		}
	}

	void PerksFeed::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("PerksFeed: No mod-event source. Perk menu requests will not be heard.");
			return;
		}
		Log::INFO("PerksFeed: Registered.");
	}

	void PerksFeed::PushTrees(const std::string& request) {
		const auto start = std::chrono::steady_clock::now();

		const auto feed = FeedRequest::Open(request, FeedRequest::Mode::Optional, "PerksFeed (trees)");
		if (!feed) {
			return;
		}
		auto& movie = *feed->movie;
		PerkDataStore::LoadOnce();

		RE::GFxValue trees;
		movie.CreateArray(&trees);
		std::size_t perkCount = 0;
		for (const auto& tree : PerkDataStore::Trees()) {
			RE::GFxValue treeObject;
			movie.CreateObject(&treeObject);
			treeObject.SetMember("id", RE::GFxValue(tree.id.c_str()));
			treeObject.SetMember("displayName", RE::GFxValue(tree.displayName.c_str()));
			treeObject.SetMember("icon", RE::GFxValue(tree.icon.c_str()));
			treeObject.SetMember("backgroundArt", RE::GFxValue(tree.backgroundArt.c_str()));
			treeObject.SetMember("themeColor", RE::GFxValue(tree.themeColor.c_str()));
			treeObject.SetMember("attribute", RE::GFxValue(tree.attribute.c_str()));
			treeObject.SetMember("currencyType", RE::GFxValue(CurrencyTypeName(tree.currencyType)));
			treeObject.SetMember("currencyLabel", RE::GFxValue(tree.currencyLabel.c_str()));
			treeObject.SetMember("globalEditorId", RE::GFxValue(tree.globalEditorId.c_str()));

			RE::GFxValue perks;
			movie.CreateArray(&perks);
			for (const auto& perk : tree.perks) {
				perks.PushBack(PerkObject(movie, perk));
				++perkCount;
			}
			treeObject.SetMember("perks", perks);
			trees.PushBack(treeObject);
		}

		if (!feed->Push(trees)) {
			return;
		}
		Log::INFO("PerksFeed: {} tree(s), {} perk(s) -> {} in {} us.",
			PerkDataStore::Trees().size(), perkCount, feed->target, ScaleformUtility::MicrosecondsSince(start));
	}

	void PerksFeed::PushState(const std::string& request) {
		const auto start = std::chrono::steady_clock::now();

		const auto feed = FeedRequest::Open(request, FeedRequest::Mode::Optional, "PerksFeed (state)");
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!feed || !player) {
			return;
		}
		auto& movie = *feed->movie;
		PerkDataStore::LoadOnce();
		const auto& trees = PerkDataStore::Trees();

		RE::GFxValue state;
		movie.CreateObject(&state);
		state.SetMember("level", RE::GFxValue(static_cast<double>(player->GetLevel())));

		//Each tree's balance in its own currency.
		RE::GFxValue currency;
		movie.CreateObject(&currency);
		const int perkPoints = static_cast<int>(REBridge::GameStats(player).perkCount);
		for (const auto& tree : trees) {
			double balance = 0.0;
			if (tree.currencyType == CurrencyType::kPerkPoints) {
				balance = static_cast<double>(perkPoints);
			}
			else if (auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(tree.globalEditorId)) {
				balance = static_cast<double>(global->value);
			}
			else {
				Log::WARN("PerksFeed: Currency global '{}' of tree '{}' not resolved. Balance 0. Is po3 Tweaks installed?",
					tree.globalEditorId, tree.id);
			}
			currency.SetMember(tree.id.c_str(), RE::GFxValue(balance));
		}
		state.SetMember("currency", currency);

		//Every actor value a tree attribute or an actorValue requirement names, read by name.
		std::unordered_set<std::string> actorValueNames;
		for (const auto& tree : trees) {
			if (!tree.attribute.empty()) {
				actorValueNames.insert(tree.attribute);
			}
			for (const auto& perk : tree.perks) {
				for (const auto& rank : perk.ranks) {
					for (const auto& requirement : rank.requirements) {
						if (requirement.type == RequirementType::kActorValue && !requirement.name.empty()) {
							actorValueNames.insert(requirement.name);
						}
					}
				}
			}
		}
		RE::GFxValue actorValues;
		movie.CreateObject(&actorValues);
		auto* actorValueList = RE::ActorValueList::GetSingleton();
		auto* actorValueOwner = REBridge::AVOwner(player);
		for (const auto& name : actorValueNames) {
			double value = 0.0;
			const auto actorValue = actorValueList ? actorValueList->LookupActorValueByName(name.c_str()) : RE::ActorValue::kNone;
			if (actorValue != RE::ActorValue::kNone && actorValueOwner) {
				value = static_cast<double>(actorValueOwner->GetActorValue(actorValue));
			}
			else {
				Log::WARN("PerksFeed: Actor value '{}' not resolved. Pushed as 0.", name);
			}
			actorValues.SetMember(name.c_str(), RE::GFxValue(value));
		}
		state.SetMember("avs", actorValues);

		RE::GFxValue perkRanks;
		RE::GFxValue pointsInTree;
		movie.CreateObject(&perkRanks);
		movie.CreateObject(&pointsInTree);
		for (const auto& tree : trees) {
			int total = 0;
			for (const auto& perk : tree.perks) {
				const int owned = OwnedRank(player, perk);
				perkRanks.SetMember(perk.id.c_str(), RE::GFxValue(static_cast<double>(owned)));
				total += owned;
			}
			pointsInTree.SetMember(tree.id.c_str(), RE::GFxValue(static_cast<double>(total)));
		}
		state.SetMember("perkRanks", perkRanks);
		state.SetMember("pointsInTree", pointsInTree);

		//Entries only for items with a visibilityGlobal; the SWF shows everything else.
		RE::GFxValue treeVisible;
		RE::GFxValue perkVisible;
		movie.CreateObject(&treeVisible);
		movie.CreateObject(&perkVisible);
		for (const auto& tree : trees) {
			if (!tree.visibilityGlobal.empty()) {
				treeVisible.SetMember(tree.id.c_str(), RE::GFxValue(VisibilityValue(tree.visibilityGlobal)));
			}
			for (const auto& perk : tree.perks) {
				if (!perk.visibilityGlobal.empty()) {
					perkVisible.SetMember(perk.id.c_str(), RE::GFxValue(VisibilityValue(perk.visibilityGlobal)));
				}
			}
		}
		state.SetMember("treeVisible", treeVisible);
		state.SetMember("perkVisible", perkVisible);

		if (!feed->Push(state)) {
			return;
		}
		Log::INFO("PerksFeed: State (level {}, {} perk point(s), {} tree(s), {} actor value(s)) -> {} in {} us.",
			player->GetLevel(), perkPoints, trees.size(), actorValueNames.size(), feed->target,
			ScaleformUtility::MicrosecondsSince(start));
	}

	RE::BSEventNotifyControl PerksFeed::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		if (a_event->eventName == PerksContract::ModEvent::RequestTrees) {
			SKSE::GetTaskInterface()->AddTask([request = ModEvents::StrArg(*a_event)] { PushTrees(request); });
		}
		else if (a_event->eventName == PerksContract::ModEvent::RequestState) {
			SKSE::GetTaskInterface()->AddTask([request = ModEvents::StrArg(*a_event)] { PushState(request); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
