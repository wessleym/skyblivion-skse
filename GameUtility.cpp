#include <windows.h>

class GameUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm) {
        std::string_view className = "SKYBGameUtility";
        SKSEScriptRegistrar::Register(vm, className, "LegacyGetAmountSoldStolen", getAmountSoldStolen);// WTM:  Change:  Renamed from GetAmountSoldStolen
        SKSEScriptRegistrar::Register(vm, className, "LegacyModAmountSoldStolen", modAmountSoldStolen);// WTM:  Change:  Renamed from ModAmountSoldStolen
        SKSEScriptRegistrar::Register(vm, className, "LegacyIsPCAMurderer", isPCAMurderer);// WTM:  Change:  Renamed from IsPCAMurderer
    }

private:
    static UINT32 getAmountSoldStolen(RE::StaticFunctionTag*) {
        return RE::PlayerCharacter::GetSingleton()->GetInfoRuntimeData().amountStolenSold;
    }
    static void modAmountSoldStolen(RE::StaticFunctionTag*, unsigned long amount) {
        RE::PlayerCharacter::GetSingleton()->GetInfoRuntimeData().amountStolenSold +=
            amount;  // WTM:  Change:  Was amountStolenSold = amount.
    }
    static UINT32 isPCAMurderer(RE::StaticFunctionTag*) {
        return RE::PlayerCharacter::GetSingleton()->GetGameStatsData().murder;
    }
};