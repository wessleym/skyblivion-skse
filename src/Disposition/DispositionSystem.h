#pragma once

class DispositionSystem {
public:
    static void Initialize();
    static void OnDataLoaded();
    static void SetInitialDisposition(RE::Actor* actor, std::string_view edid);
    static int GetDispositionActorValue(RE::Actor* actor);
    static void SetDispositionActorValue(RE::Actor* actor, float value, bool force = false);

private:
    static constexpr RE::ActorValue kDispositionAV = RE::ActorValue::kVariable01;
    static constexpr const char* kDispositionAVName = "Variable01";

    template <class T>
    static void LookUpForm(const char* editorID, T*& out, const char* typeName) {
        auto form = RE::TESForm::LookupByEditorID(editorID);
        if (!form) {
            Log::ERROR("Failed to find {}: {}", typeName, editorID);
            return;
        }
        out = form->As<T>();
        if (!out) {
            Log::ERROR("Form was not a {}: {}", typeName, editorID);
        }
    }

    static void LookUpFactions();
    static void LookUpGlobals();
    static void LookUpRaces();
    static float CalcDisposition(RE::Actor* npc);
    static void PapyrusSetActorValue(RE::TESObjectREFR* a_ref, RE::BSFixedString valueName, float value, bool force);
};
