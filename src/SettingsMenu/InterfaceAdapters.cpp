// Moved from SKYBUIDemo.dll into Skyblivion.dll, 2026-10-05 (fold-in, UI integration workspace
// ledger L-0056). Source of record until then: game\native\SKYBUIDemo\src in that workspace.
#include "SettingsMenu/InterfaceAdapters.h"

namespace {
	using RE::BSScript::Object;
	using ObjectPtr = RE::BSTSmartPointer<Object>;

	constexpr const char* kMoreHudQuest = "ahzmainquest";        // quest editor ID = script name
	constexpr const char* kQuickLootQuest = "MCMQuickLootIE";
	constexpr const char* kQuickLootScript = "QuickLootIEMCM";
	constexpr const char* kManagerQuest = "SKI_ConfigManagerInstance";
	constexpr const char* kManagerScript = "SKI_ConfigManager";

	struct HiddenPage {
		const char* quest;   // editor ID of the quest the config script sits on
		const char* script;  // that script
	};
	constexpr std::array<HiddenPage, 2> kHiddenPages{{
		{"SKI_ConfigMenuInstance", "SKI_ConfigMenu"},  // SkyUI's own page
		{kQuickLootQuest, kQuickLootScript},           // QuickLoot IE
	}};

	RE::BSScript::Internal::VirtualMachine* VM() { return RE::BSScript::Internal::VirtualMachine::GetSingleton(); }

	// the script object of a_script bound to the quest with editor ID a_quest
	ObjectPtr BoundScript(const char* a_quest, const char* a_script) {
		auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>(a_quest);
		auto* vm = VM();
		if (!quest || !vm) {
			return nullptr;
		}
		auto* policy = vm->GetObjectHandlePolicy();
		const auto handle = policy ? policy->GetHandleForObject(RE::TESQuest::FORMTYPE, quest) : 0;
		ObjectPtr obj;
		if (!handle || !vm->FindBoundObject(handle, a_script, obj)) {
			return nullptr;
		}
		return obj;
	}

	// QuickLootIEMCM clears IsInitialLoad once OnConfigInit has imported/reset
	// its settings; before that anything written would be overwritten
	bool QuickLootInitialised(const ObjectPtr& a_ql) {
		const auto* flag = a_ql->GetProperty("IsInitialLoad");
		return !flag || !flag->IsBool() || !flag->GetBool();
	}

	// one script-object argument (SKI_ConfigManager.UnregisterMod(SKI_ConfigBase))
	class ObjectArgument : public RE::BSScript::IFunctionArguments {
	public:
		explicit ObjectArgument(ObjectPtr a_obj) : _obj(std::move(a_obj)) {}
		bool operator()(RE::BSScrapArray<RE::BSScript::Variable>& a_dst) const override {
			a_dst.resize(1);
			a_dst[0].SetObject(_obj);
			return true;
		}

	private:
		ObjectPtr _obj;
	};
}

InterfaceAdapters* InterfaceAdapters::GetSingleton() {
	static InterfaceAdapters singleton;
	return &singleton;
}

void InterfaceAdapters::Register() {
	if (auto src = SKSE::GetModCallbackEventSource()) {
		src->AddEventSink(this);
	}
	Log::INFO("interface: adapters ready (moreHUD globals, QuickLoot properties, MCM pages hidden: {})", kHiddenPages.size());
}

void InterfaceAdapters::SetGlobal(std::string_view a_editorID, float a_value) {
	auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(a_editorID);
	if (!global) {
		Log::WARN("interface: GlobalVariable {} not found", a_editorID);
		return;
	}
	if (global->value != a_value) {
		global->value = a_value;
		_moreHudDirty = true;
		ScheduleFlush();
	}
}

void InterfaceAdapters::SetQuickLoot(std::string_view a_property, float a_value) {
	_quickLoot[std::string(a_property)] = a_value;
	_quickLootDirty = true;
	ScheduleFlush();
}

void InterfaceAdapters::ScheduleFlush() {
	if (_flushQueued) {
		return;
	}
	_flushQueued = true;
	SKSE::GetTaskInterface()->AddTask([this] { Flush(); });
}

// Once per frame of changes: refresh moreHUD through its own UpdateSettings,
// write the pending QuickLoot properties and let its DLL re-read them.
void InterfaceAdapters::Flush() {
	_flushQueued = false;
	auto* vm = VM();
	if (!vm) {
		return;
	}
	RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none;

	if (_moreHudDirty) {
		if (auto hud = BoundScript(kMoreHudQuest, kMoreHudQuest)) {
			_moreHudDirty = false;
			auto* args = RE::MakeFunctionArguments(false);
			vm->DispatchMethodCall(hud, "UpdateSettings", args, none);
			Log::INFO("interface: moreHUD settings refreshed");
		}
		// no game loaded yet: the next load's apply refreshes it
	}

	if (_quickLootDirty) {
		auto ql = BoundScript(kQuickLootQuest, kQuickLootScript);
		if (!ql || !QuickLootInitialised(ql)) {
			// its first-time init would overwrite us: keep the values for the
			// next SKICP_configManagerReady
			if (!_quickLootWaitLogged) {
				Log::INFO("interface: QuickLoot not initialised yet - {} setting(s) wait for it", _quickLoot.size());
				_quickLootWaitLogged = true;
			}
		} else {
			_quickLootWaitLogged = false;
			std::size_t written = 0;
			for (const auto& [name, value] : _quickLoot) {
				auto* var = ql->GetProperty(name.c_str());
				if (!var) {
					Log::WARN("interface: QuickLoot has no property {}", name);
					continue;
				}
				if (var->IsBool()) {
					var->SetBool(value >= 0.5f);
				} else if (var->IsInt()) {
					var->SetSInt(static_cast<std::int32_t>(std::lround(value)));
				} else if (var->IsFloat()) {
					var->SetFloat(value);
				} else {
					Log::WARN("interface: QuickLoot property {} has an unexpected type", name);
					continue;
				}
				++written;
			}
			_quickLoot.clear();
			_quickLootDirty = false;
			auto* args = RE::MakeFunctionArguments();
			vm->DispatchStaticCall("QuickLootIENative", "UpdateVariables", args, none);
			Log::INFO("interface: QuickLoot - {} setting(s) written, UpdateVariables called", written);
		}
	}
}

void InterfaceAdapters::HideMcmPages() {
	auto* vm = VM();
	auto  manager = BoundScript(kManagerQuest, kManagerScript);
	if (!vm || !manager) {
		return;
	}
	RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none;
	for (const auto& page : kHiddenPages) {
		auto config = BoundScript(page.quest, page.script);
		if (!config) {
			continue;
		}
		// returns -1 (nothing to do) once the page is gone; harmless to repeat
		vm->DispatchMethodCall(manager, "UnregisterMod", new ObjectArgument(config), none);
	}
}

RE::BSEventNotifyControl InterfaceAdapters::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
														 RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
	// SkyUI sends this after every load and then every 5-30 s; config pages
	// register in answer to it, so the next one after a registration removes it.
	// It is also the retry for QuickLoot writes held back until its init ran.
	if (a_event && a_event->eventName == "SKICP_configManagerReady") {
		SKSE::GetTaskInterface()->AddTask([this] {
			HideMcmPages();
			if (_quickLootDirty) {
				ScheduleFlush();
			}
		});
	}
	return RE::BSEventNotifyControl::kContinue;
}
