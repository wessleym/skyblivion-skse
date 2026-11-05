#include "HelpFeed.h"
#include "HelpContract.h"
#include "HelpDataStore.h"
#include "Platform/FormCache.h"
#include "Platform/MainThread.h"
#include "Papyrus/ModEvents.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <chrono>

namespace Help {

	namespace {
		//True with no global named; else while the global is non-zero. A global that does not resolve reads as
		//hidden (the form table logs the miss once).
		bool Visible(const std::string& editorID) {
			if (editorID.empty()) {
				return true;
			}
			auto* global = FormCache::Get<RE::TESGlobal>(editorID, "HelpFeed (visibility)");
			return global && global->value != 0.0f;
		}
	}

	void HelpFeed::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("HelpFeed: No mod-event source. Help page requests will not be heard.");
			return;
		}
		Log::INFO("HelpFeed: Registered.");
	}

	void HelpFeed::PushHelp(const std::string& request) {
		const auto start = std::chrono::steady_clock::now();

		const auto feed = FeedRequest::Open(request, FeedRequest::Mode::Optional, "HelpFeed");
		if (!feed) {
			return;
		}
		auto& movie = *feed->movie;
		HelpDataStore::LoadOnce();

		RE::GFxValue categories;
		movie.CreateArray(&categories);
		std::size_t categoryCount = 0;
		std::size_t topicCount = 0;
		for (const auto& category : HelpDataStore::Categories()) {
			if (!Visible(category.visibilityGlobal)) {
				continue;
			}
			RE::GFxValue topics;
			movie.CreateArray(&topics);
			std::size_t shown = 0;
			for (const auto& topic : category.topics) {
				if (!Visible(topic.visibilityGlobal)) {
					continue;
				}
				RE::GFxValue topicObject;
				movie.CreateObject(&topicObject);
				topicObject.SetMember("id", RE::GFxValue(topic.id.c_str()));
				topicObject.SetMember("title", RE::GFxValue(topic.title.c_str()));
				topicObject.SetMember("body", RE::GFxValue(topic.body.c_str()));
				topics.PushBack(topicObject);
				++shown;
			}
			if (shown == 0) {
				continue;
			}
			RE::GFxValue categoryObject;
			movie.CreateObject(&categoryObject);
			categoryObject.SetMember("id", RE::GFxValue(category.id.c_str()));
			categoryObject.SetMember("name", RE::GFxValue(category.displayName.c_str()));
			categoryObject.SetMember("topics", topics);
			categories.PushBack(categoryObject);
			++categoryCount;
			topicCount += shown;
		}

		if (!feed->Push(categories)) {
			return;
		}
		Log::INFO("HelpFeed: {} categor(ies), {} topic(s) -> {} in {} us.", categoryCount, topicCount, feed->target,
			ScaleformUtility::MicrosecondsSince(start));
	}

	RE::BSEventNotifyControl HelpFeed::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (a_event && a_event->eventName == HelpContract::ModEvent::RequestHelp) {
			MainThread::Post([request = ModEvents::StrArg(*a_event)] { PushHelp(request); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
