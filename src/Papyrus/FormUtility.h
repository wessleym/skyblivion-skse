#pragma once
#include "SKSEScriptRegistrar.h"

class FormUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm);

    // Clears every object's last GetSecondsPassed time. Called when a game is loaded or started.
    static void ClearGetSecondsPassedTimes();

private:
    //From https://github.com/NoahBoddie/rogues-gallery-papyrus/blob/main/src/Papyrus.cpp
    //Used with permission
    static std::vector<RE::TESForm*> GetAssociatedMenuForm(RE::BSScript::Internal::VirtualMachine* a_vm,
                                                            RE::VMStackID a_stackID,
                                                            RE::StaticFunctionTag*, RE::BSFixedString a_menu);

    // Meant to emulate Oblivion's GetSecondsPassed.
    static float GetSecondsPassed(RE::StaticFunctionTag*, RE::TESForm* a_form);
    static float GetSecondsPassedByActiveEffect(RE::StaticFunctionTag*, RE::ActiveEffect* a_effect);
    // The ScriptClock seconds since this object's previous call. Returns 0 on its first call since a game was loaded or started.
    static float SecondsSinceLastGetSecondsPassed(const void* a_object);
};
