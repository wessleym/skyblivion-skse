#include "SKSELog.cpp"

class ObjectReferenceUtility {
public:
    static void Register(RE::BSScript::Internal::VirtualMachine* vm) {
        std::string_view className = "SKYBObjectReferenceUtility";
        
        sayFunction = SKSEScriptRegistrar::LocateFunction("Say");
        SKSEScriptRegistrar::Register(vm, className, "LegacySay", ObScriptSay);

        sayToFunction = SKSEScriptRegistrar::LocateFunction("SayTo");
        SKSEScriptRegistrar::Register(vm, className, "LegacySayTo", ObScriptSayTo);

        isAnimPlayingFunction = SKSEScriptRegistrar::LocateFunction("IsAnimPlaying");
        SKSEScriptRegistrar::Register(vm, className, "IsAnimPlaying", isAnimPlaying);

        getDestroyedFunction = SKSEScriptRegistrar::LocateFunction("GetDestroyed");
        SKSEScriptRegistrar::Register(vm, className, "LegacyGetDestroyed", getDestroyed);

        // vm->RegisterFunction("LegacyGetContainer", "ObjectReference", GetContainer);// WTM:  Change:  Experimenting

        startConversationFunction = SKSEScriptRegistrar::LocateFunction("StartConversation");
        SKSEScriptRegistrar::Register(vm, className, "LegacyStartConversation", startConversation);
        // WTM:  Note:  I think I got this to work, but it only seems to work when called on the player.
        // For example, PlayerRef.StartConversation(SomeActor_p, SomeTopic_p) works, but
        // SomeActor_p.StartConversation(PlayerRef, SomeTopic_p) seems to do nothing.

        SKSEScriptRegistrar::Register(vm, className, "ContainsItem2", ContainsItem);
    }

private:
    /* __declspec(dllimport) */ static double g_silent_voice_duration_seconds;
    /* __declspec(dllimport) */ static int g_is_obscript_say_say_to;

    static RE::SCRIPT_FUNCTION* sayFunction;
    static BYTE sayScriptData[9];
    static RE::Script* initDummySayScript() {
        BYTE dummySCRISayAlloc[sizeof(RE::Script)] = {0};
        RE::Script* dummySayScript = (RE::Script*)&dummySCRISayAlloc;
        char say[4] = "Say";
        dummySayScript->formFlags = 0x000400a;
        dummySayScript->formID = 0xff000e05;
        dummySayScript->formType = (RE::FormType)0x13;
        // dummySayScript->pad13 = 0x00;
        dummySayScript->header.refObjectCount = 0x00000001;
        dummySayScript->header.dataSize = 0x00000009;
        dummySayScript->header.variableCount = 0x00000000;
        dummySayScript->header.isQuestScript = 1;
        dummySayScript->text = say;
        dummySayScript->data = (RE::SCRIPT_FUNCTION::ScriptData*)sayScriptData;
        return dummySayScript;
    }
    static float ObScriptSay(RE::StaticFunctionTag*, RE::TESObjectREFR* thisActor, RE::TESTopic* TopicID, bool value) {
        double result = 0.0;

        if (NULL != sayFunction) {
            // auto scriptFactory = IFormFactory::GetConcreteFormFactoryByType<Script>();
            // if (!scriptFactory) {
            //	return 0.0;
            // }

            // Script* script = scriptFactory->Create();
            // if (!script) {
            //	return 0.0;
            // }

            UINT32 opcodeOffset = 0x4;

            if (thisActor == NULL || TopicID == NULL) return 0.5;

            RE::SCRIPT_REFERENCED_OBJECT arg2;
            // memset(&arg2.form_name, 0, sizeof(BSString));
            arg2.editorID = "";
            arg2.form = TopicID;

            // BSSimpleList<SCRIPT_REFERENCED_OBJECT*> arglist2(&arg2, NULL);
            // BSSimpleList<SCRIPT_REFERENCED_OBJECT*> arglist1(&arg1, &arglist2);
            // RefListEntry arglist2;

            // arglist1.var = &arg1;
            // arglist2.var = &arg2;
            // arglist1.next = &arglist2;
            // arglist2.next = NULL;

            RE::BSSimpleList<RE::SCRIPT_REFERENCED_OBJECT*> reflist;
            reflist.push_front(&arg2);

            RE::Script* dummySayScript = initDummySayScript();
            dummySayScript->refObjects = reflist;

            g_is_obscript_say_say_to = true;
            g_silent_voice_duration_seconds = 5.0;

            sayFunction->executeFunction(sayFunction->params, (RE::SCRIPT_FUNCTION::ScriptData*)sayScriptData,
                                         thisActor, NULL, dummySayScript, NULL, result, opcodeOffset);

            g_is_obscript_say_say_to = false;
            result = g_silent_voice_duration_seconds;
            return result + 0.5;

            // std::stringstream stream;
            // stream << "Say "
            //	<< std::setfill('0') << std::setw(8)
            //	<< std::hex << TopicID->formID;

            //_DMESSAGE(stream.str().c_str());

            // script->SetCommand(stream.str());

            // g_is_obscript_say_say_to = true;

            // script->Invoke(thisActor, RE::Script::InvokeType::kSysWindowCompileAndRun);

            // g_is_obscript_say_say_to = false;

            // result = g_silent_voice_duration_seconds;

            // delete script;
        }
        return result;
    }

