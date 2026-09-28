#include "StartMenuService.h"
#include "StartMenuContract.h"
#include "Papyrus/ModEvents.h"
#include "Settings/SettingsContract.h"
#include "Settings/SettingsService.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <algorithm>
#include <array>

namespace StartMenu {

	namespace {
		//The difficulty ladder the settings menu offers, in iDifficulty order.
		constexpr std::array<const char*, 6> kDifficultyKeys{
			"$Novice", "$Apprentice", "$Adept", "$Expert", "$Master", "$Legendary"
		};
		constexpr int kDefaultDifficulty = 2;

		RE::GPtr<RE::GFxMovieView> MainMenuMovie() {
			return ScaleformUtility::OpenMenuMovie(RE::MainMenu::MENU_NAME);
		}

		//The prefs collection first, where the game keeps it; the base collection as a fallback when prefs lacks it.
		RE::Setting* DifficultySetting() {
			if (auto* prefs = RE::INIPrefSettingCollection::GetSingleton()) {
				if (auto* setting = prefs->GetSetting("iDifficulty:GamePlay")) {
					return setting;
				}
			}
			if (auto* baseSettings = RE::INISettingCollection::GetSingleton()) {
				return baseSettings->GetSetting("iDifficulty:GamePlay");
			}
			return nullptr;
		}

		int ClampDifficulty(int index) {
			return std::clamp(index, 0, static_cast<int>(kDifficultyKeys.size()) - 1);
		}

		int CurrentDifficulty() {
			auto* setting = DifficultySetting();
			return setting ? ClampDifficulty(setting->data.i) : kDefaultDifficulty;
		}

		//One option row. values: translation keys, one per choice.
		RE::GFxValue OptionObject(RE::GFxMovieView& movie, const char* key, const char* label, const char* description,
			const char* groups, std::initializer_list<const char*> values, int index, int defaultIndex) {
			RE::GFxValue option;
			movie.CreateObject(&option);
			option.SetMember("key", RE::GFxValue(key));
			option.SetMember("label", RE::GFxValue(label));
			option.SetMember("desc", RE::GFxValue(description));
			option.SetMember("groups", RE::GFxValue(groups));
			RE::GFxValue valueArray;
			movie.CreateArray(&valueArray);
			for (const char* value : values) {
				valueArray.PushBack(RE::GFxValue(value));
			}
			option.SetMember("values", valueArray);
			option.SetMember("index", RE::GFxValue(static_cast<double>(index)));
			option.SetMember("defaultIndex", RE::GFxValue(static_cast<double>(defaultIndex)));
			return option;
		}

		const char* OrEmpty(const RE::BSFixedString& text) {
			return text.c_str() ? text.c_str() : "";
		}
	}

	void StartMenuService::Register() {
		if (!ModEvents::AddSink(&s_sink)) {
			Log::WARN("StartMenuService: No mod-event source. Title screen requests will not be heard.");
			return;
		}
		Log::INFO("StartMenuService: Registered.");
	}

	void StartMenuService::PushOptions() {
		auto movie = MainMenuMovie();
		if (!movie) {
			Log::INFO("StartMenuService: Options requested, but the Main Menu is not open.");
			return;
		}
		using Settings::SettingsService;
		namespace Keys = Settings::SettingsContract;

		const int difficulty = CurrentDifficulty();
		const int toasts = SettingsService::GetInt(Keys::AchievementsSection, Keys::ToastKey, 1) ? 1 : 0;
		const int shared = SettingsService::GetInt(Keys::AchievementsSection, Keys::SharedKey, 0) ? 1 : 0;

		RE::GFxValue options;
		movie->CreateArray(&options);
		options.PushBack(OptionObject(*movie, StartMenuContract::Option::Difficulty,
			"$SKYB_start_difficulty", "$SKYB_start_difficulty_desc", "newgame,settings",
			{ kDifficultyKeys[0], kDifficultyKeys[1], kDifficultyKeys[2], kDifficultyKeys[3], kDifficultyKeys[4], kDifficultyKeys[5] },
			difficulty, difficulty));
		options.PushBack(OptionObject(*movie, StartMenuContract::Option::ToastNotifications,
			"$SKYB_start_ach_toasts", "$SKYB_start_ach_toasts_desc", "newgame",
			{ "$SKYB_start_off", "$SKYB_start_on" }, toasts, 1));
		options.PushBack(OptionObject(*movie, StartMenuContract::Option::AchievementScope,
			"$SKYB_start_ach_scope", "$SKYB_start_ach_scope_desc", "newgame",
			{ "$SKYB_start_ach_scope_character", "$SKYB_start_ach_scope_shared" }, shared, 0));

		const bool delivered = movie->Invoke(StartMenuContract::AsPath::SetOptions, nullptr, &options, 1);
		Log::INFO("StartMenuService: Pushed 3 options (difficulty={}, toasts={}, shared={}), delivered={}.",
			difficulty, toasts, shared, delivered);
	}

