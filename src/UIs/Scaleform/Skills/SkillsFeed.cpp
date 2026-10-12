#include "SkillsFeed.h"
#include "SkillData.h"
#include "SkillsContract.h"
#include "Papyrus/ModEvents.h"
#include "Platform/MainThread.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <chrono>

namespace Skills {

	void SkillsFeed::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("SkillsFeed: No mod-event source. Skills menu requests will not be heard.");
			return;
		}
		Log::INFO("SkillsFeed: Registered.");
	}

	void SkillsFeed::PushSkillData(const std::string& request) {
		const auto start = std::chrono::steady_clock::now();

		const auto feed = FeedRequest::Open(request, FeedRequest::Mode::Required, "SkillsFeed (skill data)");
		if (!feed) {
			return;
		}
		const auto readings = SkillData::ReadAll("SkillsFeed");
		if (!readings) {
			return;
		}
		auto* movie = feed->movie.get();

		RE::GFxValue rows;
		movie->CreateArray(&rows);
		for (const auto& skill : *readings) {
			RE::GFxValue row;
			movie->CreateObject(&row);
			row.SetMember("current", RE::GFxValue(skill.current));
			row.SetMember("base", RE::GFxValue(skill.base));
			row.SetMember("maximum", RE::GFxValue(skill.permanent));
			row.SetMember("xp", RE::GFxValue(skill.xp));
			row.SetMember("levelThreshold", RE::GFxValue(skill.levelThreshold));
			row.SetMember("progress", RE::GFxValue(skill.progress));
			rows.PushBack(row);
		}

		if (!feed->Push(rows)) {
			return;
		}
		Log::INFO("SkillsFeed: {} skill rows -> {} ({}) in {} us.", readings->size(), feed->target, feed->mode, ScaleformUtility::MicrosecondsSince(start));
	}

	void SkillsFeed::PushMasteryPerks(const std::string& request, int skillIndex) {
		const auto start = std::chrono::steady_clock::now();

		const auto feed = FeedRequest::Open(request, FeedRequest::Mode::Ignored, "SkillsFeed (mastery perks)");
		if (!feed || !RE::PlayerCharacter::GetSingleton()) {
			return;
		}
		if (skillIndex < 0) {
			Log::WARN("SkillsFeed: Skill index {} out of range.", skillIndex);
			return;
		}
		const auto perks = SkillData::MasteryPerks(static_cast<std::size_t>(skillIndex), "SkillsFeed");
		if (!perks) {
			return;
		}
		auto* movie = feed->movie.get();

		RE::GFxValue rows;
		movie->CreateArray(&rows);
		for (const auto& perk : *perks) {
			RE::GFxValue row;
			movie->CreateObject(&row);
			row.SetMember("fullName", RE::GFxValue(perk.name.c_str()));
			row.SetMember("description", RE::GFxValue(perk.description.c_str()));
			row.SetMember("hasPerk", RE::GFxValue(perk.owned));
			rows.PushBack(row);
		}

		feed->Push(rows);
		Log::INFO("SkillsFeed: {} mastery perk row(s) for skill {} -> {} in {} us.", perks->size(), skillIndex, feed->target, ScaleformUtility::MicrosecondsSince(start));
	}

	RE::BSEventNotifyControl SkillsFeed::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		if (a_event->eventName == SkillsContract::ModEvent::RequestSkillData) {
			MainThread::Post([request = ModEvents::StrArg(*a_event)] { PushSkillData(request); });
		}
		else if (a_event->eventName == SkillsContract::ModEvent::RequestMasteryPerks) {
			const int skillIndex = static_cast<int>(a_event->numArg);
			MainThread::Post([request = ModEvents::StrArg(*a_event), skillIndex] { PushMasteryPerks(request, skillIndex); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
