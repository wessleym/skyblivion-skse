#include "DialogueServices.h"
#include "DialogueContract.h"
#include "Degradation/RepairMenu.h"
#include "Platform/FormCache.h"
#include "Platform/IniFile.h"
#include "Platform/Text.h"

#include <algorithm>
#include <string_view>
#include <vector>

namespace Dialogue {

	namespace {
		struct ServiceTopic {
			std::string editorID;
			std::string icon;
		};

		struct ServiceDef {
			std::string id;
			std::vector<ServiceTopic> topics;
			std::vector<std::string> factions;  //editor IDs
			bool fallback = false;
			std::string openClass;
			std::string openFunction;
		};

		//"EditorID|Icon"; the icon is optional.
		ServiceTopic MakeTopic(const std::string& spec) {
			const auto parts = Text::Split(spec, '|');
			return { parts.empty() ? std::string{} : parts[0], parts.size() > 1 ? parts[1] : std::string{} };
		}

		std::vector<ServiceDef> BuiltInServices() {
			std::vector<ServiceDef> services;

			ServiceDef barter;
			barter.id = "barter";
			for (const char* spec : {
					 "SKYBDialGenericForSaleTopic",
					 "SKYBWERoadDealer01SaleTopic",
				 }) {
				barter.topics.push_back(MakeTopic(spec));
			}
			for (const char* spec : {
					 "SKYBJobMerchantFaction",
					 "SKYBWEServicesMerchantFaction",
				 }) {
				barter.factions.push_back(spec);
			}
			services.push_back(std::move(barter));

			ServiceDef training;
			training.id = "training";
			for (const char* spec : {
					 "TES4TrainingAlchemyTopic|Alchemy",
					 "TES4TrainingAlterationTopic|Alteration",
					 "TES4TrainingArcheryTopic|Archery",
					 "TES4TrainingBlockTopic|Block",
					 "TES4TrainingConjurationTopic|Conjuration",
					 "TES4TrainingDestructionTopic|Destruction",
					 "TES4TrainingEnchantingTopic|Enchanting",
					 "TES4TrainingHeavyArmorTopic|HeavyArmor",
					 "TES4TrainingIllusionTopic|Illusion",
					 "TES4TrainingLightArmorTopic|LightArmor",
					 "TES4TrainingLockpickingTopic|Lockpicking",
					 "TES4TrainingOneHandedTopic|OneHanded",
					 "TES4TrainingPickpocketTopic|Pickpocket",
					 "TES4TrainingRestorationTopic|Restoration",
					 "TES4TrainingSmithingTopic|Smithing",
					 "TES4TrainingSneakTopic|Sneak",
					 "SKYBDialGenericTrainSpeechTopic|Speech",
					 "TES4TrainingTwoHandedTopic|TwoHanded",
				 }) {
				training.topics.push_back(MakeTopic(spec));
			}
			for (const char* spec : {
					 "SKYBTrainerFactionAlchemy",
					 "SKYBTrainerFactionAlteration",
					 "SKYBTrainerFactionMarksman",
					 "SKYBTrainerFactionBlock",
					 "SKYBTrainerFactionConjuration",
					 "SKYBTrainerFactionDestruction",
					 "SKYBTrainerFactionEnchanting",
					 "SKYBTrainerFactionHeavyArmor",
					 "SKYBTrainerFactionIllusion",
					 "SKYBTrainerFactionLightArmor",
					 "SKYBTrainerFactionLockpicking",
					 "SKYBTrainerFactionOneHanded",
					 "SKYBTrainerFactionPickpocket",
					 "SKYBTrainerFactionRestoration",
					 "SKYBTrainerFactionSmithing",
					 "SKYBFactionTrainerSneak",
					 "SKYBTrainerFactionSpeechcraft",
					 "SKYBTrainerFactionTwoHanded",
				 }) {
				training.factions.push_back(spec);
			}
			services.push_back(std::move(training));

			//Repair and recharge have no topic: the button shows for any member of their factions.
			ServiceDef repair;
			repair.id = "repair";
			repair.fallback = true;
			repair.factions.push_back("SKYBRepairFaction");
			repair.factions.push_back("SKYBRepairmanFaction");
			services.push_back(std::move(repair));

			ServiceDef recharge;
			recharge.id = "recharge";
			recharge.fallback = true;
			recharge.factions.push_back("SKYBRechargeFaction");
			services.push_back(std::move(recharge));

			return services;
		}