	void StartMenuService::PushLocations() {
		auto movie = MainMenuMovie();
		auto* saveLoadManager = RE::BGSSaveLoadManager::GetSingleton();
		if (!movie || !saveLoadManager) {
			return;
		}
		RE::GFxValue saves;
		movie->CreateArray(&saves);
		std::size_t count = 0;
		for (auto* entry : saveLoadManager->saveGameList) {
			if (!entry) {
				continue;
			}
			RE::GFxValue save;
			movie->CreateObject(&save);
			save.SetMember("name", RE::GFxValue(OrEmpty(entry->characterName)));
			save.SetMember("playTime", RE::GFxValue(OrEmpty(entry->playTime)));
			save.SetMember("location", RE::GFxValue(OrEmpty(entry->currentLocation)));
			save.SetMember("file", RE::GFxValue(OrEmpty(entry->fileName)));
			saves.PushBack(save);
			++count;
		}
		const bool delivered = movie->Invoke(StartMenuContract::AsPath::SetSaveLocations, nullptr, &saves, 1);
		Log::INFO("StartMenuService: Pushed {} save location(s), delivered={}.", count, delivered);
	}

	void StartMenuService::ApplyOption(const std::string& key, int index) {
		using Settings::SettingsService;
		namespace Keys = Settings::SettingsContract;

		if (key == StartMenuContract::Option::AchievementScope) {
			SettingsService::SetInt(Keys::AchievementsSection, Keys::SharedKey, index ? 1 : 0);
			return;
		}
		if (key == StartMenuContract::Option::ToastNotifications) {
			SettingsService::SetInt(Keys::AchievementsSection, Keys::ToastKey, index ? 1 : 0);
			SettingsService::ApplyToGame();
			return;
		}
		if (key == StartMenuContract::Option::Difficulty) {
			if (auto* setting = DifficultySetting()) {
				setting->data.i = ClampDifficulty(index);
				if (auto* prefs = RE::INIPrefSettingCollection::GetSingleton()) {
					prefs->WriteSetting(setting);
				}
				Log::INFO("StartMenuService: Difficulty -> {}.", setting->data.i);
			}
			else {
				Log::WARN("StartMenuService: iDifficulty:GamePlay not found. The difficulty choice is ignored.");
			}
		}
		else {
			Log::INFO("StartMenuService: Option '{}' -> {} (recorded only).", key, index);
		}
		//Difficulty and unknown options are also recorded under [NewGame].
		SettingsService::SetInt(Keys::NewGameSection, key, index);
	}

	RE::BSEventNotifyControl StartMenuService::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		if (a_event->eventName == StartMenuContract::ModEvent::RequestOptions) {
			PushOptions();
		}
		else if (a_event->eventName == StartMenuContract::ModEvent::RequestLocations) {
			PushLocations();
		}
		else if (a_event->eventName == StartMenuContract::ModEvent::SetOption) {
			ApplyOption(ModEvents::StrArg(*a_event), static_cast<int>(a_event->numArg));
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
