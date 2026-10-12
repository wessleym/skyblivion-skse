#include "StartMenuService.h"
#include "StartMenuContract.h"
#include "Papyrus/ModEvents.h"
#include "Settings/SettingsContract.h"
#include "Settings/SettingsService.h"
#include "SettingsMenu/SettingsStore.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <algorithm>
#include <array>
#include <format>
#include <string>

namespace StartMenu {

	namespace {
		RE::GPtr<RE::GFxMovieView> MainMenuMovie() {
			return ScaleformUtility::OpenMenuMovie(RE::MainMenu::MENU_NAME);
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

	void StartMenuService::RepairInputAfterMenus() {
#if SKY_COMMONLIB == SKY_COMMONLIB_NG
		auto* ui = RE::UI::GetSingleton();
		if (!ui || !ui->modal) {
			return;
		}
		for (const auto& menu : ui->menuStack) {
			if (menu && menu->menuFlags.all(RE::UI_MENU_FLAGS::kModal)) {
				return;
			}
		}
		ui->modal = false;
		Log::INFO("StartMenuService: UI.modal was set with no modal menu on the stack. Cleared.");
#endif
	}

	void StartMenuService::PushOptions() {
		auto movie = MainMenuMovie();
		if (!movie) {
			Log::INFO("StartMenuService: Options requested, but the Main Menu is not open.");
			return;
		}
		RE::GFxValue options;
		movie->CreateArray(&options);
		//one row per settings-menu category with a newgame label (Categories.csv): its presets, led by
		//"Current Settings" only when the new-game values match none of them (the player changed something);
		//otherwise the row starts on the preset they match. Nothing changes until a different choice is made.
		using SKYBSettingsMenu::kCategories;
		using SKYBSettingsMenu::kPresetCount;
		using SKYBSettingsMenu::kPresetLabels;
		auto* store = SKYBSettingsMenu::Store::GetSingleton();
		s_pending.assign(kCategories.size(), -1);
		s_hasCurrent.assign(kCategories.size(), false);
		int presetRows = 0;
		for (std::size_t c = 0; c < kCategories.size(); ++c) {
			const auto& cat = kCategories[c];
			if (!cat.newgame || !*cat.newgame) {
				continue;
			}
			const int matched = store->PresetOf(cat.id);
			const bool custom = matched < 0;
			s_hasCurrent[c] = custom;
			RE::GFxValue option;
			movie->CreateObject(&option);
			const auto key = std::string(StartMenuContract::Option::PresetPrefix) + cat.id;
			const auto desc = std::string(cat.newgame) + "_desc";
			option.SetMember("key", RE::GFxValue(key.c_str()));
			option.SetMember("label", RE::GFxValue(cat.newgame));
			option.SetMember("desc", RE::GFxValue(desc.c_str()));
			option.SetMember("groups", RE::GFxValue("newgame"));
			RE::GFxValue values;
			movie->CreateArray(&values);
			if (custom) {
				values.PushBack(RE::GFxValue("$SKYB_start_preset_current"));
			}
			for (std::size_t p = 0; p < kPresetCount; ++p) {
				values.PushBack(RE::GFxValue(kPresetLabels[p]));
			}
			option.SetMember("values", values);
			const double index = custom ? 0.0 : static_cast<double>(matched);
			option.SetMember("index", RE::GFxValue(index));
			option.SetMember("defaultIndex", RE::GFxValue(index));
			options.PushBack(option);
			++presetRows;
		}
		//the achievement pop-up and scope rows are not offered here: they are in
		//Settings > Interface > Skyblivion; a choice sent for them is still recorded (ApplyOption)

		const bool delivered = movie->Invoke(StartMenuContract::AsPath::SetOptions, nullptr, &options, 1);
		Log::INFO("StartMenuService: Pushed {} preset row(s), delivered={}.", presetRows, delivered);
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
			return;
		}
		//the presets wait for the new game (OnNewGame): nothing is written until it starts
		if (key.starts_with(StartMenuContract::Option::PresetPrefix)) {
			const auto id = key.substr(std::string_view(StartMenuContract::Option::PresetPrefix).size());
			const auto& cats = SKYBSettingsMenu::kCategories;
			for (std::size_t c = 0; c < cats.size() && c < s_pending.size(); ++c) {
				if (id == cats[c].id) {
					const int preset = s_hasCurrent[c] ? index - 1 : index;
					s_pending[c] = std::clamp(preset, -1, static_cast<int>(SKYBSettingsMenu::kPresetCount) - 1);
					Log::INFO("StartMenuService: new game's {} -> {}.", id,
						s_pending[c] < 0 ? "current settings" : SKYBSettingsMenu::kPresetIds[s_pending[c]]);
					return;
				}
			}
			Log::WARN("StartMenuService: no settings category '{}'.", id);
			return;
		}
		Log::INFO("StartMenuService: Option '{}' -> {} (recorded only).", key, index);
		//Unknown options are recorded under [NewGame].
		SettingsService::SetInt(Keys::NewGameSection, key, index);
	}

	void StartMenuService::OnNewGame() {
		auto* store = SKYBSettingsMenu::Store::GetSingleton();
		const auto& cats = SKYBSettingsMenu::kCategories;
		for (std::size_t c = 0; c < cats.size() && c < s_pending.size(); ++c) {
			if (s_pending[c] >= 0) {
				Log::INFO("StartMenuService: new game takes the {} preset {}.", cats[c].id,
					SKYBSettingsMenu::kPresetIds[s_pending[c]]);
				store->ApplyPresetToCharacter(cats[c].id, static_cast<std::size_t>(s_pending[c]));
			}
		}
		s_pending.assign(cats.size(), -1);
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
