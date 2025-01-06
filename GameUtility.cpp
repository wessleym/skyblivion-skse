#include <windows.h>

class GameUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm) {
        vm->RegisterFunction("LegacyGetAmountSoldStolen", "TES4GameUtility",
                             getAmountSoldStolen);  // WTM:  Change:  Renamed from GetAmountSoldStolen
        vm->RegisterFunction("LegacyModAmountSoldStolen", "TES4GameUtility",
                             modAmountSoldStolen);  // WTM:  Change:  Renamed from ModAmountSoldStolen
        vm->RegisterFunction("LegacyIsPCAMurderer", "TES4GameUtility",
                             isPCAMurderer);  // WTM:  Change:  Renamed from IsPCAMurderer
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