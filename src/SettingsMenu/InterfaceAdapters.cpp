#include "SettingsMenu/InterfaceAdapters.h"

#include "Platform/FormCache.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {
	using RE::BSScript::Object;
	using ObjectPtr = RE::BSTSmartPointer<Object>;

	constexpr const char* kMoreHudQuest = "ahzmainquest";        // quest editor ID = script name
	constexpr const char* kQuickLootQuest = "MCMQuickLootIE";
	constexpr const char* kQuickLootScript = "QuickLootIEMCM";
	constexpr std::string_view kQuickLootMaster = "SKYB_QuickLootEnabled";
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

	// the script object of a_script bound to the quest with editor ID a_quest (the quest resolves once; a
	// miss is logged once, which is expected when that mod is not installed)
	ObjectPtr BoundScript(const char* a_quest, const char* a_script) {
		auto* quest = FormCache::Get<RE::TESQuest>(a_quest, "interface (optional mod's quest)");
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

	// every script object of a_script, on whichever quests carry it (Skyblivion.esm and QuickLootIE.esp both
	// have a quest with QuickLootIEMCM; QuickLoot reads either, so each gets the values). The quests are found
	// once, the first time any is, and kept.
	std::vector<ObjectPtr> AllBound(const char* a_script) {
		static std::unordered_map<std::string, std::vector<RE::TESQuest*>> cache;
		std::vector<ObjectPtr> out;
		auto* vm = VM();
		auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
		auto* dh = RE::TESDataHandler::GetSingleton();
		if (!policy || !dh) {
			return out;
		}
		auto find = [&](RE::TESQuest* a_quest) {
			const auto handle = a_quest ? policy->GetHandleForObject(RE::TESQuest::FORMTYPE, a_quest) : 0;
			ObjectPtr  obj;
			if (handle && vm->FindBoundObject(handle, a_script, obj) && obj) {
				out.push_back(obj);
				return true;
			}
			return false;
		};
		auto& quests = cache[a_script];
		if (!quests.empty()) {
			for (auto* q : quests) {
				find(q);
			}
			return out;
		}
		for (auto* q : dh->GetFormArray<RE::TESQuest>()) {
			if (find(q)) {
				quests.push_back(q);
			}
		}
		if (!quests.empty()) {
			Log::INFO("interface: {} is on {} quest(s)", a_script, quests.size());
		}
		return out;
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
	auto* global = FormCache::Get<RE::TESGlobal>(a_editorID, "interface");
	// AHZShowEnemyMagickaMeter is not found by editor ID in game (its stamina twin is); moreHUD itself reads
	// its globals through ahzmainquest's properties of the same names, so take it from there, once
	if (!global && a_editorID.starts_with("AHZ")) {
		if (auto hud = BoundScript(kMoreHudQuest, kMoreHudQuest)) {
			if (auto* var = hud->GetProperty(std::string(a_editorID).c_str()); var && var->IsObject()) {
				global = var->Unpack<RE::TESGlobal*>();
			}
		}
		if (global) {
			FormCache::Keep(a_editorID, global);
			Log::INFO("interface: {} taken from moreHUD's property ({:08X})", a_editorID, global->GetFormID());
		}
	}
	if (!global) {
		Log::WARN("interface: GlobalVariable {} not found", a_editorID);
		return;
	}
	if (global->value != a_value) {
		global->value = a_value;
		//only moreHUD's globals (AHZ*) need UpdateSettings; other globals are read where they are used
		if (a_editorID.starts_with("AHZ")) {
			_moreHudDirty = true;
			ScheduleFlush();
		}
	}
}

// QuickLoot IE has no master switch, so SKYB_QuickLootEnabled drives its four EnableFor* properties:
// off writes them false; the rows' values (default on) are kept and written back when it is on again.
void InterfaceAdapters::SetQuickLoot(std::string_view a_property, float a_value) {
	static constexpr std::array<const char*, 4> kEnableFor{ "QLIE_EnableForContainers", "QLIE_EnableForCorpses",
		"QLIE_EnableForAnimals", "QLIE_EnableForDragons" };
	if (a_property == kQuickLootMaster) {
		_quickLootOn = a_value >= 0.5f;
		for (const char* name : kEnableFor) {
			const auto it = _quickLootWanted.find(name);
			_quickLoot[name] = _quickLootOn ? (it != _quickLootWanted.end() ? it->second : 1.0f) : 0.0f;
		}
		Log::INFO("interface: QuickLoot {}", _quickLootOn ? "on" : "off (every EnableFor written false)");
	} else if (std::find_if(kEnableFor.begin(), kEnableFor.end(), [&](const char* n) { return a_property == n; }) != kEnableFor.end()) {
		_quickLootWanted[std::string(a_property)] = a_value;
		_quickLoot[std::string(a_property)] = _quickLootOn ? a_value : 0.0f;
	} else {
		_quickLoot[std::string(a_property)] = a_value;
	}
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
		const auto all = AllBound(kQuickLootScript);
		const bool ready = !all.empty() && std::all_of(all.begin(), all.end(), [](const ObjectPtr& a) { return QuickLootInitialised(a); });
		if (!ready) {
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
			for (const auto& ql : all) {
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
				_quickLootApplied[name] = value;
				++written;
			}
			}
			_quickLoot.clear();
			_quickLootDirty = false;
			auto* args = RE::MakeFunctionArguments();
			vm->DispatchStaticCall("QuickLootIENative", "UpdateVariables", args, none);
			Log::INFO("interface: QuickLoot - {} setting(s) written on {} script(s), UpdateVariables called", written, all.size());
			ScheduleRecheck();
		}
	}
}

