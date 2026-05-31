#include "Plugin.h"

#include "Disposition/DispositionSystem.h"
#include "Papyrus/SKSEScriptRegistrar.h"
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
		if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
			Log::INFO("SKSE Data Loaded...");
			DispositionSystem::OnDataLoaded();
			UISystem::OnDataLoaded();
			Log::INFO("SKSE Data Loaded Complete");
		}
	};
	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		return false;
	}

	Log::INFO("Listener Registered. Waiting for Listener to Be Called...");

	return true;
}
