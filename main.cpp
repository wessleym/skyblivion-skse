/*#include "skse64_common/skse_version.h"  // RUNTIME_VERSION

#include "version.h"  // VERSION_VERSTRING, VERSION_MAJOR

#include "SKSE/API.h" //GENERAL SKSE API
#include "RE/CommandTable.h" //OBSCRIPT HOOKS
#include "RE/Skyrim.h" //SKYRIM PAPYRUS INTERFACE

#include "common/ITypes.h" //TYPES

#include <sstream>*/

#include <spdlog/sinks/basic_file_sink.h>
#include <stdfloat>
#include <stdint.h>
#include <windows.h>
#include "SKSE/Logger.h"
 
/* __declspec(dllimport) */double g_silent_voice_duration_seconds;
/* __declspec(dllimport) */int g_is_obscript_say_say_to;

//These used to be defined be SKSE, but they are no longer.
#define _MESSAGE(a_fmt, ...)	/*SKSE::Impl::ConsoleLogger::VPrint(__FILE__, __LINE__, SKSE::Logger::Level::kMessage, a_fmt, __VA_ARGS__)*/SKSE::log::info(a_fmt)
#define _ERROR(a_fmt, ...)		/*SKSE::Impl::ConsoleLogger::VPrint(__FILE__, __LINE__, SKSE::Logger::Level::kError, a_fmt, __VA_ARGS__)*/SKSE::log::error(a_fmt)
#define _FATALERROR(a_fmt, ...)	/*SKSE::Impl::ConsoleLogger::VPrint(__FILE__, __LINE__, SKSE::Logger::Level::kFatalError, a_fmt, __VA_ARGS__)*/SKSE::log::critical(a_fmt)

#ifndef SKYB_VERSION_INCLUDED
    #define SKYB_VERSION_INCLUDED

    #define MAKE_STR_HELPER(a_str) #a_str
    #define MAKE_STR(a_str) MAKE_STR_HELPER(a_str)

    #define SKYB_VERSION_MAJOR 1
    #define SKYB_VERSION_MINOR 0
    #define SKYB_VERSION_PATCH 0
    #define SKYB_VERSION_BETA 0
    #define SKYB_VERSION_VERSTRING   \
        MAKE_STR(SKYB_VERSION_MAJOR) \
        "." MAKE_STR(SKYB_VERSION_MINOR) "." MAKE_STR(SKYB_VERSION_PATCH) "." MAKE_STR(SKYB_VERSION_BETA)

#endif



//NativeFunction* stopFunction = NULL;

namespace RE {
	namespace ObScriptHooks {

		SCRIPT_FUNCTION* isAnimPlayingFunction = NULL;
		SCRIPT_FUNCTION::ScriptData isAnimPlayingFunctionData =
		{
			0x1128,
			0x0001,
			0x0000
		};

		SCRIPT_FUNCTION* getDestroyedFunction = NULL;
		SCRIPT_FUNCTION::ScriptData getDestroyedFunctionData =
		{
			0x10CB,
			0x0001,
			0x0000
		};

		SCRIPT_FUNCTION* waitFunction = NULL;//WTM:  Change:  Experimenting
		SCRIPT_FUNCTION::ScriptData waitFunctionData =
		{
			0x1000 + (int)FUNCTION_DATA::FunctionID::kWait,
			0x0001,
			0x0000
		};

		SCRIPT_FUNCTION* essentialDeathReloadFunction = NULL;//WTM:  Change:  Experimenting
		SCRIPT_FUNCTION::ScriptData essentialDeathReloadFunctionData =
		{
			0x1000 + (int)FUNCTION_DATA::FunctionID::kEssentialDeathReload,
			0x0001,
			0x0000
		};

		SCRIPT_FUNCTION* sayFunction = NULL;
		SCRIPT_FUNCTION* sayToFunction = NULL;
		SCRIPT_FUNCTION* startConversationFunction = NULL;//WTM:  Change:  Experimenting

