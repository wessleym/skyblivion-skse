#pragma once
#include "SKSEScriptRegistrar.h"

class GameUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm) {
        std::string_view className = "SKYBGameUtility";
        SKSEScriptRegistrar::Register(vm, className, "LegacyGetAmountSoldStolen", getAmountSoldStolen);// WTM:  Change:  Renamed from GetAmountSoldStolen
        SKSEScriptRegistrar::Register(vm, className, "LegacyModAmountSoldStolen", modAmountSoldStolen);// WTM:  Change:  Renamed from ModAmountSoldStolen
        SKSEScriptRegistrar::Register(vm, className, "LegacyIsPCAMurderer", isPCAMurderer);// WTM:  Change:  Renamed from IsPCAMurderer
    }

private:
    static std::uint32_t getAmountSoldStolen(RE::StaticFunctionTag*) {
        return RE::PlayerCharacter::GetSingleton()->amountStolenSold;
    }
    static void modAmountSoldStolen(RE::StaticFunctionTag*, unsigned long amount) {
        RE::PlayerCharacter::GetSingleton()->amountStolenSold +=
            amount;  // WTM:  Change:  Was amountStolenSold = amount.
    }
    static std::uint32_t isPCAMurderer(RE::StaticFunctionTag*) {
        return RE::PlayerCharacter::GetSingleton()->murder;
    }
};