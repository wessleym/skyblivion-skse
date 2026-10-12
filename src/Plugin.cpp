#include "Plugin.h"

#include "Achievements/AchievementService.h"
#include "Degradation/Breakage.h"
#include "Degradation/DecayService.h"
#include "Degradation/InventoryHook.h"
#include "Degradation/RepairMenu.h"
#include "Degradation/Settings.h"
#include "Degradation/TemperHook.h"
#include "Disposition/DispositionSystem.h"
#include "Hotkeys/HotkeyService.h"
#include "Papyrus/SKSEScriptRegistrar.h"
#include "Resources.h"
#include "Platform/MainThread.h"
#include "Settings/SettingsService.h"
#include "SettingsMenu/CombatService.h"
#include "SettingsMenu/EconomyService.h"
#include "SettingsMenu/InterfaceAdapters.h"
#include "SettingsMenu/SettingsMenu.h"
#include "SettingsMenu/SettingsStore.h"
#include "SettingsMenu/SoundService.h"
#include "Sigil/SigilSystem.h"
#include "Storage/CoSave.h"
#include "UIs/PrismaUI/Persuasion/PersuasionSettings.h"
#include "UIs/Scaleform/StartMenu/StartMenuService.h"
#include "UIs/UISystem.h"

#include <Windows.h>

bool Plugin::OnLoad(const SKSE::LoadInterface* a_skse)
{
	auto logsFolder = SKSE::log::log_directory();
	if (!logsFolder) {
		Log::CRITICAL("!logsFolder");
		return false;
	}

	SKSE::Init(a_skse);

	Log::INFO("Skyblivion SKSE Plugin Loading...");

	//Loads block the main thread, which owns the window, so after five seconds a click makes Windows
	//show "(Not Responding)" although the load is proceeding. Ghosting is turned off for the process.
	::DisableProcessWindowsGhosting();
	Log::INFO("Window ghosting disabled: no \"(Not Responding)\" during long loads.");

	if (a_skse->IsEditor()) {
		Log::CRITICAL("Loaded in editor (Creation Kit)");
		return false;
	}

	if (!SKSEScriptRegistrar::Initialize()) {
		return false;
	}

	DispositionSystem::Initialize();

	//One SKSE trampoline for every call-site hook in the plugin (the frame hook, the temper
	//patches): AllocTrampoline replaces the previous allocation, so it is called once, here.
	SKSE::AllocTrampoline(1 << 8);
	//Calls into live Scaleform movies run on the main thread, once a frame (SKSE's task queues
	//can run on worker threads).
	MainThread::Install();

	//Weapon and armor degradation. The temper patches install on Anniversary Edition runtimes
	//only; elsewhere they log why and stay off.
	SKYBDeg::InventoryHook::Install();
	SKYBDeg::TemperHook::Install();

	UISystem::Initialize();

	//One co-save for the plugin: every store adds its record first (UISystem: spell making).
	SKYBSettingsMenu::Store::InstallSerialization();
	//Achievements: this save's unlocks (definitions in ConfigFiles\Achievements).
	Achievements::AchievementService::InstallSerialization();
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
			UISystem::OnPreLoadGame({ static_cast<const char*>(msg->data), msg->dataLen });
			break;
		case SKSE::MessagingInterface::kNewGame:
			UISystem::OnGameStarting();
			//The new game's globals exist a frame later.
			SKYBDeg::Settings::ReloadIfChanged();
			Persuasion::Settings::ReloadIfChanged();
			SKSE::GetTaskInterface()->AddTask([] {
				SKYBSettingsMenu::Store::GetSingleton()->OnGameReady();
				StartMenu::StartMenuService::OnNewGame();  // the new-game panel's presets, if chosen
				StartMenu::StartMenuService::RepairInputAfterMenus();
			});
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			//Reload the degradation and persuasion INIs if they changed, then apply every settings-menu
			//row (its degradation and persuasion overrides among them).
			SKYBDeg::Settings::ReloadIfChanged();
			Persuasion::Settings::ReloadIfChanged();
			SKYBSettingsMenu::Store::GetSingleton()->OnGameReady();
			StartMenu::StartMenuService::RepairInputAfterMenus();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			Log::INFO("SKSE Data Loaded...");
			//The systems below use Resources.
			Resources::Install();
			DispositionSystem::OnDataLoaded();
			Sigil::SigilSystem::Register();
			Achievements::AchievementService::Register();
			//Before UISystem: the title screen and the journal read the settings.
			Settings::SettingsService::Register();
			//The settings menu and the services it drives.
			//Combat before the store: the store applies through it.
			CombatService::GetSingleton()->Register();
			SKYBSettingsMenu::SoundService::GetSingleton()->Register();
			EconomyService::GetSingleton()->Register();
			HotkeyService::GetSingleton()->Register();
			InterfaceAdapters::GetSingleton()->Register();
			SKYBSettingsMenu::Store::GetSingleton()->Register();
			SKYBSettingsMenu::SettingsMenu::Register();
			//Degradation: its INI and tool list, the repair menu and its opener, the decay itself.
			SKYBDeg::Settings::Resolve();
			SKYBDeg::RepairMenu::Register();
			SKYBDeg::RepairMenuOpener::GetSingleton()->Register();
			SKYBDeg::DecayService::GetSingleton()->Register();
			SKYBDeg::Breakage::EquipGuard::GetSingleton()->Register();
			//Persuasion: its INI and perk rules (UISystem opens the wheel).
			Persuasion::Settings::Resolve();
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
