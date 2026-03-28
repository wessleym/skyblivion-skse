#pragma once

#undef ERROR

class SKSEScriptRegistrar {
public:
    static RE::SCRIPT_FUNCTION* LocateFunction(std::string_view functionName) {
        RE::SCRIPT_FUNCTION* function = RE::SCRIPT_FUNCTION::LocateScriptCommand(functionName);
        if (function != NULL) {
            REX::INFO("Found {} Function", functionName);
        } else {
            REX::ERROR("Could Not Find {} Function", functionName);
        }
        return function;
    }
    
	template <class F>
    static void Register(RE::BSScript::Internal::VirtualMachine* vm, std::string_view className, std::string_view functionName, F callback) {
        vm->RegisterFunction(functionName, className, callback);
        REX::INFO("Registered {}.{}", className, functionName);
    }
};