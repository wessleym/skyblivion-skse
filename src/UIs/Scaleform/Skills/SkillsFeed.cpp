#include "SkillsFeed.h"
#include "SkillsContract.h"
#include "Papyrus/ModEvents.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <array>
#include <chrono>

namespace Skills {

	namespace {
		//The menu's fixed order: Oblivion's skill categories, alphabetical within each.
		//Matches the retired SkyblivionUI.dll and SKYBAttributeUtility.GetAllSkillStrings.
		constexpr std::array<RE::ActorValue, 18> kSkillOrder{
			RE::ActorValue::kArchery,     RE::ActorValue::kBlock,       RE::ActorValue::kHeavyArmor,
			RE::ActorValue::kOneHanded,   RE::ActorValue::kSmithing,    RE::ActorValue::kTwoHanded,
			RE::ActorValue::kAlteration,  RE::ActorValue::kConjuration, RE::ActorValue::kDestruction,
			RE::ActorValue::kEnchanting,  RE::ActorValue::kIllusion,    RE::ActorValue::kRestoration,
			RE::ActorValue::kAlchemy,     RE::ActorValue::kLightArmor,  RE::ActorValue::kLockpicking,
			RE::ActorValue::kPickpocket,  RE::ActorValue::kSneak,       RE::ActorValue::kSpeech,
		};
	}

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
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!feed || !player) {
			return;
		}
		auto* movie = feed->movie.get();
		auto* actorValues = REBridge::AVOwner(player);
		auto* playerSkills = REBridge::PlayerSkills(player);
		auto* skills = playerSkills ? playerSkills->data : nullptr;
		if (!actorValues || !skills) {
			Log::WARN("SkillsFeed: Player actor values or skill data unavailable.");
			return;
		}

		RE::GFxValue rows;
		movie->CreateArray(&rows);
		for (const auto actorValue : kSkillOrder) {
			//PlayerSkills holds one entry per skill, from One-Handed in actor value order.
			const auto& skill = skills->skills[static_cast<int>(actorValue) - static_cast<int>(RE::ActorValue::kOneHanded)];
			const float threshold = skill.levelThreshold;
			const float progress = threshold > 0.0f ? skill.xp / threshold * 100.0f : 0.0f;

			RE::GFxValue row;
			movie->CreateObject(&row);
			row.SetMember("current", RE::GFxValue(actorValues->GetActorValue(actorValue)));
			row.SetMember("base", RE::GFxValue(actorValues->GetBaseActorValue(actorValue)));
			row.SetMember("maximum", RE::GFxValue(actorValues->GetPermanentActorValue(actorValue)));
			row.SetMember("xp", RE::GFxValue(skill.xp));
			row.SetMember("levelThreshold", RE::GFxValue(threshold));
			row.SetMember("progress", RE::GFxValue(progress));
			rows.PushBack(row);
		}

		if (!feed->Push(rows)) {
			return;
		}
		Log::INFO("SkillsFeed: {} skill rows -> {} ({}) in {} us.", kSkillOrder.size(), feed->target, feed->mode, ScaleformUtility::MicrosecondsSince(start));
	}

	void SkillsFeed::PushMasteryPerks(const std::string& request, int skillIndex) {
		const auto start = std::chrono::steady_clock::now();

		const auto feed = FeedRequest::Open(request, FeedRequest::Mode::Ignored, "SkillsFeed (mastery perks)");
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!feed || !player) {
			return;
		}
		auto* movie = feed->movie.get();

		auto* skillPerkLists = RE::TESForm::LookupByEditorID<RE::BGSListForm>(SkillsContract::MasteryPerkListEditorID);
		if (!skillPerkLists) {
			Log::WARN("SkillsFeed: {} not resolved. Is po3 Tweaks installed?", SkillsContract::MasteryPerkListEditorID);
			return;
		}
		if (skillIndex < 0 || skillIndex >= static_cast<int>(skillPerkLists->forms.size())) {
			Log::WARN("SkillsFeed: Skill index {} out of range ({} inner lists).", skillIndex, skillPerkLists->forms.size());
			return;
		}
		auto* skillPerkListForm = skillPerkLists->forms[skillIndex];
		auto* skillPerkList = skillPerkListForm ? skillPerkListForm->As<RE::BGSListForm>() : nullptr;
		if (!skillPerkList) {
			Log::WARN("SkillsFeed: Entry {} of {} is not a FormList.", skillIndex, SkillsContract::MasteryPerkListEditorID);
			return;
		}

		RE::GFxValue rows;
		movie->CreateArray(&rows);
		int rowCount = 0;
		for (auto* form : skillPerkList->forms) {
			auto* perk = form ? form->As<RE::BGSPerk>() : nullptr;
			if (!perk) {
				continue;
			}
			RE::BSString description;
			//Null parent, as in the retired DLL, which resolved description tags with no parent form.
			perk->GetDescription(description, nullptr);
			const char* fullName = perk->GetFullName();

			RE::GFxValue row;
			movie->CreateObject(&row);
			row.SetMember("fullName", RE::GFxValue(fullName ? fullName : ""));
			row.SetMember("description", RE::GFxValue(description.c_str() ? description.c_str() : ""));
			row.SetMember("hasPerk", RE::GFxValue(player->HasPerk(perk)));
			rows.PushBack(row);
			++rowCount;
		}

		feed->Push(rows);
		Log::INFO("SkillsFeed: {} mastery perk row(s) for skill {} -> {} in {} us.", rowCount, skillIndex, feed->target, ScaleformUtility::MicrosecondsSince(start));
	}

	RE::BSEventNotifyControl SkillsFeed::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		if (a_event->eventName == SkillsContract::ModEvent::RequestSkillData) {
			SKSE::GetTaskInterface()->AddTask([request = ModEvents::StrArg(*a_event)] { PushSkillData(request); });
		}
		else if (a_event->eventName == SkillsContract::ModEvent::RequestMasteryPerks) {
			const int skillIndex = static_cast<int>(a_event->numArg);
			SKSE::GetTaskInterface()->AddTask([request = ModEvents::StrArg(*a_event), skillIndex] { PushMasteryPerks(request, skillIndex); });
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