// two looks, 3 and 8 seconds on: QuickLoot's late setup comes within a few seconds of the game starting
void InterfaceAdapters::ScheduleRecheck() {
	for (const auto delay : { std::chrono::seconds(3), std::chrono::seconds(8) }) {
		std::thread([this, delay] {
			std::this_thread::sleep_for(delay);
			if (auto* tasks = SKSE::GetTaskInterface()) {
				tasks->AddTask([this] { Recheck(); });
			}
		}).detach();
	}
}

void InterfaceAdapters::Recheck() {
	if (_quickLootApplied.empty() || _quickLootDirty) {
		return;  // nothing written yet, or a write is still on its way (it schedules its own looks)
	}
	auto*      vm = VM();
	const auto all = AllBound(kQuickLootScript);
	if (!vm || all.empty()) {
		return;
	}
	std::size_t rewritten = 0;
	for (const auto& ql : all) {
	if (!QuickLootInitialised(ql)) {
		continue;
	}
	for (const auto& [name, value] : _quickLootApplied) {
		auto* var = ql->GetProperty(name.c_str());
		if (!var) {
			continue;
		}
		if (var->IsBool() && var->GetBool() != (value >= 0.5f)) {
			var->SetBool(value >= 0.5f);
		} else if (var->IsInt() && var->GetSInt() != static_cast<std::int32_t>(std::lround(value))) {
			var->SetSInt(static_cast<std::int32_t>(std::lround(value)));
		} else if (var->IsFloat() && std::abs(var->GetFloat() - value) > 1e-4f) {
			var->SetFloat(value);
		} else {
			continue;
		}
		++rewritten;
	}
	}
	if (rewritten > 0) {
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none;
		auto* args = RE::MakeFunctionArguments();
		vm->DispatchStaticCall("QuickLootIENative", "UpdateVariables", args, none);
		Log::INFO("interface: QuickLoot had put back {} setting(s) after they were written - written again", rewritten);
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
		// every quest that carries the page's script (QuickLoot's is on two)
		for (const auto& config : AllBound(page.script)) {
			// returns -1 (nothing to do) once the page is gone; harmless to repeat
			vm->DispatchMethodCall(manager, "UnregisterMod", new ObjectArgument(config), none);
		}
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
			} else {
				Recheck();
			}
		});
	}
	return RE::BSEventNotifyControl::kContinue;
}
