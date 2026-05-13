#pragma once
#include "SKSEScriptRegistrar.h"

class FormUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm);

private:
    //From https://github.com/NoahBoddie/rogues-gallery-papyrus/blob/main/src/Papyrus.cpp
    //Used with permission
    static std::vector<RE::TESForm*> GetAssociatedMenuForm(RE::BSScript::Internal::VirtualMachine* a_vm,
                                                            RE::VMStackID a_stackID,
                                                            RE::StaticFunctionTag*, RE::BSFixedString a_menu);
};
