#pragma once

class SKSEScriptRegistrar {
public:
    static bool Initialize();
    static RE::SCRIPT_FUNCTION* LocateFunction(std::string_view functionName);

    // threadSafe: Papyrus may call the function at once from its own threads instead of waiting for the next frame.
    template <class F>
    static void Register(RE::BSScript::Internal::VirtualMachine* vm, std::string_view className, std::string_view functionName, F callback, bool threadSafe = false) {
        vm->RegisterFunction(functionName, className, callback, threadSafe);
        Log::INFO("SKSEScriptRegistrar: Registered {}.{}", className, functionName);
    }
};