    static RE::SCRIPT_FUNCTION* sayToFunction;
    static BYTE sayToScriptData[12];
    static RE::Script* initDummySayToScript() {
        BYTE dummySCRISayToAlloc[sizeof(RE::Script)] = {0};
        RE::Script* dummySayToScript = (RE::Script*)&dummySCRISayToAlloc;
        char sayTo[6] = "SayTo";
        dummySayToScript->formFlags = 0x000400a;
        dummySayToScript->formID = 0xff000a17;
        dummySayToScript->formType = (RE::FormType)0x13;
        // dummySayToScript->pad13 = 0x0f;
        dummySayToScript->header.refObjectCount = 0x00000002;
        dummySayToScript->header.dataSize = 0x0000000c;
        dummySayToScript->header.variableCount = 0x00000000;
        dummySayToScript->header.isQuestScript = 1;
        // dummySayToScript->headertype = 0x00010000;
        dummySayToScript->text = sayTo;
        dummySayToScript->data = (RE::SCRIPT_FUNCTION::ScriptData*)sayToScriptData;
        return dummySayToScript;
    }
    static float ObScriptSayTo(RE::StaticFunctionTag*, RE::TESObjectREFR* thisActor, RE::Actor* anotherActor,
                               RE::TESTopic* TopicID, bool value) {
        double result = 0.0;

        if (NULL != sayToFunction) {
            // auto scriptFactory = IFormFactory::GetConcreteFormFactoryByType<Script>();
            // if (!scriptFactory) {
            //	return 0.0;
            // }

            // Script* script = scriptFactory->Create();
            // if (!script) {
            //	return 0.0;
            // }

            UINT32 opcodeOffset = 0x4;

            if (thisActor == NULL || anotherActor == NULL || TopicID == NULL) return 0.5;

            RE::SCRIPT_REFERENCED_OBJECT arg1;
            // memset(&arg1.form_name, 0, sizeof(BSString));
            arg1.editorID = "";
            arg1.form = anotherActor;
            RE::SCRIPT_REFERENCED_OBJECT arg2;
            // memset(&arg2.form_name, 0, sizeof(BSString));
            arg2.editorID = "";
            arg2.form = TopicID;

            // BSSimpleList<SCRIPT_REFERENCED_OBJECT*> arglist2(&arg2, NULL);
            // BSSimpleList<SCRIPT_REFERENCED_OBJECT*> arglist1(&arg1, &arglist2);
            // RefListEntry arglist2;

            // arglist1.var = &arg1;
            // arglist2.var = &arg2;
            // arglist1.next = &arglist2;
            // arglist2.next = NULL;

            RE::BSSimpleList<RE::SCRIPT_REFERENCED_OBJECT*> reflist;
            reflist.push_front(&arg2);
            reflist.push_front(&arg1);

            RE::Script* dummySayToScript = initDummySayToScript();
            dummySayToScript->refObjects = reflist;

            g_is_obscript_say_say_to = true;
            g_silent_voice_duration_seconds = 5.0;

            sayToFunction->executeFunction(sayToFunction->params, (RE::SCRIPT_FUNCTION::ScriptData*)sayToScriptData,
                                           thisActor, NULL, dummySayToScript, NULL, result, opcodeOffset);

            g_is_obscript_say_say_to = false;
            result = g_silent_voice_duration_seconds;
            return result + 0.5;

            // std::stringstream stream;
            // stream << "Say "
            //	<< std::setfill('0') << std::setw(8)
            //	<< std::hex << TopicID->formID;

            //_DMESSAGE(stream.str().c_str());

            // script->SetCommand(stream.str());

            // g_is_obscript_say_say_to = true;

            // script->Invoke(thisActor, RE::Script::InvokeType::kSysWindowCompileAndRun);

            // g_is_obscript_say_say_to = false;

            // result = g_silent_voice_duration_seconds;

            // delete script;
        }
        return result;
    }

