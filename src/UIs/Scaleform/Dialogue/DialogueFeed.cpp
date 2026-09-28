#include "DialogueFeed.h"
#include "DialogueContract.h"
#include "DialogueServices.h"
#include "Disposition/DispositionSystem.h"
#include "Papyrus/ModEvents.h"
#include "UIs/Scaleform/ScaleformUtility.h"
#include "UIs/UISystem.h"

#include <algorithm>
#include <chrono>

namespace Dialogue {

	namespace {
		//Editor ID first, since it survives a form renumber; the form key covers a load order without po3 Tweaks.
		//Cached after the first attempt: the form cannot change mid-session.
		template <class T>
		T* LookupCached(T*& cache, bool& attempted, const char* editorID, RE::FormID formID) {
			if (attempted) {
				return cache;
			}
			attempted = true;
			cache = RE::TESForm::LookupByEditorID<T>(editorID);
			if (!cache) {
				if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
					cache = dataHandler->LookupForm<T>(formID, DialogueContract::Forms::Plugin);
				}
			}
			if (!cache) {
				Log::WARN("DialogueFeed: {} not resolved.", editorID);
			}
			return cache;
		}

		RE::TESTopic* PersuasionTopic() {
			static RE::TESTopic* cache = nullptr;
			static bool attempted = false;
			return LookupCached(cache, attempted,
				DialogueContract::Forms::PersuasionTopicEditorID, DialogueContract::Forms::PersuasionTopicFormID);
		}

		RE::TESFaction* NoPersuadeFaction() {
			static RE::TESFaction* cache = nullptr;
			static bool attempted = false;
			return LookupCached(cache, attempted,
				DialogueContract::Forms::NoPersuadeFactionEditorID, DialogueContract::Forms::NoPersuadeFactionFormID);
		}

		//The current speaker, or the last one, which the menu still shows while a response plays.
		RE::Actor* DialogueSpeaker() {
			auto* topicManager = RE::MenuTopicManager::GetSingleton();
			if (!topicManager) {
				return nullptr;
			}
			auto handle = topicManager->speaker ? topicManager->speaker : topicManager->lastSpeaker;
			auto reference = handle.get();
			return reference ? reference->As<RE::Actor>() : nullptr;
		}

		//The persuasion topic's row in the list the game most recently gave the menu, or -1.
		int PersuasionTopicRow() {
			auto* topicManager = RE::MenuTopicManager::GetSingleton();
			auto* topic = PersuasionTopic();
			if (!topicManager || !topic || !topicManager->dialogueList) {
				return -1;
			}
			int row = 0;
			for (auto* entry : *topicManager->dialogueList) {
				if (entry && entry->parentTopic == topic) {
					return row;
				}
				++row;
			}
			return -1;
		}

		//Why persuasion is unavailable for a speaker: the persuasion TopicInfo's two conditions.
		//The faction is checked first, so a speaker who is both in it and in combat reports the faction,
		//which is also what hides the menu's disposition panel.
		enum class Refusal {
			None,
			NoPersuadeFaction,
			InCombat
		};

		Refusal PersuasionRefusal(RE::Actor* speaker) {
			if (auto* faction = NoPersuadeFaction(); faction && speaker->IsInFaction(faction)) {
				return Refusal::NoPersuadeFaction;
			}
			if (speaker->IsInCombat()) {
				return Refusal::InCombat;
			}
			return Refusal::None;
		}
	}

	void DialogueFeed::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("DialogueFeed: No mod-event source. Dialogue menu requests will not be heard.");
			return;
		}
		Log::INFO("DialogueFeed: Registered.");
	}

	void DialogueFeed::PushDialogueData(const std::string& request) {
		const auto start = std::chrono::steady_clock::now();

		const auto feed = FeedRequest::Open(request, FeedRequest::Mode::Ignored, "DialogueFeed");
		if (!feed) {
			return;
		}
		auto* movie = feed->movie.get();

		auto* speaker = DialogueSpeaker();
		int persuadeRow = -1;
		int disposition = 0;
		bool noPersuade = false;
		bool canPersuade = false;
		if (speaker) {
			persuadeRow = PersuasionTopicRow();
			//The value the persuasion view shows, clamped so the menu's meter can scale linearly.
			disposition = std::clamp(DispositionSystem::GetDispositionActorValue(speaker), 0, 100);
			const auto refusal = PersuasionRefusal(speaker);
			noPersuade = refusal == Refusal::NoPersuadeFaction;
			canPersuade = refusal == Refusal::None;
		}

		RE::GFxValue record;
		movie->CreateObject(&record);
		record.SetMember("hasSpeaker", RE::GFxValue(speaker != nullptr));
		record.SetMember("noPersuade", RE::GFxValue(noPersuade));
		record.SetMember("canPersuade", RE::GFxValue(canPersuade));
		record.SetMember("disposition", RE::GFxValue(static_cast<double>(disposition)));
		record.SetMember("persuadeRow", RE::GFxValue(static_cast<double>(persuadeRow)));
		const auto services = DialogueServices::AddToRecord(*movie, record, speaker);

		RE::GFxValue rows;
		movie->CreateArray(&rows);
		rows.PushBack(record);
		feed->Push(rows);

		Log::INFO("DialogueFeed: Speaker={} disposition={} noPersuade={} canPersuade={} persuadeRow={} services:{} -> {} in {} us.",
			speaker ? speaker->GetName() : "(none)", disposition, noPersuade, canPersuade, persuadeRow, services, feed->target,
			ScaleformUtility::MicrosecondsSince(start));
	}

	void DialogueFeed::OpenPersuasion() {
		auto* speaker = DialogueSpeaker();
		if (!speaker) {
			Log::WARN("DialogueFeed: Persuasion requested with no dialogue speaker.");
			return;
		}
		switch (PersuasionRefusal(speaker)) {
		case Refusal::NoPersuadeFaction:
			Log::INFO("DialogueFeed: Persuasion refused. {} is in {}.", speaker->GetName(), DialogueContract::Forms::NoPersuadeFactionEditorID);
			return;
		case Refusal::InCombat:
			Log::INFO("DialogueFeed: Persuasion refused. {} is in combat.", speaker->GetName());
			return;
		case Refusal::None:
			break;
		}
		Log::INFO("DialogueFeed: Opening persuasion with {}.", speaker->GetName());
		UISystem::OpenPersuasion(speaker);
	}

	RE::BSEventNotifyControl DialogueFeed::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		if (a_event->eventName == DialogueContract::ModEvent::RequestDialogueData) {
			SKSE::GetTaskInterface()->AddTask([request = ModEvents::StrArg(*a_event)] { PushDialogueData(request); });
		}
		else if (a_event->eventName == DialogueContract::ModEvent::OpenPersuasion) {
			SKSE::GetTaskInterface()->AddTask([] { OpenPersuasion(); });
		}
		else if (a_event->eventName == DialogueContract::ModEvent::OpenDialogueService) {
			SKSE::GetTaskInterface()->AddTask([serviceID = ModEvents::StrArg(*a_event)] {
				DialogueServices::Open(serviceID, DialogueSpeaker());
			});
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
