#include "ObjectReferenceUtility.h"

void ObjectReferenceUtility::Register(RE::BSScript::Internal::VirtualMachine* vm) {
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
	//SKSEScriptRegistrar::Register(vm, className, "LegacyStartConversation", startConversation);
	// WTM:  Note:  I think I got this to work, but it only seems to work when called on the player.
	// For example, PlayerRef.StartConversation(SomeActor_p, SomeTopic_p) works, but
	// SomeActor_p.StartConversation(PlayerRef, SomeTopic_p) seems to do nothing.

	// I'm measuring performance in SKYBObjectReferenceUtility.psc for the below.
	SKSEScriptRegistrar::Register(vm, className, "ContainsItem2", ContainsItem);
}

RE::Script* ObjectReferenceUtility::initDummySayScript() {
	static std::array<std::uint8_t, sizeof(RE::Script)> dummySCRISayAlloc{};
	RE::Script* dummySayScript = reinterpret_cast<RE::Script*>(dummySCRISayAlloc.data());
	static char say[4] = "Say";
	dummySayScript->formFlags = 0x000400a;
	dummySayScript->formID = 0xff000e05;
	dummySayScript->formType = static_cast<RE::FormType>(0x13);
	// dummySayScript->pad13 = 0x00;
	dummySayScript->header.refObjectCount = 0x00000001;
	dummySayScript->header.dataSize = 0x00000009;
	dummySayScript->header.variableCount = 0x00000000;
	dummySayScript->header.isQuestScript = 1;
	dummySayScript->text = say;
	dummySayScript->data = reinterpret_cast<RE::SCRIPT_FUNCTION::ScriptData*>(sayScriptData.data());
	return dummySayScript;
}

float ObjectReferenceUtility::ObScriptSay(RE::StaticFunctionTag*, RE::TESObjectREFR* thisActor, RE::TESTopic* TopicID, bool /*value*/) {
	if (sayFunction == nullptr) { return 0; }
	if (thisActor == nullptr || TopicID == nullptr) return 0.5;

	RE::SCRIPT_REFERENCED_OBJECT arg2;
	arg2.editorID = "";
	arg2.form = TopicID;

	RE::BSSimpleList<RE::SCRIPT_REFERENCED_OBJECT*> reflist;
	reflist.push_front(&arg2);

	RE::Script* dummySayScript = initDummySayScript();
	dummySayScript->refObjects = reflist;

	g_is_obscript_say_say_to = true;
	g_silent_voice_duration_seconds = 5.0;

	double result = 0;
	std::uint32_t opcodeOffset = 0x4;
	sayFunction->executeFunction(sayFunction->params, reinterpret_cast<RE::SCRIPT_FUNCTION::ScriptData*>(sayScriptData.data()), thisActor, nullptr, dummySayScript, nullptr, result, opcodeOffset);

	g_is_obscript_say_say_to = false;
	if (result == 0) { result = g_silent_voice_duration_seconds; }
	return static_cast<float>(result + 0.5);
}

RE::Script* ObjectReferenceUtility::initDummySayToScript() {
	static std::array<std::uint8_t, sizeof(RE::Script)> dummySCRISayToAlloc{};
	RE::Script* dummySayToScript = reinterpret_cast<RE::Script*>(dummySCRISayToAlloc.data());
	static char sayTo[6] = "SayTo";
	dummySayToScript->formFlags = 0x000400a;
	dummySayToScript->formID = 0xff000a17;
	dummySayToScript->formType = static_cast<RE::FormType>(0x13);
	// dummySayToScript->pad13 = 0x0f;
	dummySayToScript->header.refObjectCount = 0x00000002;
	dummySayToScript->header.dataSize = 0x0000000c;
	dummySayToScript->header.variableCount = 0x00000000;
	dummySayToScript->header.isQuestScript = 1;
	// dummySayToScript->headertype = 0x00010000;
	dummySayToScript->text = sayTo;
	dummySayToScript->data = reinterpret_cast<RE::SCRIPT_FUNCTION::ScriptData*>(sayToScriptData.data());
	return dummySayToScript;
}

float ObjectReferenceUtility::ObScriptSayTo(RE::StaticFunctionTag*, RE::TESObjectREFR* thisActor, RE::Actor* anotherActor, RE::TESTopic* TopicID, bool/*value*/) {
	if (sayToFunction == nullptr) { return 0; }
	if (thisActor == nullptr || anotherActor == nullptr || TopicID == nullptr) return 0.5;

	RE::SCRIPT_REFERENCED_OBJECT arg1;
	arg1.editorID = "";
	arg1.form = anotherActor;
	RE::SCRIPT_REFERENCED_OBJECT arg2;
	arg2.editorID = "";
	arg2.form = TopicID;

	RE::BSSimpleList<RE::SCRIPT_REFERENCED_OBJECT*> reflist;
	reflist.push_front(&arg2);
	reflist.push_front(&arg1);

	RE::Script* dummySayToScript = initDummySayToScript();
	dummySayToScript->refObjects = reflist;

	g_is_obscript_say_say_to = true;
	g_silent_voice_duration_seconds = 5.0;
	double result = 0;
	std::uint32_t opcodeOffset = 0x4;

	sayToFunction->executeFunction(sayToFunction->params, reinterpret_cast<RE::SCRIPT_FUNCTION::ScriptData*>(sayToScriptData.data()), thisActor, nullptr, dummySayToScript, nullptr, result, opcodeOffset);

	g_is_obscript_say_say_to = false;
	if (result == 0) { result = g_silent_voice_duration_seconds; }
	return static_cast<float>(result + 0.5);
}

