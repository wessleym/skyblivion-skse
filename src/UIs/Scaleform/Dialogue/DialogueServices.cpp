#include "DialogueServices.h"
#include "DialogueContract.h"

#include <cstdint>
#include <fstream>
#include <string_view>
#include <vector>

namespace Dialogue {

	namespace {
		struct ServiceTopic {
			std::string editorID;
			std::uint32_t formID = 0;
			std::string plugin;
			std::string icon;
			RE::TESTopic* form = nullptr;
			bool lookupAttempted = false;
		};

		struct ServiceFaction {
			std::string editorID;
			std::uint32_t formID = 0;
			std::string plugin;
			RE::TESFaction* form = nullptr;
			bool lookupAttempted = false;
		};

		struct ServiceDef {
			std::string id;
			std::vector<ServiceTopic> topics;
			std::vector<ServiceFaction> factions;
			bool fallback = false;
			std::string openClass;
			std::string openFunction;
		};

		std::string Trim(std::string_view text) {
			const auto begin = text.find_first_not_of(" \t\r\n");
			if (begin == std::string_view::npos) {
				return {};
			}
			const auto end = text.find_last_not_of(" \t\r\n");
			return std::string(text.substr(begin, end - begin + 1));
		}

		//Splits on a separator, trimming each piece and dropping empty ones.
		std::vector<std::string> SplitList(std::string_view text, char separator) {
			std::vector<std::string> pieces;
			while (true) {
				const auto position = text.find(separator);
				auto piece = Trim(text.substr(0, position));
				if (!piece.empty()) {
					pieces.push_back(std::move(piece));
				}
				if (position == std::string_view::npos) {
					return pieces;
				}
				text = text.substr(position + 1);
			}
		}

		//"210285:Skyblivion.esm" -> the plugin-relative form ID and the plugin.
		bool ParseFormKey(const std::string& key, std::uint32_t& formID, std::string& plugin) {
			const auto colon = key.find(':');
			if (colon == std::string::npos) {
				return false;
			}
			try {
				formID = static_cast<std::uint32_t>(std::stoul(key.substr(0, colon), nullptr, 16));
			}
			catch (...) {
				return false;
			}
			plugin = Trim(std::string_view(key).substr(colon + 1));
			return !plugin.empty();
		}

		//"EditorID|FormKey|Icon"; the FormKey and icon are optional.
		ServiceTopic MakeTopic(const std::string& spec) {
			ServiceTopic topic;
			const auto parts = SplitList(spec, '|');
			if (!parts.empty()) {
				topic.editorID = parts[0];
			}
			if (parts.size() > 1) {
				ParseFormKey(parts[1], topic.formID, topic.plugin);
			}
			if (parts.size() > 2) {
				topic.icon = parts[2];
			}
			return topic;
		}

		//"EditorID|FormKey"; the FormKey is optional.
		ServiceFaction MakeFaction(const std::string& spec) {
			ServiceFaction faction;
			const auto parts = SplitList(spec, '|');
			if (!parts.empty()) {
				faction.editorID = parts[0];
			}
			if (parts.size() > 1) {
				ParseFormKey(parts[1], faction.formID, faction.plugin);
			}
			return faction;
		}

