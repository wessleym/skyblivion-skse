#pragma once

class DispositionSystem {
public:
    static bool Initialize();
    static void SetActorDisposition(RE::Actor* actor, std::string_view edid);

private:
    static constexpr RE::ActorValue kDispositionAV = RE::ActorValue::kVariable01;
    static constexpr const char* kDispositionAVName = "Variable01";

    template <class T>
    static void LookUpForm(const char* editorID, T*& out, const char* typeName) {
        auto form = RE::TESForm::LookupByEditorID(editorID);
        if (!form) {
            REX::WARN("Failed to find {}: {}", typeName, editorID);
            return;
        }
        out = form->As<T>();
        if (!out) {
            REX::WARN("Form was not a {}: {}", typeName, editorID);
        }
    }

    static void OnMessage(SKSE::MessagingInterface::Message* msg);
    static void LookUpFactions();
    static void LookUpGlobals();
    static void LookUpRaces();
    static float CalcDisposition(RE::Actor* npc);
    static void PapyrusSetActorValue(RE::TESObjectREFR* a_ref, RE::BSFixedString valueName, float value);
};
