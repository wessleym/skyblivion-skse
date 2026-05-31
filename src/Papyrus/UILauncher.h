#pragma once
#include "SKSEScriptRegistrar.h"
#include "UIs/UISystem.h"

class UILauncher {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm) {
        SKSEScriptRegistrar::Register(vm, "SKYBUILauncher", "OpenPersuasionGame", OpenPersuasionGame);
        SKSEScriptRegistrar::Register(vm, "SKYBUILauncher", "OpenSpellMaking", OpenSpellMaking);
    }

private:
    static void OpenPersuasionGame(RE::StaticFunctionTag*, RE::Actor* target) {
        Log::INFO("Papyrus: OpenPersuasionGame called with target {:08X}",
                  target ? target->GetFormID() : 0u);
        UISystem::OpenPersuasion(target);
    }

    static void OpenSpellMaking(RE::StaticFunctionTag*) {
        Log::INFO("Papyrus: OpenSpellMaking called");
        UISystem::OpenSpellMaking();
    }
};
