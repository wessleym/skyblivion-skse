#pragma once
#include "SKSEScriptRegistrar.h"
#include <array>

class ObjectReferenceUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm);

private:
    static inline RE::SCRIPT_FUNCTION* sayFunction = nullptr;
    static inline std::array<std::uint8_t, 9> sayScriptData{
        static_cast<std::uint8_t>(RE::FUNCTION_DATA::FunctionID::kSay), 0x10, 0x5, 0x0, 0x1, 0x0, 0x72, 0x1, 0x0};
    static inline RE::SCRIPT_FUNCTION* sayToFunction = nullptr;
    static inline std::array<std::uint8_t, 12> sayToScriptData{
        static_cast<std::uint8_t>(RE::FUNCTION_DATA::FunctionID::kSayTo), 0x10, 0x8, 0x0, 0x2, 0x0, 0x72, 0x1, 0x0, 0x72, 0x2, 0x0};
    static inline RE::SCRIPT_FUNCTION* getDestroyedFunction = nullptr;
    static inline RE::SCRIPT_FUNCTION* startConversationFunction = nullptr;  // WTM:  Change:  Experimenting
    static inline std::array<std::uint8_t, 12> startConversationScriptData{
        static_cast<std::uint8_t>(RE::FUNCTION_DATA::FunctionID::kStartConversation), 0x10, 0x8, 0x0, 0x2, 0x0, 0x72, 0x1, 0x0, 0x72, 0x2, 0x0};

    static RE::Script* initDummySayScript();
    static float ObScriptSay(RE::StaticFunctionTag*, RE::TESObjectREFR* thisActor, RE::TESTopic* TopicID, bool value);

    static RE::Script* initDummySayToScript();
    static float ObScriptSayTo(RE::StaticFunctionTag*, RE::TESObjectREFR* thisActor, RE::Actor* anotherActor,
                               RE::TESTopic* TopicID, bool value);

    // Runs the Say or SayTo command with its script and returns the line's length plus half a second.
    static float RunSayCommand(RE::SCRIPT_FUNCTION* command, RE::Script* script, RE::TESObjectREFR* speaker,
                               RE::TESObjectREFR* listener, RE::TESTopic* topic);

    static bool isAnimPlaying(RE::StaticFunctionTag*, RE::TESObjectREFR* animatedRefr);

    // TODO: CHECK: should be Double?
    static std::uint32_t getDestroyed(RE::StaticFunctionTag*, RE::TESObjectREFR* reference);

    static RE::Script* initDummyStartConversationScript();
    static void startConversation(RE::StaticFunctionTag*, RE::Actor* thisActor, RE::Actor* otherActor,
                                  RE::TESTopic* topic);  // WTM:  Change:  Experimenting.  I'm trying to pass in arguments.

    static bool ContainsItem(RE::StaticFunctionTag*, RE::TESObjectREFR* objectRef, RE::TESForm* soughtObject);
};