bool ObjectReferenceUtility::isAnimPlaying(RE::StaticFunctionTag*, RE::TESObjectREFR* animatedRefr) {
	if (isAnimPlayingFunction) {
		RE::SCRIPT_FUNCTION::ScriptData isAnimPlayingFunctionData = { 0x1128, 0x0001, 0x0000 };
		double result = 0.0;
		std::uint32_t opcodeOffset = 0x4;

		isAnimPlayingFunction->executeFunction(isAnimPlayingFunction->params, &isAnimPlayingFunctionData,
			animatedRefr, nullptr, nullptr, nullptr, result, opcodeOffset);
		return (result != 0.0);
	}
	return false;
}

std::uint32_t ObjectReferenceUtility::getDestroyed(RE::StaticFunctionTag*, RE::TESObjectREFR* reference) {
	if (getDestroyedFunction) {
		RE::SCRIPT_FUNCTION::ScriptData getDestroyedFunctionData = { 0x10CB, 0x0001, 0x0000 };
		double result = 0.0;
		std::uint32_t opcodeOffset = 0x4;

		getDestroyedFunction->executeFunction(getDestroyedFunction->params, &getDestroyedFunctionData, reference,
			nullptr, nullptr, nullptr, result, opcodeOffset);
		return (result != 0.0);
	}
	return 0;
}

RE::Script* ObjectReferenceUtility::initDummyStartConversationScript() {
	static std::array<std::uint8_t, sizeof(RE::Script)> dummySCRIStartConversationAlloc{};
	RE::Script* dummyStartConversationScript = reinterpret_cast<RE::Script*>(dummySCRIStartConversationAlloc.data());
	static char startConversation[18] = "StartConversation";
	dummyStartConversationScript->formFlags = 0x000400a;
	dummyStartConversationScript->formID = 0xff000a18;
	dummyStartConversationScript->formType = static_cast<RE::FormType>(0x13);
	dummyStartConversationScript->header.refObjectCount = 0x00000002;
	dummyStartConversationScript->header.dataSize = 0x0000000c;
	dummyStartConversationScript->header.variableCount = 0x00000000;
	dummyStartConversationScript->header.isQuestScript = 1;
	dummyStartConversationScript->text = startConversation;
	dummyStartConversationScript->data = reinterpret_cast<RE::SCRIPT_FUNCTION::ScriptData*>(startConversationScriptData.data());
	return dummyStartConversationScript;
}

void ObjectReferenceUtility::startConversation(RE::StaticFunctionTag*, RE::Actor* thisActor, RE::Actor* otherActor,
	RE::TESTopic* topic) {
	if (startConversationFunction) {
		double result = 0.0;
		std::uint32_t opcodeOffset = 0x4;

		RE::SCRIPT_REFERENCED_OBJECT arg1;
		arg1.editorID = "";
		arg1.form = otherActor;
		RE::SCRIPT_REFERENCED_OBJECT arg2;
		arg2.editorID = "";
		arg2.form = topic;

		RE::BSSimpleList<RE::SCRIPT_REFERENCED_OBJECT*> reflist;
		reflist.push_front(&arg2);
		reflist.push_front(&arg1);

		RE::Script* dummyStartConversationScript = initDummyStartConversationScript();
		dummyStartConversationScript->refObjects = reflist;

		startConversationFunction->executeFunction(
			startConversationFunction->params, reinterpret_cast<RE::SCRIPT_FUNCTION::ScriptData*>(startConversationScriptData.data()),
			thisActor, nullptr, dummyStartConversationScript, nullptr, result, opcodeOffset);
	}
}

bool ObjectReferenceUtility::ContainsItem(RE::StaticFunctionTag*, RE::TESObjectREFR* objectRef, RE::TESForm* soughtObject) {
	if (!objectRef || !soughtObject) {
		return false;
	}
	auto soughtObjectAsBoundObject = soughtObject->As<RE::TESBoundObject>();
	if (!soughtObjectAsBoundObject) {
		return false;
	}
	auto inventory = objectRef->GetInventory();
	return inventory.contains(soughtObjectAsBoundObject);
}