		BYTE dummySCRISayToAlloc[sizeof(Script)] = { 0 };
		BYTE dummySCRISayAlloc[sizeof(Script)] = { 0 };
		BYTE dummySCRIStartConversationAlloc[sizeof(Script)] = { 0 };
		BYTE dummySCRIEssentialDeathReloadAlloc[sizeof(Script)] = { 0 };
		Script* dummySayToScript = (Script*)&dummySCRISayToAlloc;
		Script* dummySayScript = (Script*)&dummySCRISayAlloc;
		Script* dummyStartConversationScript = (Script*)&dummySCRIStartConversationAlloc;

		BYTE sayToScriptData[12] =                { (BYTE)FUNCTION_DATA::FunctionID::kSayTo            , 0x10, 0x8, 0x0, 0x2, 0x0, 0x72, 0x1, 0x0, 0x72, 0x2, 0x0 };
		BYTE sayScriptData[9] =                   { (BYTE)FUNCTION_DATA::FunctionID::kSay              , 0x10, 0x5, 0x0, 0x1, 0x0, 0x72, 0x1, 0x0 };
		BYTE startConversationScriptData[12] =    { (BYTE)FUNCTION_DATA::FunctionID::kStartConversation, 0x10, 0x8, 0x0, 0x2, 0x0, 0x72, 0x1, 0x0, 0x72, 0x2, 0x0 };

		void initDummySayToScript() {
            char sayTo[6] = "SayTo";
            dummySayToScript->formFlags = 0x000400a;
			dummySayToScript->formID = 0xff000a17;
			dummySayToScript->formType = (FormType)0x13;
			//dummySayToScript->pad13 = 0x0f;
			dummySayToScript->header.refObjectCount = 0x00000002;
			dummySayToScript->header.dataSize = 0x0000000c;
			dummySayToScript->header.variableCount = 0x00000000;
			dummySayToScript->header.isQuestScript = 1;
			//dummySayToScript->headertype = 0x00010000;
            dummySayToScript->text = sayTo;
			dummySayToScript->data = (SCRIPT_FUNCTION::ScriptData*)sayToScriptData;
		}

		void initDummySayScript() {
            char say[4] = "Say";
			dummySayScript->formFlags = 0x000400a;
			dummySayScript->formID = 0xff000e05;
			dummySayScript->formType = (FormType)0x13;
			//dummySayScript->pad13 = 0x00;
			dummySayScript->header.refObjectCount = 0x00000001;
			dummySayScript->header.dataSize = 0x00000009;
			dummySayScript->header.variableCount = 0x00000000;
			dummySayScript->header.isQuestScript = 1;
            dummySayScript->text = say;
			dummySayScript->data = (SCRIPT_FUNCTION::ScriptData*)sayScriptData;
		}

		void initDummyStartConversationScript() {
            char startConversation[18] = "StartConversation";
			dummyStartConversationScript->formFlags = 0x000400a;
			dummyStartConversationScript->formID = 0xff000a18;
			dummyStartConversationScript->formType = (FormType)0x13;
			dummyStartConversationScript->header.refObjectCount = 0x00000002;
			dummyStartConversationScript->header.dataSize = 0x0000000c;
			dummyStartConversationScript->header.variableCount = 0x00000000;
			dummyStartConversationScript->header.isQuestScript = 1;
            dummyStartConversationScript->text = startConversation;
			dummyStartConversationScript->data = (SCRIPT_FUNCTION::ScriptData*)startConversationScriptData;
		}