    static RE::SCRIPT_FUNCTION* isAnimPlayingFunction;
    static bool isAnimPlaying(RE::StaticFunctionTag*, RE::TESObjectREFR* animatedRefr) {
        if (isAnimPlayingFunction) {
            RE::SCRIPT_FUNCTION::ScriptData isAnimPlayingFunctionData = {0x1128, 0x0001, 0x0000};
            double result = 0.0;
            UINT32 opcodeOffset = 0x4;

            isAnimPlayingFunction->executeFunction(isAnimPlayingFunction->params, &isAnimPlayingFunctionData,
                                                   animatedRefr, NULL, NULL, NULL, result, opcodeOffset);
            return (result != 0.0);
        }
        return false;
    }

    static RE::SCRIPT_FUNCTION* getDestroyedFunction;
    // TODO: CHECK: should be Double?
    static UINT32 getDestroyed(RE::StaticFunctionTag*, RE::TESObjectREFR* reference) {
        if (getDestroyedFunction) {
            RE::SCRIPT_FUNCTION::ScriptData getDestroyedFunctionData = {0x10CB, 0x0001, 0x0000};
            double result = 0.0;
            UINT32 opcodeOffset = 0x4;

            getDestroyedFunction->executeFunction(getDestroyedFunction->params, &getDestroyedFunctionData, reference,
                                                  NULL, NULL, NULL, result, opcodeOffset);
            return (result != 0.0);
        }
        return 0;
    }

    /*static UINT32 GetContainer(RE::TESObjectREFR* objectReference) {  // WTM:  Change:  Experimenting
        if (objectReference == NULL) {
            return 1;
        }
        RE::TESContainer* container = objectReference->GetContainer();
        if (container == NULL) {
            return 2;
        }
        return 3;
        // int num = container->numContainerObjects;
    }*/

