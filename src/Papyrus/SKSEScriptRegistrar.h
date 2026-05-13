#pragma once

class SKSEScriptRegistrar {
public:
    static bool Register();
    static RE::SCRIPT_FUNCTION* LocateFunction(std::string_view functionName);

    template <class F>
    static void Register(RE::BSScript::Internal::VirtualMachine* vm, std::string_view className, std::string_view functionName, F callback) {
        vm->RegisterFunction(functionName, className, callback);
        REX::INFO("Registered {}.{}", className, functionName);
    }
};