		float ObScriptSay(TESObjectREFR* thisActor, TESTopic* TopicID, bool value)
		{
			double result = 0.0;

			if (NULL != sayFunction)
			{


				//auto scriptFactory = IFormFactory::GetConcreteFormFactoryByType<Script>();
				//if (!scriptFactory) {
				//	return 0.0;
				//}

				//Script* script = scriptFactory->Create();
				//if (!script) {
				//	return 0.0;
				//}

				UINT32 opcodeOffset = 0x4;

				if (thisActor == NULL || TopicID == NULL)
					return 0.5;

				SCRIPT_REFERENCED_OBJECT arg2;
				//memset(&arg2.form_name, 0, sizeof(BSString));
				arg2.editorID = "";
				arg2.form = TopicID;

				//BSSimpleList<SCRIPT_REFERENCED_OBJECT*> arglist2(&arg2, NULL);
				//BSSimpleList<SCRIPT_REFERENCED_OBJECT*> arglist1(&arg1, &arglist2);
				//RefListEntry arglist2;

				//arglist1.var = &arg1;
				//arglist2.var = &arg2;
				//arglist1.next = &arglist2;
				//arglist2.next = NULL;


				BSSimpleList<SCRIPT_REFERENCED_OBJECT*> reflist;
				reflist.push_front(&arg2);

				initDummySayScript();
				dummySayScript->refObjects = reflist;

				g_is_obscript_say_say_to = true;
				g_silent_voice_duration_seconds = 5.0;

				sayFunction->executeFunction(
					sayFunction->params,
					(SCRIPT_FUNCTION::ScriptData*)sayScriptData,
					thisActor,
					NULL,
					dummySayScript,
					NULL,
					result,
					opcodeOffset
				);

				g_is_obscript_say_say_to = false;
				result = g_silent_voice_duration_seconds;
				return result + 0.5;

				//std::stringstream stream;
				//stream << "Say "
				//	<< std::setfill('0') << std::setw(8)
				//	<< std::hex << TopicID->formID;

				//_DMESSAGE(stream.str().c_str());

				//script->SetCommand(stream.str());

				//g_is_obscript_say_say_to = true;

				//script->Invoke(thisActor, RE::Script::InvokeType::kSysWindowCompileAndRun);

				//g_is_obscript_say_say_to = false;

				//result = g_silent_voice_duration_seconds;

				//delete script;
			}
			return result;
		}

		float ObScriptSayTo(TESObjectREFR* thisActor, Actor* anotherActor, TESTopic* TopicID, bool value)
		{
			double result = 0.0;

			if (NULL != sayToFunction)
			{


				//auto scriptFactory = IFormFactory::GetConcreteFormFactoryByType<Script>();
				//if (!scriptFactory) {
				//	return 0.0;
				//}

				//Script* script = scriptFactory->Create();
				//if (!script) {
				//	return 0.0;
				//}

				UINT32 opcodeOffset = 0x4;

				if (thisActor == NULL || anotherActor == NULL || TopicID == NULL)
					return 0.5;


				SCRIPT_REFERENCED_OBJECT arg1;
				//memset(&arg1.form_name, 0, sizeof(BSString));
				arg1.editorID = "";
				arg1.form = anotherActor;
				SCRIPT_REFERENCED_OBJECT arg2;
				//memset(&arg2.form_name, 0, sizeof(BSString));
				arg2.editorID = "";
				arg2.form = TopicID;

				//BSSimpleList<SCRIPT_REFERENCED_OBJECT*> arglist2(&arg2, NULL);
				//BSSimpleList<SCRIPT_REFERENCED_OBJECT*> arglist1(&arg1, &arglist2);
				//RefListEntry arglist2;

				//arglist1.var = &arg1;
				//arglist2.var = &arg2;
				//arglist1.next = &arglist2;
				//arglist2.next = NULL;


				BSSimpleList<SCRIPT_REFERENCED_OBJECT*> reflist;
				reflist.push_front(&arg2);
				reflist.push_front(&arg1);

				initDummySayToScript();
				dummySayToScript->refObjects = reflist;

				g_is_obscript_say_say_to = true;
				g_silent_voice_duration_seconds = 5.0;

				sayToFunction->executeFunction(
					sayToFunction->params,
					(SCRIPT_FUNCTION::ScriptData*)sayToScriptData,
					thisActor,
					NULL,
					dummySayToScript,
					NULL,
					result,
					opcodeOffset
				);

				g_is_obscript_say_say_to = false;
				result = g_silent_voice_duration_seconds;
				return result + 0.5;

				//std::stringstream stream;
				//stream << "Say "
				//	<< std::setfill('0') << std::setw(8)
				//	<< std::hex << TopicID->formID;

				//_DMESSAGE(stream.str().c_str());

				//script->SetCommand(stream.str());

				//g_is_obscript_say_say_to = true;

				//script->Invoke(thisActor, RE::Script::InvokeType::kSysWindowCompileAndRun);

				//g_is_obscript_say_say_to = false;

				//result = g_silent_voice_duration_seconds;

				//delete script;
			}
			return result;
		}

