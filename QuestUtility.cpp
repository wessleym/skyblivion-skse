class QuestUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm) {
        vm->RegisterFunction("PrepareForReinitializing", "SKYBQuestUtility", PrepareForReinitializing);
    }
private:
    static bool PrepareForReinitializing(RE::StaticFunctionTag*, RE::TESQuest* quest) {
        quest->alreadyRun = false;
        // TODO: stop
        return true;
    }
};