		std::vector<ServiceDef> BuiltInServices() {
			std::vector<ServiceDef> services;

			ServiceDef barter;
			barter.id = "barter";
			for (const char* spec : {
					 "SKYBDialGenericForSaleTopic|210285:Skyblivion.esm",
					 "SKYBWERoadDealer01SaleTopic|0DED9A:Skyblivion.esm",
				 }) {
				barter.topics.push_back(MakeTopic(spec));
			}
			for (const char* spec : {
					 "SKYBJobMerchantFaction|25637B:Skyblivion.esm",
					 "SKYBWEServicesMerchantFaction|28F8E8:Skyblivion.esm",
				 }) {
				barter.factions.push_back(MakeFaction(spec));
			}
			services.push_back(std::move(barter));

			ServiceDef training;
			training.id = "training";
			for (const char* spec : {
					 "TES4TrainingAlchemyTopic|210C4F:Skyblivion.esm|Alchemy",
					 "TES4TrainingAlterationTopic|210C10:Skyblivion.esm|Alteration",
					 "TES4TrainingArcheryTopic|210C99:Skyblivion.esm|Archery",
					 "TES4TrainingBlockTopic|210CB2:Skyblivion.esm|Block",
					 "TES4TrainingConjurationTopic|210C5B:Skyblivion.esm|Conjuration",
					 "TES4TrainingDestructionTopic|210C43:Skyblivion.esm|Destruction",
					 "TES4TrainingEnchantingTopic|210CCB:Skyblivion.esm|Enchanting",
					 "TES4TrainingHeavyArmorTopic|210C74:Skyblivion.esm|HeavyArmor",
					 "TES4TrainingIllusionTopic|210CBE:Skyblivion.esm|Illusion",
					 "TES4TrainingLightArmorTopic|210C13:Skyblivion.esm|LightArmor",
					 "TES4TrainingLockpickingTopic|210CD9:Skyblivion.esm|Lockpicking",
					 "TES4TrainingOneHandedTopic|210C81:Skyblivion.esm|OneHanded",
					 "TES4TrainingPickpocketTopic|0F968B:Skyblivion.esm|Pickpocket",
					 "TES4TrainingRestorationTopic|210C8D:Skyblivion.esm|Restoration",
					 "TES4TrainingSmithingTopic|210CA5:Skyblivion.esm|Smithing",
					 "TES4TrainingSneakTopic|210C2A:Skyblivion.esm|Sneak",
					 "SKYBDialGenericTrainSpeechTopic|210C37:Skyblivion.esm|Speech",
					 "TES4TrainingTwoHandedTopic|210C67:Skyblivion.esm|TwoHanded",
				 }) {
				training.topics.push_back(MakeTopic(spec));
			}
			for (const char* spec : {
					 "SKYBTrainerFactionAlchemy|009667:Skyblivion.esm",
					 "SKYBTrainerFactionAlteration|009668:Skyblivion.esm",
					 "SKYBTrainerFactionMarksman|00967A:Skyblivion.esm",
					 "SKYBTrainerFactionBlock|009671:Skyblivion.esm",
					 "SKYBTrainerFactionConjuration|009672:Skyblivion.esm",
					 "SKYBTrainerFactionDestruction|009673:Skyblivion.esm",
					 "SKYBTrainerFactionEnchanting|009674:Skyblivion.esm",
					 "SKYBTrainerFactionHeavyArmor|009676:Skyblivion.esm",
					 "SKYBTrainerFactionIllusion|009677:Skyblivion.esm",
					 "SKYBTrainerFactionLightArmor|009678:Skyblivion.esm",
					 "SKYBTrainerFactionLockpicking|009679:Skyblivion.esm",
					 "SKYBTrainerFactionOneHanded|00967B:Skyblivion.esm",
					 "SKYBTrainerFactionPickpocket|00967C:Skyblivion.esm",
					 "SKYBTrainerFactionRestoration|0560C0:Skyblivion.esm",
					 "SKYBTrainerFactionSmithing|0564D1:Skyblivion.esm",
					 "SKYBFactionTrainerSneak|0564F4:Skyblivion.esm",
					 "SKYBTrainerFactionSpeechcraft|0564F5:Skyblivion.esm",
					 "SKYBTrainerFactionTwoHanded|13C216:Skyblivion.esm",
				 }) {
				training.factions.push_back(MakeFaction(spec));
			}
			services.push_back(std::move(training));

			//Reserved: the build has the factions but no menu, topic or launcher yet.
			//They appear once the INI gives them an open= call and fallback=1.
			ServiceDef repair;
			repair.id = "repair";
			repair.factions.push_back(MakeFaction("SKYBRepairFaction|0D8209:Skyblivion.esm"));
			repair.factions.push_back(MakeFaction("SKYBRepairmanFaction|295CE7:Skyblivion.esm"));
			services.push_back(std::move(repair));

			ServiceDef recharge;
			recharge.id = "recharge";
			recharge.factions.push_back(MakeFaction("SKYBRechargeFaction|0D820A:Skyblivion.esm"));
			services.push_back(std::move(recharge));

			return services;
		}