		bool prepareForReinitializing(TESQuest* a_form)
		{
			a_form->alreadyRun = false;
			//TODO: stop
			return true;
		}

		UINT32 getAmountSoldStolen(StaticFunctionTag*) {
            return PlayerCharacter::GetSingleton()->GetInfoRuntimeData().amountStolenSold;
		}

		void modAmountSoldStolen(StaticFunctionTag*, unsigned long amount) {
			PlayerCharacter::GetSingleton()->GetInfoRuntimeData().amountStolenSold += amount;//WTM:  Change:  Was amountStolenSold = amount.
		}

		UINT32 isPCAMurderer(StaticFunctionTag*) {
			return PlayerCharacter::GetSingleton()->GetGameStatsData().murder;
		}

		bool isAnimPlaying(TESObjectREFR* animatedRefr) {
			if (isAnimPlayingFunction)
			{
				double result = 0.0;
                UINT32 opcodeOffset = 0x4;
				
				isAnimPlayingFunction->executeFunction(
					isAnimPlayingFunction->params,
					&isAnimPlayingFunctionData,
					animatedRefr,
					NULL,
					NULL,
					NULL,
					result,
					opcodeOffset
				);
				return (result != 0.0);
			}
			return false;
		}

		//TODO: CHECK: should be Double?
        UINT32 getDestroyed(TESObjectREFR* reference) {
			if (getDestroyedFunction)
			{
				double result = 0.0;
                UINT32 opcodeOffset = 0x4;

				getDestroyedFunction->executeFunction(
					getDestroyedFunction->params,
					&getDestroyedFunctionData,
					reference,
					NULL,
					NULL,
					NULL,
					result,
					opcodeOffset
				);
				return (result != 0.0);
			}
			return 0;
		}

		void startConversation(Actor* thisActor, Actor* otherActor, TESTopic* topic) {//WTM:  Change:  Experimenting.  I'm trying to pass in arguments.
			if (startConversationFunction)
			{
				double result = 0.0;
                UINT32 opcodeOffset = 0x4;

				SCRIPT_REFERENCED_OBJECT arg1;
				//memset(&arg1.form_name, 0, sizeof(BSString));
				arg1.editorID = "";
				arg1.form = otherActor;
				SCRIPT_REFERENCED_OBJECT arg2;
				//memset(&arg2.form_name, 0, sizeof(BSString));
				arg2.editorID = "";
				arg2.form = topic;

				//BSSimpleList<SCRIPT_REFERENCED_OBJECT*> arglist2(&arg2, NULL);
				//BSSimpleList<SCRIPT_REFERENCED_OBJECT*> arglist1(&arg1, &arglist2);
				//RefListEntry arglist2;

				//arglist1.var = &arg1;
				//arglist2.var = &arg2;
				//arglist1.next = &arglist2;
				//arglist2.next = NULL;


				BSSimpleList<SCRIPT_REFERENCED_OBJECT*> reflist;
				reflist.push_front(&arg2);
				reflist.push_front(&arg1);

				initDummyStartConversationScript();
				dummyStartConversationScript->refObjects = reflist;

				startConversationFunction->executeFunction(
					startConversationFunction->params,
					(SCRIPT_FUNCTION::ScriptData*)startConversationScriptData,
					thisActor,
					NULL,
					dummyStartConversationScript,
					NULL,
					result,
					opcodeOffset
				);
			}
		}