		void ApplyServicesIni(std::vector<ServiceDef>& services) {
			ServiceDef* current = nullptr;
			const bool read = IniFile::Read(DialogueContract::ServicesIniFile, "DialogueServices", [&](const IniFile::Entry& a_entry) {
				if (a_entry.section.empty()) {
					Log::WARN("DialogueServices: {}({}) ignored: no [service] above it.", DialogueContract::ServicesIniFile, a_entry.line);
					return;
				}
				if (!current || current->id != a_entry.section) {
					const auto found = std::ranges::find(services, a_entry.section, &ServiceDef::id);
					if (found != services.end()) {
						current = &*found;
					} else {
						current = &services.emplace_back();
						current->id = a_entry.section;
						Log::INFO("DialogueServices: New service '{}' from the INI (the menu must know it too).", current->id);
					}
				}
				const auto& key = a_entry.key;
				const auto& value = a_entry.value;
				if (key == "clear") {
					if (value == "1") {
						current->topics.clear();
						current->factions.clear();
					}
				}
				else if (key == "topics") {
					for (const auto& spec : Text::Split(value, ',')) {
						current->topics.push_back(MakeTopic(spec));
					}
				}
				else if (key == "factions") {
					for (const auto& spec : Text::Split(value, ',')) {
						current->factions.push_back(spec);
					}
				}
				else if (key == "fallback") {
					current->fallback = value == "1" || value == "true";
				}
				else if (key == "open") {
					const auto dot = value.rfind('.');
					if (dot == std::string::npos) {
						Log::WARN("DialogueServices: [{}] open='{}' is not Class.Function.", current->id, value);
					}
					else {
						current->openClass = value.substr(0, dot);
						current->openFunction = value.substr(dot + 1);
					}
				}
				else {
					Log::WARN("DialogueServices: [{}] unknown key '{}'.", current->id, key);
				}
			});
			if (read) {
				Log::INFO("DialogueServices: Read {}.", DialogueContract::ServicesIniFile);
			}
		}

		//Built on first use, after data load.
		std::vector<ServiceDef>& Services() {
			static std::vector<ServiceDef> services;
			static bool built = false;
			if (!built) {
				built = true;
				services = BuiltInServices();
				ApplyServicesIni(services);
				for (const auto& service : services) {
					Log::INFO("DialogueServices: [{}] {} topic(s), {} faction(s), fallback={}, open={}.",
						service.id, service.topics.size(), service.factions.size(), service.fallback,
						service.openClass.empty() ? std::string("(built-in)") : service.openClass + "." + service.openFunction);
				}
			}
			return services;
		}

		bool InServiceFaction(RE::Actor* speaker, ServiceDef& service) {
			if (!speaker) {
				return false;
			}
			for (const auto& faction : service.factions) {
				if (auto* form = FormCache::Get<RE::TESFaction>(faction, "DialogueServices"); form && speaker->IsInFaction(form)) {
					return true;
				}
			}
			return false;
		}

		bool HasBuiltIn(const ServiceDef& service) {
			return service.id == "barter" || service.id == "training" || service.id == "repair" || service.id == "recharge";
		}

		//Whether the fallback path has anything to call: an open= from the table, or a built-in call.
		bool CanOpen(const ServiceDef& service) {
			return !service.openClass.empty() || HasBuiltIn(service);
		}

		//The SKYBDegradation natives of earlier versions (OpenVendorRepair / OpenVendorRecharge; no longer registered,
		//2026-11-06) for the service that has the same built-in call: the build's INI still names them.
		bool OwnNative(const ServiceDef& service) {
			if (!Text::IEquals(service.openClass, "SKYBDegradation")) {
				return false;
			}
			return (service.id == "repair" && Text::IEquals(service.openFunction, "OpenVendorRepair")) ||
				   (service.id == "recharge" && Text::IEquals(service.openFunction, "OpenVendorRecharge"));
		}

		//Whether Scripts\<class>.pex is in the data (loose or in an archive).
		bool ScriptInstalled(const std::string& className) {
			RE::BSResourceNiBinaryStream pex{ std::format("Scripts/{}.pex", className) };
			return pex.good();
		}

		//The first row of the dialogue list holding one of the service's topics, with that topic's icon.
		int FindRow(ServiceDef& service, std::string& icon) {
			auto* topicManager = RE::MenuTopicManager::GetSingleton();
			if (!topicManager || !topicManager->dialogueList) {
				return -1;
			}
			int row = 0;
			for (auto* entry : *topicManager->dialogueList) {
				if (entry && entry->parentTopic) {
					for (const auto& topic : service.topics) {
						if (FormCache::Get<RE::TESTopic>(topic.editorID, "DialogueServices") == entry->parentTopic) {
							icon = topic.icon;
							return row;
						}
					}
				}
				++row;
			}
			return -1;
		}
	}

	std::string DialogueServices::AddToRecord(RE::GFxMovieView& movie, RE::GFxValue& record, RE::Actor* speaker) {
		RE::GFxValue services;
		movie.CreateArray(&services);
		std::string summary;
		for (auto& service : Services()) {
			std::string icon;
			const int row = speaker ? FindRow(service, icon) : -1;
			bool available = row >= 0;
			if (!available && speaker && service.fallback && CanOpen(service) && InServiceFaction(speaker, service)) {
				//ShowBarterMenu has no stock to open without a vendor faction.
				available = service.id != "barter" || REBridge::VendorFaction(speaker) != nullptr;
			}

			RE::GFxValue entry;
			movie.CreateObject(&entry);
			entry.SetMember("id", RE::GFxValue(service.id.c_str()));
			entry.SetMember("row", RE::GFxValue(static_cast<double>(row)));
			entry.SetMember("available", RE::GFxValue(available));
			entry.SetMember("icon", RE::GFxValue(icon.c_str()));
			services.PushBack(entry);

			summary += std::format(" {}:row={},avail={}{}", service.id, row, available ? 1 : 0, icon.empty() ? "" : "," + icon);
		}
		record.SetMember("services", services);
		return summary;
	}

