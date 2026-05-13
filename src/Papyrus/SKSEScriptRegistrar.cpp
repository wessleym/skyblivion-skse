#include "SKSEScriptRegistrar.h"
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

bool SKSEScriptRegistrar::Register() {
    auto papyrus = SKSE::GetPapyrusInterface();
    if (!papyrus->Register(RegisterFuncs)) {
        REX::CRITICAL("Failed to register Papyrus functions.");
        return false;
    }
    return true;
}

RE::SCRIPT_FUNCTION* SKSEScriptRegistrar::LocateFunction(std::string_view functionName) {
    RE::SCRIPT_FUNCTION* function = RE::SCRIPT_FUNCTION::LocateScriptCommand(functionName);
    if (function != nullptr) {
        REX::INFO("Found {} Function", functionName);
    } else {
        REX::ERROR("Could Not Find {} Function", functionName);
    }
    return function;
}