		void ApplyServicesIni(std::vector<ServiceDef>& services) {
			std::ifstream in(DialogueContract::ServicesIniFile);
			if (!in) {
				return;
			}
			Log::INFO("DialogueServices: Reading {}.", DialogueContract::ServicesIniFile);
			ServiceDef* current = nullptr;
			std::string line;
			int lineNumber = 0;
			while (std::getline(in, line)) {
				++lineNumber;
				const auto text = Trim(line);
				if (text.empty() || text[0] == ';' || text[0] == '#') {
					continue;
				}
				if (text.front() == '[' && text.back() == ']') {
					const auto id = Trim(std::string_view(text).substr(1, text.size() - 2));
					current = nullptr;
					for (auto& service : services) {
						if (service.id == id) {
							current = &service;
						}
					}
					if (!current) {
						services.push_back(ServiceDef{});
						current = &services.back();
						current->id = id;
						Log::INFO("DialogueServices: New service '{}' from the INI (the menu must know it too).", id);
					}
					continue;
				}
				const auto equals = text.find('=');
				if (equals == std::string::npos || !current) {
					Log::WARN("DialogueServices: {}({}) ignored ('{}').", DialogueContract::ServicesIniFile, lineNumber, text);
					continue;
				}
				const auto key = Trim(std::string_view(text).substr(0, equals));
				const auto value = Trim(std::string_view(text).substr(equals + 1));
				if (key == "clear") {
					if (value == "1") {
						current->topics.clear();
						current->factions.clear();
					}
				}
				else if (key == "topics") {
					for (const auto& spec : SplitList(value, ',')) {
						current->topics.push_back(MakeTopic(spec));
					}
				}
				else if (key == "factions") {
					for (const auto& spec : SplitList(value, ',')) {
						current->factions.push_back(MakeFaction(spec));
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

		//Editor ID first (po3 Tweaks caches them), then the FormKey. Each entry resolves once.
		RE::TESTopic* Resolve(ServiceTopic& topic) {
			if (topic.form || topic.lookupAttempted) {
				return topic.form;
			}
			topic.lookupAttempted = true;
			if (!topic.editorID.empty()) {
				topic.form = RE::TESForm::LookupByEditorID<RE::TESTopic>(topic.editorID);
			}
			if (!topic.form && topic.formID) {
				if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
					topic.form = dataHandler->LookupForm<RE::TESTopic>(topic.formID, topic.plugin);
				}
			}
			if (!topic.form) {
				Log::WARN("DialogueServices: Topic '{}' not resolved.", topic.editorID);
			}
			return topic.form;
		}

		RE::TESFaction* Resolve(ServiceFaction& faction) {
			if (faction.form || faction.lookupAttempted) {
				return faction.form;
			}
			faction.lookupAttempted = true;
			if (!faction.editorID.empty()) {
				faction.form = RE::TESForm::LookupByEditorID<RE::TESFaction>(faction.editorID);
			}
			if (!faction.form && faction.formID) {
				if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
					faction.form = dataHandler->LookupForm<RE::TESFaction>(faction.formID, faction.plugin);
				}
			}
			if (!faction.form) {
				Log::WARN("DialogueServices: Faction '{}' not resolved.", faction.editorID);
			}
			return faction.form;
		}

		bool InServiceFaction(RE::Actor* speaker, ServiceDef& service) {
			if (!speaker) {
				return false;
			}
			for (auto& faction : service.factions) {
				if (auto* form = Resolve(faction); form && speaker->IsInFaction(form)) {
					return true;
				}
			}
			return false;
		}

		//Whether the fallback path has anything to call: an open= from the table, or a built-in call.
		bool CanOpen(const ServiceDef& service) {
			return !service.openClass.empty() || service.id == "barter" || service.id == "training";
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
					for (auto& topic : service.topics) {
						if (Resolve(topic) == entry->parentTopic) {
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
			if (candidate.id == serviceID) {
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
		if (!service->openClass.empty()) {
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
	}

}
