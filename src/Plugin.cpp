#include "Plugin.h"

#include "Achievements/AchievementStore.h"
#include "Degradation/DecayService.h"
#include "Degradation/InventoryHook.h"
#include "Degradation/RepairMenu.h"
#include "Degradation/Settings.h"
#include "Degradation/TemperHook.h"
#include "Disposition/DispositionSystem.h"
#include "Hotkeys/HotkeyService.h"
#include "Papyrus/SKSEScriptRegistrar.h"
#include "Settings/SettingsService.h"
#include "SettingsMenu/CombatService.h"
#include "SettingsMenu/EconomyService.h"
#include "SettingsMenu/InterfaceAdapters.h"
#include "SettingsMenu/SettingsMenu.h"
#include "SettingsMenu/SettingsStore.h"
#include "Sigil/SigilSystem.h"
#include "Storage/CoSave.h"
#include "UIs/Scaleform/StartMenu/StartMenuService.h"
#include "UIs/UISystem.h"

bool Plugin::OnLoad(const SKSE::LoadInterface* a_skse)
{
	auto logsFolder = SKSE::log::log_directory();
	if (!logsFolder) {
		Log::CRITICAL("!logsFolder");
		return false;
	}

	SKSE::Init(a_skse);

	Log::INFO("Skyblivion SKSE Plugin Loading...");

	if (a_skse->IsEditor()) {
		Log::CRITICAL("Loaded in editor (Creation Kit)");
		return false;
	}

	if (!SKSEScriptRegistrar::Initialize()) {
		return false;
	}

	DispositionSystem::Initialize();

	//Weapon and armor degradation (from SKYBDegradation.dll, 2026-10-05). The temper patches
	//install on Anniversary Edition runtimes only; elsewhere they log why and stay off.
	SKYBDeg::InventoryHook::Install();
	SKYBDeg::TemperHook::Install();

	UISystem::Initialize();

	//One co-save for the plugin: every store adds its record first (UISystem: spell making).
	SKYBSettingsMenu::Store::InstallSerialization();
	Storage::CoSave::Install();

	//SKSE::GetMessagingInterface()->RegisterListener only calls back to the first registered listener.
	auto OnMessage = [](SKSE::MessagingInterface::Message* msg) {
		switch (msg->type) {
		case SKSE::MessagingInterface::kPostLoad:
			UISystem::OnPostLoad();
			break;
		case SKSE::MessagingInterface::kInputLoaded:
			UISystem::OnInputLoaded();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			UISystem::OnGameStarting();
			break;
		case SKSE::MessagingInterface::kNewGame:
			UISystem::OnGameStarting();
			//The new game's globals exist a frame later.
			SKYBDeg::Settings::ReloadIfChanged();
			SKSE::GetTaskInterface()->AddTask([] {
				Settings::SettingsService::ApplyToGame();
				SKYBSettingsMenu::Store::GetSingleton()->OnGameReady();
				StartMenu::StartMenuService::RepairInputAfterMenus();
			});
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			//The save is in: the degradation INI if it changed, file-backed settings into its globals,
			//then every settings-menu row applied (degradation overrides among them).
			SKYBDeg::Settings::ReloadIfChanged();
			Settings::SettingsService::ApplyToGame();
			SKYBSettingsMenu::Store::GetSingleton()->OnGameReady();
			StartMenu::StartMenuService::RepairInputAfterMenus();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			Log::INFO("SKSE Data Loaded...");
			DispositionSystem::OnDataLoaded();
			Sigil::SigilSystem::Register();
			Achievements::AchievementStore::Register();
			//Before UISystem: the title screen and the journal read the settings.
			Settings::SettingsService::Register();
			//The settings menu and the services it drives (from SKYBUIDemo.dll, 2026-10-05).
			//Combat before the store: the store applies through it.
			CombatService::GetSingleton()->Register();
			EconomyService::GetSingleton()->Register();
			HotkeyService::GetSingleton()->Register();
			InterfaceAdapters::GetSingleton()->Register();
			SKYBSettingsMenu::Store::GetSingleton()->Register();
			SKYBSettingsMenu::SettingsMenu::Register();
			//Degradation: its INI and tool list, the repair menu and its opener, the decay itself.
			SKYBDeg::Settings::Resolve();
			SKYBDeg::RepairMenu::Register();
			SKYBDeg::RepairMenuOpener::GetSingleton()->Register();
			SKYBDeg::ChargeWatcher::GetSingleton()->Register();
			SKYBDeg::DecayService::GetSingleton()->Register();
			UISystem::OnDataLoaded();
			Log::INFO("SKSE Data Loaded Complete");
			break;
		default:
			break;
		}
	};
	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		return false;
	}

	Log::INFO("Listener Registered. Waiting for Listener to Be Called...");

	return true;
}
