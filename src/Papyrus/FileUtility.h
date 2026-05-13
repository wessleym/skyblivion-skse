#pragma once
#include "SKSEScriptRegistrar.h"

class FileUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm);

private:
    // https://github.com/eeveelo/PapyrusUtil/blob/6212a5cdedbaceb5d805501b9518921ce5423e76/MiscUtil.cpp#L512
    static std::vector<RE::BSFixedString> FilesInFolder(RE::StaticFunctionTag*, RE::BSFixedString directoryPath,
                                                        RE::BSFixedString extension);
};
