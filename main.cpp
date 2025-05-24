#include <spdlog/sinks/basic_file_sink.h>

#include "FileUtility.cpp"
#include "FormUtility.cpp"
#include "GameUtility.cpp"
#include "ObjectReferenceUtility.cpp"
#include "QuestUtility.cpp"
#include "SkillUtility.cpp"
#include "SKSELog.cpp"

#ifndef SKYB_VERSION_INCLUDED
    #define SKYB_VERSION_INCLUDED

    #define MAKE_STR_HELPER(a_str) #a_str
    #define MAKE_STR(a_str) MAKE_STR_HELPER(a_str)

    #define SKYB_VERSION_MAJOR 1
    #define SKYB_VERSION_MINOR 0
    #define SKYB_VERSION_PATCH 0
    #define SKYB_VERSION_BETA 0
    #define SKYB_VERSION_VERSTRING   \
        MAKE_STR(SKYB_VERSION_MAJOR) \
        "." MAKE_STR(SKYB_VERSION_MINOR) "." MAKE_STR(SKYB_VERSION_PATCH) "." MAKE_STR(SKYB_VERSION_BETA)

#endif

static bool RegisterFuncs(RE::BSScript::Internal::VirtualMachine* vm) {
    _MESSAGE("Registering custom functions...");
    FileUtility::Register(vm);
    FormUtility::Register(vm);
    GameUtility::Register(vm);
    QuestUtility::Register(vm);
    ObjectReferenceUtility::Register(vm);
    SkillUtility::Register(vm);
    _MESSAGE("Custom functions registered.");
    return true;
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    auto logsFolder = SKSE::log::log_directory();//For some reason, this maps to "Documents\My Games\Skyrim.INI\SKSE\".
    if (!logsFolder) {
        SKSE::stl::report_and_fail("!logsFolder");
    }
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);

    _MESSAGE("Skyblivion SKSE Plugin Loading...");

    SKSE::Init(skse);

    if (skse->IsEditor()) {
        _FATALERROR("Loaded in editor. Marking as incompatible.");
        return false;
    }

    auto papyrus = SKSE::GetPapyrusInterface();
    if (!papyrus->Register(RegisterFuncs)) {
        _FATALERROR("Failed to register papyrus callback!");
        return false;
    }

    return true;
}