#include "SkillSheetFeed.h"
#include "SkillData.h"
#include "SkillSheetContract.h"
#include "Papyrus/ModEvents.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <algorithm>
#include <chrono>

namespace Skills {

	namespace {
		bool GamepadActive() {
			auto* input = RE::BSInputDeviceManager::GetSingleton();
			return input && input->IsGamepadEnabled();
		}
	}

	void SkillSheetFeed::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("SkillSheetFeed: No mod-event source. Skill sheet requests will not be heard.");
			return;
		}
		Log::INFO("SkillSheetFeed: Registered.");
	}

	void SkillSheetFeed::PushSheet(const std::string& request) {
		const auto start = std::chrono::steady_clock::now();

		const auto callback = MenuCallback::Open(request, "SkillSheetFeed (sheet)");
		if (!callback) {
			return;
		}
		const auto readings = SkillData::ReadAll("SkillSheetFeed");
		if (!readings) {
			return;
		}
		auto* movie = callback->movie.get();

		RE::GFxValue skills;
		movie->CreateArray(&skills);
		for (const auto& skill : *readings) {
			RE::GFxValue row;
			movie->CreateObject(&row);
			row.SetMember("av", RE::GFxValue(static_cast<int>(skill.actorValue)));
			row.SetMember("effective", RE::GFxValue(skill.current));
			row.SetMember("base", RE::GFxValue(skill.base));
			row.SetMember("percent", RE::GFxValue(std::clamp(skill.progress, 0.0f, SkillSheetContract::MaxPercent)));
			skills.PushBack(row);
		}

		const bool gamepad = GamepadActive();
		RE::GFxValue sheet;
		movie->CreateObject(&sheet);
		sheet.SetMember("gamepad", RE::GFxValue(gamepad));
		sheet.SetMember("skills", skills);
		callback->Invoke(sheet);

		Log::INFO("SkillSheetFeed: {} skills (gamepad {}) -> {} in {} us.", readings->size(), gamepad, callback->target, ScaleformUtility::MicrosecondsSince(start));
	}

	void SkillSheetFeed::PushMastery(const std::string& request, int actorValue) {
		const auto start = std::chrono::steady_clock::now();

		const auto callback = MenuCallback::Open(request, "SkillSheetFeed (mastery)");
		if (!callback) {
			return;
		}
		auto* movie = callback->movie.get();

		RE::GFxValue ranks;
		movie->CreateArray(&ranks);
		const auto index = SkillData::IndexOf(static_cast<RE::ActorValue>(actorValue));
		if (!index) {
			Log::WARN("SkillSheetFeed: Actor value {} is not one of the 18 skills.", actorValue);
		}
		else if (const auto perks = SkillData::MasteryPerks(*index, "SkillSheetFeed")) {
			for (const auto& perk : *perks) {
				RE::GFxValue rank;
				movie->CreateObject(&rank);
				rank.SetMember("name", RE::GFxValue(perk.name.c_str()));
				rank.SetMember("text", RE::GFxValue(perk.description.c_str()));
				rank.SetMember("owned", RE::GFxValue(perk.owned));
				ranks.PushBack(rank);
			}
		}

		RE::GFxValue info;
		movie->CreateObject(&info);
		info.SetMember("av", RE::GFxValue(actorValue));
		info.SetMember("ranks", ranks);
		callback->Invoke(info);

		Log::INFO("SkillSheetFeed: {} mastery rank(s) for actor value {} -> {} in {} us.", ranks.GetArraySize(), actorValue, callback->target, ScaleformUtility::MicrosecondsSince(start));
	}

	RE::BSEventNotifyControl SkillSheetFeed::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		//UI tasks, not AddTask: the sheet is a custom menu, which pauses the game,
		//and answers queued on the general task list crashed custom menus while they opened.
		if (a_event->eventName == SkillSheetContract::ModEvent::RequestSheet) {
			SKSE::GetTaskInterface()->AddUITask([request = ModEvents::StrArg(*a_event)] { PushSheet(request); });
		}
		else if (a_event->eventName == SkillSheetContract::ModEvent::RequestMastery) {
			const int actorValue = static_cast<int>(a_event->numArg);
			SKSE::GetTaskInterface()->AddUITask([request = ModEvents::StrArg(*a_event), actorValue] { PushMastery(request, actorValue); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
