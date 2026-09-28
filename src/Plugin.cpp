#include "Plugin.h"

#include "Achievements/AchievementStore.h"
#include "Disposition/DispositionSystem.h"
#include "Papyrus/SKSEScriptRegistrar.h"
#include "Settings/SettingsService.h"
#include "Sigil/SigilSystem.h"
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

	UISystem::Initialize();

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
			SKSE::GetTaskInterface()->AddTask([] { Settings::SettingsService::ApplyToGame(); });
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			Settings::SettingsService::ApplyToGame();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			Log::INFO("SKSE Data Loaded...");
			DispositionSystem::OnDataLoaded();
			Sigil::SigilSystem::Register();
			Achievements::AchievementStore::Register();
			//Before UISystem: the title screen and the journal read the settings.
			Settings::SettingsService::Register();
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
