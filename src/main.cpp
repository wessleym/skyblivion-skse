#include "Papyrus/SKSEScriptRegistrar.h"
#include "Disposition/DispositionSystem.h"

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
    auto logsFolder = SKSE::log::log_directory();//For some reason, this maps to "Documents\My Games\Skyrim.INI\SKSE\".
    if (!logsFolder) {
        REX::INFO("!logsFolder");
        return false;
    }

    REX::INFO("Skyblivion SKSE Plugin Loading...");

    SKSE::Init(a_skse);

    if (a_skse->IsEditor()) {
        REX::CRITICAL("Loaded in editor. Marking as incompatible.");
        return false;
    }

    if (!SKSEScriptRegistrar::Register()) {
        return false;
    }

    if (!DispositionSystem::Initialize()) {
        return false;
    }

    return true;
}