    static RE::SCRIPT_FUNCTION* startConversationFunction;  // WTM:  Change:  Experimenting
    static BYTE startConversationScriptData[12];
    static RE::Script* initDummyStartConversationScript() {
        BYTE dummySCRIStartConversationAlloc[sizeof(RE::Script)] = {0};
        RE::Script* dummyStartConversationScript = (RE::Script*)&dummySCRIStartConversationAlloc;
        char startConversation[18] = "StartConversation";
        dummyStartConversationScript->formFlags = 0x000400a;
        dummyStartConversationScript->formID = 0xff000a18;
        dummyStartConversationScript->formType = (RE::FormType)0x13;
        dummyStartConversationScript->header.refObjectCount = 0x00000002;
        dummyStartConversationScript->header.dataSize = 0x0000000c;
        dummyStartConversationScript->header.variableCount = 0x00000000;
        dummyStartConversationScript->header.isQuestScript = 1;
        dummyStartConversationScript->text = startConversation;
        dummyStartConversationScript->data = (RE::SCRIPT_FUNCTION::ScriptData*)startConversationScriptData;
        return dummyStartConversationScript;
    }
    static void startConversation(
        RE::StaticFunctionTag*, RE::Actor* thisActor, RE::Actor* otherActor,
        RE::TESTopic* topic) {  // WTM:  Change:  Experimenting.  I'm trying to pass in arguments.
        if (startConversationFunction) {
            double result = 0.0;
            UINT32 opcodeOffset = 0x4;

            RE::SCRIPT_REFERENCED_OBJECT arg1;
            // memset(&arg1.form_name, 0, sizeof(BSString));
            arg1.editorID = "";
            arg1.form = otherActor;
            RE::SCRIPT_REFERENCED_OBJECT arg2;
            // memset(&arg2.form_name, 0, sizeof(BSString));
            arg2.editorID = "";
            arg2.form = topic;

            // BSSimpleList<SCRIPT_REFERENCED_OBJECT*> arglist2(&arg2, NULL);
            // BSSimpleList<SCRIPT_REFERENCED_OBJECT*> arglist1(&arg1, &arglist2);
            // RefListEntry arglist2;

            // arglist1.var = &arg1;
            // arglist2.var = &arg2;
            // arglist1.next = &arglist2;
            // arglist2.next = NULL;

            RE::BSSimpleList<RE::SCRIPT_REFERENCED_OBJECT*> reflist;
            reflist.push_front(&arg2);
            reflist.push_front(&arg1);

            RE::Script* dummyStartConversationScript = initDummyStartConversationScript();
            dummyStartConversationScript->refObjects = reflist;

            startConversationFunction->executeFunction(
                startConversationFunction->params, (RE::SCRIPT_FUNCTION::ScriptData*)startConversationScriptData,
                thisActor, NULL, dummyStartConversationScript, NULL, result, opcodeOffset);
        }
    }

    static bool ContainsItem(RE::StaticFunctionTag*, RE::TESObjectREFR* objectRef, RE::TESForm* soughtObject) {
        auto changes = objectRef->GetInventoryChanges();
        if (!changes || !changes->entryList) {
            return false;
        }

        for (auto& entry : *changes->entryList) {
            if (entry && entry->object && (entry->object == soughtObject || entry->object->formID == soughtObject->formID) && entry->countDelta > 0) {
                return true;
            }
        }

        return false;
    }
};

double ObjectReferenceUtility::g_silent_voice_duration_seconds;
int ObjectReferenceUtility::g_is_obscript_say_say_to;
BYTE ObjectReferenceUtility::startConversationScriptData[12] = {
    (BYTE)RE::FUNCTION_DATA::FunctionID::kStartConversation, 0x10, 0x8, 0x0, 0x2, 0x0, 0x72, 0x1, 0x0, 0x72, 0x2, 0x0};
BYTE ObjectReferenceUtility::sayScriptData[9] = {
    (BYTE)RE::FUNCTION_DATA::FunctionID::kSay, 0x10, 0x5, 0x0, 0x1, 0x0, 0x72, 0x1, 0x0};
BYTE ObjectReferenceUtility::sayToScriptData[12] = {
    (BYTE)RE::FUNCTION_DATA::FunctionID::kSayTo, 0x10, 0x8, 0x0, 0x2, 0x0, 0x72, 0x1, 0x0, 0x72, 0x2, 0x0};
RE::SCRIPT_FUNCTION* ObjectReferenceUtility::sayFunction = NULL;
RE::SCRIPT_FUNCTION* ObjectReferenceUtility::sayToFunction = NULL;
RE::SCRIPT_FUNCTION* ObjectReferenceUtility::isAnimPlayingFunction = NULL;
RE::SCRIPT_FUNCTION* ObjectReferenceUtility::getDestroyedFunction = NULL;
RE::SCRIPT_FUNCTION* ObjectReferenceUtility::startConversationFunction = NULL;