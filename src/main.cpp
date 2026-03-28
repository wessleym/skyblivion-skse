#include "FileUtility.h"
#include "FormUtility.h"
#include "GameUtility.h"
#include "MouseUtility.h"
#include "ObjectReferenceUtility.h"
#include "QuestUtility.h"
#include "SkillUtility.h"

static bool RegisterFuncs(RE::BSScript::Internal::VirtualMachine* vm) {
    REX::INFO("Registering custom functions...");
    FileUtility::Register(vm);
    FormUtility::Register(vm);
    GameUtility::Register(vm);
    MouseUtility::Register(vm);
    ObjectReferenceUtility::Register(vm);
    QuestUtility::Register(vm);
    SkillUtility::Register(vm);
    REX::INFO("Custom functions registered.");
    return true;
}


SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
    auto logsFolder = SKSE::log::log_directory();//For some reason, this maps to "Documents\My Games\Skyrim.INI\SKSE\".
    if (!logsFolder) {
        REX::INFO("!logsFolder");
        return false;
    }

    /*
    auto pluginName = SKSE::GetPluginName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
    */

    REX::INFO("Skyblivion SKSE Plugin Loading...");

	SKSE::Init(a_skse);

    if (a_skse->IsEditor()) {
        REX::CRITICAL("Loaded in editor. Marking as incompatible.");
        return false;
    }

    auto papyrus = SKSE::GetPapyrusInterface();
    if (!papyrus->Register(RegisterFuncs)) {
        REX::CRITICAL("Failed to register papyrus callback!");
        return false;
    }

    return true;
}