		std::vector<float> GetSkillDataArray(StaticFunctionTag*, int mode) {
            int listSize = PlayerCharacter::PlayerSkills::Data::Skill::kTotal;
            PlayerCharacter* pPC = PlayerCharacter::GetSingleton();
            auto skills = pPC->GetInfoRuntimeData().skills->data->skills;
            std::vector<float> returnValue(listSize, 0.0f);
            for (int i = 0; i < listSize;i++) {
                auto skill = skills[i];
                returnValue[i] = mode == 0   ? skill.xp : mode == 1 ? skill.levelThreshold : (skill.xp / skill.levelThreshold) * 100;
            }
            return returnValue;
        }

		void ObScriptWait(Actor* reference, TESPackage* package) {//WTM:  Change:  Experimenting.  How do I pass in package?
			if (waitFunction)
			{
				double result = 0.0;
				UINT32 opcodeOffset = 0x4;

				waitFunction->executeFunction(
					waitFunction->params,
					&waitFunctionData,
					reference,
					NULL,
					NULL,
					NULL,
					result,
					opcodeOffset
				);
			}
		}

		UINT32 GetContainer(TESObjectREFR* objectReference) {//WTM:  Change:  Experimenting
			if (objectReference == NULL) { return 1; }
			TESContainer* container = objectReference->GetContainer();
			if (container == NULL) { return 2; }
			return 3;
			//int num = container->numContainerObjects;
		}

		void EssentialDeathReload(StaticFunctionTag*, const BSFixedString* message) {//WTM:  Change:  Experimenting.  How do I pass in message?
			if (essentialDeathReloadFunction)
			{
				double result = 0.0;
				UINT32 opcodeOffset = 0x4;

				essentialDeathReloadFunction->executeFunction(
					essentialDeathReloadFunction->params,
					&essentialDeathReloadFunctionData,
					NULL,
					NULL,
					NULL,
					NULL,
					result,
					opcodeOffset
				);
			}
		}

