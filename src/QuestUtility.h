class QuestUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm) {
        SKSEScriptRegistrar::Register(vm, "SKYBQuestUtility", "PrepareForReinitializing", PrepareForReinitializing);
    }
private:
    static bool PrepareForReinitializing(RE::StaticFunctionTag*, RE::TESQuest* quest) {
        quest->alreadyRun = false;
        // TODO: stop
        return true;
    }
};