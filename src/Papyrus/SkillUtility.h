#pragma once
#include "SKSEScriptRegistrar.h"

class SkillUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm) {
        SKSEScriptRegistrar::Register(vm, "SKYBSkillUtility", "GetSkillDataArray", GetSkillDataArray);
    }

private:
    static std::vector<float> GetSkillDataArray(RE::StaticFunctionTag*, int mode) {
        int listSize = RE::PlayerCharacter::PlayerSkills::Data::Skill::kTotal;
        std::vector<float> returnValue(listSize, 0.0f);
        // Zeros until the player's skill data exists (the main menu, early load).
        RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
        auto playerSkills = player ? REBridge::PlayerSkills(player) : nullptr;
        if (!playerSkills || !playerSkills->data) {
            Log::WARN("SkillUtility: Player skill data unavailable. Returning zeros.");
            return returnValue;
        }
        auto skills = playerSkills->data->skills;
        for (int i = 0; i < listSize; i++) {
            auto skill = skills[i];
            returnValue[i] = mode == 0   ? skill.xp
                             : mode == 1 ? skill.levelThreshold
                             : skill.levelThreshold > 0.0f ? (skill.xp / skill.levelThreshold) * 100
                                                           : 0.0f;
        }
        return returnValue;
    }
};