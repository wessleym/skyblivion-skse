#include "AchievementJournal.h"
#include "AchievementJournalContract.h"
#include "Achievements/AchievementStore.h"
#include "Papyrus/ModEvents.h"
#include "Settings/SettingsContract.h"
#include "Settings/SettingsService.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <string_view>

namespace Achievements {

	namespace {
		//Flips every row whose key the record holds to unlocked, and orders unlocked rows first.
		//Returns the rewritten CSV; flipped counts the rows the record changed.
		std::string MergeRecord(const std::string& csv, int& flipped) {
			const auto recorded = AchievementStore::UnlockedKeys();
			std::string unlocked;
			std::string locked;
			for (auto& row : ModEvents::Split(csv, ';')) {
				const auto separator = row.find('|');
				if (separator == std::string::npos || row.empty()) {
					continue;
				}
				//A row with nothing after the separator reads as locked; append the flag so setting it writes within the string.
				if (row.size() == separator + 1) {
					row += '0';
				}
				bool isUnlocked = row[separator + 1] == '1';
				if (!isUnlocked && recorded.contains(row.substr(0, separator))) {
					row[separator + 1] = '1';
					isUnlocked = true;
					++flipped;
				}
				auto& block = isUnlocked ? unlocked : locked;
				if (!block.empty()) {
					block += ';';
				}
				block += row;
			}
			if (!locked.empty()) {
				unlocked += (unlocked.empty() ? "" : ";") + locked;
			}
			return unlocked;
		}
	}

	void AchievementJournal::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("AchievementJournal: No mod-event source. The journal's achievements tab will not be pushed.");
			return;
		}
		Log::INFO("AchievementJournal: Registered.");
	}

	void AchievementJournal::Push(const std::string& csv, bool notify) {
		auto movie = ScaleformUtility::OpenMenuMovie(AchievementJournalContract::JournalMenuName);
		if (!movie) {
			return;
		}
		const bool shared = Settings::SettingsService::GetInt(Settings::SettingsContract::AchievementsSection,
			Settings::SettingsContract::SharedKey, 0) != 0;
		int flipped = 0;
		const std::string rows = shared ? MergeRecord(csv, flipped) : csv;

		RE::GFxValue category;
		movie->CreateArray(&category);
		category.PushBack(RE::GFxValue(AchievementJournalContract::Header));
		category.PushBack(RE::GFxValue(rows.c_str()));
		category.PushBack(RE::GFxValue(notify ? "1" : "0"));
		const bool delivered = movie->Invoke(AchievementJournalContract::AsPath::AddCategory, nullptr, &category, 1);
		Log::INFO("AchievementJournal: Pushed ({}, {} flipped by the record), delivered={}.",
			shared ? "shared" : "per character", flipped, delivered);
	}

	RE::BSEventNotifyControl AchievementJournal::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (a_event && a_event->eventName == AchievementJournalContract::ModEvent::Show) {
			SKSE::GetTaskInterface()->AddUITask([csv = ModEvents::StrArg(*a_event), notify = a_event->numArg != 0.0f] {
				Push(csv, notify);
			});
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