		bool RegisterFuncs(BSScript::Internal::VirtualMachine* a_vm) {

			_MESSAGE("Initializing ObScript hooks...");

			a_vm->RegisterFunction("LegacyGetAmountSoldStolen", "Game", getAmountSoldStolen);//WTM:  Change:  Renamed from GetAmountSoldStolen
			a_vm->RegisterFunction("LegacyModAmountSoldStolen", "Game", modAmountSoldStolen);//WTM:  Change:  Renamed from ModAmountSoldStolen
			a_vm->RegisterFunction("LegacyIsPCAMurderer", "Game", isPCAMurderer);//WTM:  Change:  Renamed from IsPCAMurderer

			a_vm->RegisterFunction("PrepareForReinitializing", "Quest", prepareForReinitializing);
			
			_MESSAGE("Looking for Say execute handler");
			sayFunction = SCRIPT_FUNCTION::LocateScriptCommand("Say");
			if (NULL == sayFunction)
			{
                _ERROR("Unable to find sayFunction!");
			}
			a_vm->RegisterFunction("LegacySay", "ObjectReference", ObScriptSay);

			_MESSAGE("Looking for SayTo execute handler");
			sayToFunction = SCRIPT_FUNCTION::LocateScriptCommand("SayTo");
			if (NULL == sayToFunction)
			{
                _ERROR("Unable to find sayToFunction!");
			}
			a_vm->RegisterFunction("LegacySayTo", "ObjectReference", ObScriptSayTo);

			_MESSAGE("Looking for IsAnimPlaying execute handler");
			isAnimPlayingFunction = SCRIPT_FUNCTION::LocateScriptCommand("IsAnimPlaying");
			if (NULL == isAnimPlayingFunction)
			{
                _ERROR("Unable to find isAnimPlayingFunction!");
			}
			a_vm->RegisterFunction("IsAnimPlaying", "ObjectReference", isAnimPlaying);

			_MESSAGE("Looking for GetDestroyed execute handler");
			getDestroyedFunction = SCRIPT_FUNCTION::LocateScriptCommand("GetDestroyed");
			if (NULL == getDestroyedFunction)
			{
                _ERROR("Unable to find getDestroyedFunction!");
			}
			a_vm->RegisterFunction("LegacyGetDestroyed", "ObjectReference", getDestroyed);

			/*a_vm->RegisterFunction("LegacyGetContainer", "ObjectReference", GetContainer);//WTM:  Change:  Experimenting

			waitFunction = SCRIPT_FUNCTION::LocateScriptCommand("Wait");//WTM:  Change:  Experimenting
			if (NULL == waitFunction)
			{
				_ERROR("Unable to find waitFunction!");
			}
			a_vm->RegisterFunction("LegacyWait", "Actor", ObScriptWait);

			essentialDeathReloadFunction = SCRIPT_FUNCTION::LocateScriptCommand("EssentialDeathReload");//WTM:  Change:  Experimenting
			if (NULL == essentialDeathReloadFunction)
			{
				_ERROR("Unable to find essentialDeathReloadFunction!");
			}
			a_vm->RegisterFunction("LegacyEssentialDeathReload", "Game", EssentialDeathReload);*/

			_MESSAGE("Looking for StartConversation execute handler");
			startConversationFunction = SCRIPT_FUNCTION::LocateScriptCommand("StartConversation");
			if (NULL == startConversationFunction)
			{
				_ERROR("Unable to find startConversationFunction!");
			}
			a_vm->RegisterFunction("LegacyStartConversation", "ObjectReference", startConversation);
			//WTM:  Note:  I think I got this to work, but it only seems to work when called on the player.
			//For example, PlayerRef.StartConversation(SomeActor_p, SomeTopic_p) works, but SomeActor_p.StartConversation(PlayerRef, SomeTopic_p) seems to do nothing.

			//WTM:  Note:  For Fallen's UI
            a_vm->RegisterFunction("GetSkillDataArray", "TES4SkillUtility", GetSkillDataArray);

			_MESSAGE("Initializing ObScript hooks done");

			return true;
		}
	}
}

//This method is no longer needed.
bool SKSEPlugin_QueryOld(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info) {
    /*
    SKSE::Logger::OpenRelative(FOLDERID_Documents, L"\\My Games\\Skyrim Special Edition\\SKSE\\Skyblivion.log");
    SKSE::Logger::SetPrintLevel(SKSE::Logger::Level::kDebugMessage);
    SKSE::Logger::SetFlushLevel(SKSE::Logger::Level::kDebugMessage);
    SKSE::Logger::UseLogStamp(true);
    */

    _MESSAGE("Skyblivion v%s", SKYB_VERSION_VERSTRING);

	//This is now set from CMakeLists.txt with add_commonlibsse_plugin. (SKSEPluginInfo could also be used.)
	//https://gitlab.com/colorglass/commonlibsse-sample-plugin#plugin-initialization
    a_info->infoVersion = SKSE::PluginInfo::kVersion;
    a_info->name = "Skyblivion";
    a_info->version = SKYB_VERSION_MAJOR;

    return true;
}


SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) {
        SKSE::stl::report_and_fail("!logsFolder");
    }
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);
    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));
    spdlog::set_default_logger(std::move(loggerPtr));
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);

    _MESSAGE("Skyblivion loaded");

    SKSE::Init(skse);

    if (skse->IsEditor()) {
        _FATALERROR("Loaded in editor. Marking as incompatible.");
        return false;
    }

    /*switch (a_skse->RuntimeVersion()) {
    case RUNTIME_VERSION_1_5_97:
        break;
    default:
        _FATALERROR("Unsupported runtime version %08X!\n", a_skse->RuntimeVersion());
        return false;
    }*/

    auto papyrus = SKSE::GetPapyrusInterface();
    if (!papyrus->Register(RE::ObScriptHooks::RegisterFuncs)) {
        _FATALERROR("Failed to register papyrus callback!");
        return false;
    }

    return true;
}