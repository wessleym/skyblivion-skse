class SkillUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm) {
        SKSEScriptRegistrar::Register(vm, "SKYBSkillUtility", "GetSkillDataArray", GetSkillDataArray);
    }

private:
    static std::vector<float> GetSkillDataArray(RE::StaticFunctionTag*, int mode) {
        int listSize = RE::PlayerCharacter::PlayerSkills::Data::Skill::kTotal;
        RE::PlayerCharacter* pPC = RE::PlayerCharacter::GetSingleton();
        auto skills = pPC->GetInfoRuntimeData().skills->data->skills;
        std::vector<float> returnValue(listSize, 0.0f);
        for (int i = 0; i < listSize; i++) {
            auto skill = skills[i];
            returnValue[i] = mode == 0   ? skill.xp
                             : mode == 1 ? skill.levelThreshold
                                         : (skill.xp / skill.levelThreshold) * 100;
        }
        return returnValue;
    }
};