	//Reached when the table offers the service with no row (fallback=1),
	//or when the menu showed the button from an earlier list of this conversation and a picked topic
	//led to a list without the row. Both are gated by the speaker's factions, the topic's own condition.
	void DialogueServices::Open(const std::string& serviceID, RE::Actor* speaker) {
		ServiceDef* service = nullptr;
		for (auto& candidate : Services()) {
			//Without case: the id can arrive as "Recharge" when the BSFixedString pool already holds that spelling.
			if (Text::IEquals(candidate.id, serviceID)) {
				service = &candidate;
			}
		}
		if (!service) {
			Log::WARN("DialogueServices: Unknown service '{}'.", serviceID);
			return;
		}
		if (!speaker) {
			Log::WARN("DialogueServices: '{}' requested with no dialogue speaker.", serviceID);
			return;
		}
		if (!CanOpen(*service)) {
			Log::INFO("DialogueServices: '{}' refused. It has no built-in call and no open= in the table.", serviceID);
			return;
		}
		if (!InServiceFaction(speaker, *service)) {
			Log::INFO("DialogueServices: '{}' refused. {} is in none of its factions.", serviceID, speaker->GetName());
			return;
		}
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) {
			Log::WARN("DialogueServices: '{}' refused. No Papyrus VM.", serviceID);
			return;
		}
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		//open= naming the former SKYBDegradation.OpenVendorRepair / OpenVendorRecharge natives (the build's INI does;
		//the plugin no longer registers them, 2026-11-06) opens the same menu as the built-in call, so the built-in
		//call is used and Papyrus is not involved. And an open= whose script is not in the data is not dispatched:
		//calling a native with its script missing crashes the VM in the engine (2026-11-05: the build shipped the INI
		//naming SKYBDegradation without Scripts\SKYBDegradation.pex).
		bool builtIn = service->openClass.empty() || OwnNative(*service);
		if (!builtIn && !ScriptInstalled(service->openClass)) {
			if (!HasBuiltIn(*service)) {
				Log::WARN("DialogueServices: '{}' refused. open={}.{} but Scripts\\{}.pex is not installed.", serviceID,
					service->openClass, service->openFunction, service->openClass);
				return;
			}
			Log::WARN("DialogueServices: '{}': open={}.{} but Scripts\\{}.pex is not installed; using the built-in call.",
				serviceID, service->openClass, service->openFunction, service->openClass);
			builtIn = true;
		}
		if (!builtIn) {
			auto* args = RE::MakeFunctionArguments(static_cast<RE::Actor*>(speaker));
			const bool dispatched = vm->DispatchStaticCall(service->openClass, service->openFunction, args, callback);
			Log::INFO("DialogueServices: '{}': {}.{}({}) dispatched={}.", serviceID, service->openClass, service->openFunction,
				speaker->GetName(), dispatched);
		}
		else if (service->id == "barter") {
			if (!REBridge::VendorFaction(speaker)) {
				Log::INFO("DialogueServices: 'barter' refused. {} has no vendor faction.", speaker->GetName());
				return;
			}
			auto* policy = vm->GetObjectHandlePolicy();
			const auto handle = policy ? policy->GetHandleForObject(RE::Actor::FORMTYPE, speaker) : RE::VMHandle{};
			auto* args = RE::MakeFunctionArguments();
			const bool dispatched = vm->DispatchMethodCall(handle, "Actor", "ShowBarterMenu", args, callback);
			Log::INFO("DialogueServices: 'barter': Actor.ShowBarterMenu() on {} dispatched={}.", speaker->GetName(), dispatched);
		}
		else if (service->id == "training") {
			auto* args = RE::MakeFunctionArguments(static_cast<RE::Actor*>(speaker));
			const bool dispatched = vm->DispatchStaticCall("Game", "ShowTrainingMenu", args, callback);
			Log::INFO("DialogueServices: 'training': Game.ShowTrainingMenu({}) dispatched={}.", speaker->GetName(), dispatched);
		}
		else if (service->id == "repair" || service->id == "recharge") {
			//On the main thread, as the Papyrus natives do: this runs on the thread that delivered the menu's call.
			const bool recharge = service->id == "recharge";
			const auto handle = speaker->GetHandle();
			SKSE::GetTaskInterface()->AddTask([handle, recharge] {
				if (auto vendor = REBridge::Lookup(handle)) {
					if (recharge) {
						SKYBDeg::RepairMenu::OpenRecharge(vendor.get());
					}
					else {
						SKYBDeg::RepairMenu::OpenVendor(vendor.get());
					}
				}
			});
			Log::INFO("DialogueServices: '{}': built-in menu for {} queued.", serviceID, speaker->GetName());
		}
	}

}
