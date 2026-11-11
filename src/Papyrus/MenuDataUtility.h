#pragma once
#include "SKSEScriptRegistrar.h"
#include "Platform/FormCache.h"

#include <cstdint>
#include <vector>

//SKYBMenuData (Source\Scripts\SKYBMenuData.psc): reads that a menu's opening data needs, one call each,
//where Papyrus would spend a frame on every value. The class menu's opening lines use both (26 base
//values and the 43-entry tuning list, which would otherwise be about 90 frame-bound calls).
class MenuDataUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm) {
        SKSEScriptRegistrar::Register(vm, "SKYBMenuData", "BaseValues", BaseValues);
        SKSEScriptRegistrar::Register(vm, "SKYBMenuData", "TuningValues", TuningValues);
    }

private:
    //The actor's base value of each named actor value (as Papyrus's GetBaseActorValue(name) as int);
    //0 for a name the game does not know. Empty with no actor.
    static std::vector<std::int32_t> BaseValues(RE::StaticFunctionTag*, RE::Actor* actor, std::vector<RE::BSFixedString> names) {
        std::vector<std::int32_t> out;
        if (!actor) {
            return out;
        }
        out.resize(names.size(), 0);
        auto* owner = REBridge::AVOwner(actor);
        if (!owner) {
            return out;
        }
        for (std::size_t i = 0; i < names.size(); ++i) {
            const auto av = REBridge::ActorValueByName(names[i].c_str());
            if (av != RE::ActorValue::kNone) {
                out[i] = static_cast<std::int32_t>(owner->GetBaseActorValue(av));
            }
        }
        return out;
    }

    //The values of the first count globals of a form list found by editor ID (as SKYBAttributeUtility.LoadTuning):
    //an entry past the list's end, or not a global, is 0; empty when the list is missing or one of its first
    //fourteen entries is not a global.
    static std::vector<float> TuningValues(RE::StaticFunctionTag*, RE::BSFixedString listEditorID, std::int32_t count) {
        //A list that does not resolve is logged once by the form table.
        auto* list = FormCache::Get<RE::BGSListForm>(listEditorID.c_str(), "SKYBMenuData.TuningValues");
        if (!list || count <= 0) {
            return {};
        }
        std::vector<float> out(static_cast<std::size_t>(count), 0.0f);
        const auto listed = static_cast<std::int32_t>(list->forms.size());
        for (std::int32_t i = 0; i < count && i < listed; ++i) {
            auto* form = list->forms[i];
            auto* global = form ? form->As<RE::TESGlobal>() : nullptr;
            if (global) {
                out[i] = global->value;
            } else if (i < 14) {
                Log::WARN("SKYBMenuData.TuningValues: entry {} of '{}' is not a global", i, listEditorID.c_str());
                return {};
            }
        }
        return out;
    }
};
