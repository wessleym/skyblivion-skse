#include "SKSEScriptRegistrar.h"
#include "FileUtility.h"
#include "FormUtility.h"
#include "GameUtility.h"
#include "MouseUtility.h"
#include "ObjectReferenceUtility.h"
#include "QuestUtility.h"
#include "SkillUtility.h"
#include "UILauncher.h"

static bool RegisterFuncs(RE::BSScript::Internal::VirtualMachine* vm) {
    Log::INFO("SKSEScriptRegistrar: Registering Custom Functions...");
    FileUtility::Register(vm);
    FormUtility::Register(vm);
    GameUtility::Register(vm);
    MouseUtility::Register(vm);
    ObjectReferenceUtility::Register(vm);
    QuestUtility::Register(vm);
    SkillUtility::Register(vm);
    UILauncher::Register(vm);
    Log::INFO("SKSEScriptRegistrar: Registering Custom Functions Complete");
    return true;
}

bool SKSEScriptRegistrar::Initialize() {
    if (!SKSE::GetPapyrusInterface()->Register(RegisterFuncs)) {
        Log::CRITICAL("SKSEScriptRegistrar: Failed to Invoke SKSE::GetPapyrusInterface()->Register");
        return false;
    }
    return true;
}

RE::SCRIPT_FUNCTION* SKSEScriptRegistrar::LocateFunction(std::string_view functionName) {
    RE::SCRIPT_FUNCTION* function = RE::SCRIPT_FUNCTION::LocateScriptCommand(functionName);
    if (function != nullptr) {
        Log::INFO("SKSEScriptRegistrar: Found {} Function", functionName);
    } else {
        Log::ERROR("SKSEScriptRegistrar: Could Not Find {} Function", functionName);
    }
    return function;
}
