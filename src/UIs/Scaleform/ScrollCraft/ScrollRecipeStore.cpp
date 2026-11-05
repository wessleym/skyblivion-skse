#include "ScrollRecipeStore.h"

#include "Papyrus/SKSEScriptRegistrar.h"
#include "Platform/FormCache.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

#include <nlohmann/json.hpp>

// <Windows.h>, which this plugin's CommonLib build includes, maps PlaySound to PlaySoundA.
#ifdef PlaySound
#	undef PlaySound
#endif

namespace fs = std::filesystem;
using nlohmann::json;

namespace ScrollCraft {

	namespace {
		constexpr const char* kSeatNames[ScrollRecipeStore::SeatCount] = { "top", "bottom", "left", "right", "centre" };
		constexpr const char* kDeliveries = "ATRSG";
		//the delivery an aspect essence asks for: Aether touch, Daedric ground, Ember aimed, Gloam self, Vital area
		constexpr const char* kAspectDelivery = "TGASR";

		std::string Lower(std::string a_text)
		{
			std::transform(a_text.begin(), a_text.end(), a_text.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_text;
		}

		//"HEX|Plugin" -> the scroll, or null (logged by the caller)
		RE::TESForm* ResolveFormRef(const std::string& a_ref)
		{
			const auto bar = a_ref.find('|');
			if (bar == std::string::npos || bar == 0 || bar + 1 >= a_ref.size()) {
				return nullptr;
			}
			std::uint32_t localID = 0;
			try {
				localID = static_cast<std::uint32_t>(std::stoul(a_ref.substr(0, bar), nullptr, 16));
			} catch (...) {
				return nullptr;
			}
			return FormCache::ResolveLocal(localID & 0x00FFFFFF, a_ref.substr(bar + 1), RE::FormType::Scroll, "ScrollRecipeStore");
		}

		//One recipe object -> Recipe; the reason when it is refused. a_disabled: the recipe asks to be removed
		//(only scroll and form are needed then).
		bool Parse(const json& a_json, Recipe& a_out, bool& a_disabled, std::string& a_why)
		{
			if (!a_json.is_object()) {
				a_why = "not an object";
				return false;
			}
			auto str = [&](const char* key, std::string& out) {
				auto it = a_json.find(key);
				if (it == a_json.end() || !it->is_string()) {
					return false;
				}
				out = it->get<std::string>();
				return true;
			};
			auto num = [&](const char* key, int& out) {
				auto it = a_json.find(key);
				if (it == a_json.end() || !it->is_number_integer()) {
					return false;
				}
				out = it->get<int>();
				return true;
			};
			if (!str("scroll", a_out.scroll) || a_out.scroll.empty()) {
				a_why = "'scroll' (the editor ID) missing";
				return false;
			}
			if (!str("form", a_out.formRef)) {
				a_why = "'form' (\"HEX|Plugin\") missing";
				return false;
			}
			a_out.form = ResolveFormRef(a_out.formRef);
			if (!a_out.form) {
				a_why = "'form' " + a_out.formRef + " does not resolve to a scroll (is the plugin loaded?)";
				return false;
			}
			auto off = a_json.find("disabled");
			a_disabled = off != a_json.end() && off->is_boolean() && off->get<bool>();
			if (a_disabled) {
				return true;
			}
			if (!num("school", a_out.school) || a_out.school < 0 || a_out.school > 4) {
				a_why = "'school' must be 0 to 4";
				return false;
			}
			if (!num("tier", a_out.tier) || a_out.tier < 1 || a_out.tier > ScrollRecipeStore::SeatCount) {
				a_why = "'tier' must be 1 to 5";
				return false;
			}
			if (!str("delivery", a_out.delivery) || a_out.delivery.empty()) {
				a_why = "'delivery' missing";
				return false;
			}
			for (const char c : a_out.delivery) {
				if (!std::strchr(kDeliveries, c)) {
					a_why = "'delivery' has a letter other than A T R S G";
					return false;
				}
			}
			auto seats = a_json.find("seats");
			if (seats == a_json.end() || !seats->is_object()) {
				a_why = "'seats' (an object seat -> character) missing";
				return false;
			}
			std::string wheel(ScrollRecipeStore::SeatCount, '.');
			int filled = 0;
			for (auto it = seats->begin(); it != seats->end(); ++it) {
				int at = -1;
				for (int i = 0; i < ScrollRecipeStore::SeatCount; ++i) {
					if (it.key() == kSeatNames[i]) {
						at = i;
					}
				}
				if (at < 0) {
					a_why = "unknown seat '" + it.key() + "' (top, bottom, left, right, centre)";
					return false;
				}
				if (!it->is_string() || it->get<std::string>().size() != 1 ||
					!std::isupper(static_cast<unsigned char>(it->get<std::string>()[0]))) {
					a_why = "seat '" + it.key() + "' must hold one character A to Z";
					return false;
				}
				wheel[at] = it->get<std::string>()[0];
				++filled;
			}
			if (filled != ScrollRecipeStore::SeatsFor(a_out.tier)) {
				a_why = std::format("tier {} fills {} seat(s) but {} are filled", a_out.tier, ScrollRecipeStore::SeatsFor(a_out.tier), filled);
				return false;
			}
			//the essences: the recipe's own, else its tier's
			a_out.essCommon = ScrollRecipeStore::s_cost[a_out.tier - 1][0];
			a_out.essSchool = ScrollRecipeStore::s_cost[a_out.tier - 1][1];
			a_out.essRare = ScrollRecipeStore::s_cost[a_out.tier - 1][2];
			auto ess = a_json.find("essences");
			if (ess != a_json.end() && ess->is_object()) {
				auto get = [&](const char* key, int& out) {
					auto it = ess->find(key);
					if (it != ess->end() && it->is_number_integer() && it->get<int>() >= 0 && it->get<int>() <= 5) {
						out = it->get<int>();
					}
				};
				get("common", a_out.essCommon);
				get("school", a_out.essSchool);
				get("rare", a_out.essRare);
			}
			if (a_out.essSchool < 1 || a_out.essCommon + a_out.essSchool + a_out.essRare > 5) {
				a_why = "'essences' must take at least 1 of the school and at most 5 in all";
				return false;
			}
			a_out.wheel = wheel;
			return true;
		}

		//The base file's "seatsPerTier" (five numbers, 1..5, rising) and "essenceCost" ({"1": {"common", "school", "rare"}, ...});
		//a missing or bad table keeps the defaults and is logged.
		void ReadTables(const json& a_doc, const std::string& a_label)
		{
			auto seats = a_doc.find("seatsPerTier");
			if (seats != a_doc.end()) {
				bool ok = seats->is_array() && seats->size() == ScrollRecipeStore::SeatCount;
				int v[ScrollRecipeStore::SeatCount] = {};
				for (int i = 0; ok && i < ScrollRecipeStore::SeatCount; ++i) {
					const auto& e = (*seats)[i];
					ok = e.is_number_integer() && e.get<int>() >= 1 && e.get<int>() <= ScrollRecipeStore::SeatCount &&
						 (i == 0 || e.get<int>() >= v[i - 1]);
					if (ok) {
						v[i] = e.get<int>();
					}
				}
				if (ok) {
					std::copy(std::begin(v), std::end(v), std::begin(ScrollRecipeStore::s_seats));
				} else {
					Log::WARN("ScrollRecipeStore: {}: 'seatsPerTier' must be five rising numbers 1..5; kept 2 3 4 5 5", a_label);
				}
			}
			auto cost = a_doc.find("essenceCost");
			if (cost != a_doc.end() && cost->is_object()) {
				for (int t = 1; t <= ScrollRecipeStore::SeatCount; ++t) {
					auto row = cost->find(std::to_string(t));
					if (row == cost->end() || !row->is_object()) {
						continue;
					}
					const char* keys[3] = { "common", "school", "rare" };
					for (int k = 0; k < 3; ++k) {
						auto it = row->find(keys[k]);
						if (it != row->end() && it->is_number_integer() && it->get<int>() >= 0 && it->get<int>() <= 5) {
							ScrollRecipeStore::s_cost[t - 1][k] = it->get<int>();
						}
					}
				}
			}
		}

		//The base file first, then the others in name order.
		std::vector<fs::path> Files(std::string& a_missing)
		{
			std::vector<fs::path> out;
			std::error_code ec;
			const fs::path dir(ScrollRecipeStore::DataFolder);
			if (!fs::is_directory(dir, ec)) {
				a_missing = std::format("the folder {} is missing", ScrollRecipeStore::DataFolder);
				return out;
			}
			std::vector<fs::path> others;
			bool base = false;
			for (const auto& entry : fs::directory_iterator(dir, ec)) {
				if (!entry.is_regular_file() || Lower(entry.path().extension().string()) != ".json") {
					continue;
				}
				if (Lower(entry.path().filename().string()) == ScrollRecipeStore::BaseFile) {
					out.insert(out.begin(), entry.path());
					base = true;
				} else {
					others.push_back(entry.path());
				}
			}
			std::sort(others.begin(), others.end(), [](const fs::path& a, const fs::path& b) {
				return Lower(a.filename().string()) < Lower(b.filename().string());
			});
			out.insert(out.end(), others.begin(), others.end());
			if (!base) {
				a_missing = std::format("{} missing from {}", ScrollRecipeStore::BaseFile, ScrollRecipeStore::DataFolder);
			}
			return out;
		}

		//---------------------------------------------------------------- Papyrus
		RE::BSFixedString PapyrusStatus(RE::StaticFunctionTag*)
		{
			return RE::BSFixedString(ScrollRecipeStore::Status());
		}
		RE::TESForm* PapyrusScroll(RE::StaticFunctionTag*, std::int32_t a_index)
		{
			const auto& all = ScrollRecipeStore::Recipes();
			if (a_index < 0 || static_cast<std::size_t>(a_index) >= all.size()) {
				return nullptr;
			}
			return all[static_cast<std::size_t>(a_index)].form;
		}
		std::int32_t PapyrusFind(RE::StaticFunctionTag*, RE::TESForm* a_form)
		{
			return ScrollRecipeStore::Find(a_form);
		}
		std::int32_t PapyrusResolve(RE::StaticFunctionTag*, std::int32_t a_school, RE::BSFixedString a_wheel, std::int32_t a_aspect)
		{
			return ScrollRecipeStore::Resolve(a_school, a_wheel.c_str(), a_aspect);
		}
		//"index:school:tier:delivery:wheel:name" for every recipe whose scroll is in the known list, joined by "|"
		RE::BSFixedString PapyrusKnownTable(RE::StaticFunctionTag*, RE::BGSListForm* a_known)
		{
			std::string out;
			if (!a_known) {
				return RE::BSFixedString(out);
			}
			const auto& all = ScrollRecipeStore::Recipes();
			for (std::size_t i = 0; i < all.size(); ++i) {
				const auto& r = all[i];
				if (!a_known->HasForm(r.form)) {
					continue;
				}
				if (!out.empty()) {
					out += '|';
				}
				const char* name = r.form->GetName();
				out += std::format("{}:{}:{}:{}:{}:{}-{}-{}:{}", i, r.school, r.tier, r.delivery, r.wheel, r.essCommon, r.essSchool,
								   r.essRare, name ? name : r.scroll.c_str());
			}
			return RE::BSFixedString(out);
		}
		//"2,3,4,5,5": the seats each tier fills
		RE::BSFixedString PapyrusGetSeats(RE::StaticFunctionTag*)
		{
			std::string out;
			for (int t = 1; t <= ScrollRecipeStore::SeatCount; ++t) {
				out += (t > 1 ? "," : "") + std::to_string(ScrollRecipeStore::SeatsFor(t));
			}
			return RE::BSFixedString(out);
		}
		//"2-1-0,1-2-0,...": each tier's essence cost, Verdant-school-Radiant
		RE::BSFixedString PapyrusGetCosts(RE::StaticFunctionTag*)
		{
			std::string out;
			for (int t = 0; t < ScrollRecipeStore::SeatCount; ++t) {
				out += std::format("{}{}-{}-{}", t > 0 ? "," : "", ScrollRecipeStore::s_cost[t][0], ScrollRecipeStore::s_cost[t][1],
								   ScrollRecipeStore::s_cost[t][2]);
			}
			return RE::BSFixedString(out);
		}
		//"tier,common,school,rare" of one recipe; "" for a bad index
		RE::BSFixedString PapyrusGetCost(RE::StaticFunctionTag*, std::int32_t a_index)
		{
			const auto& all = ScrollRecipeStore::Recipes();
			if (a_index < 0 || static_cast<std::size_t>(a_index) >= all.size()) {
				return RE::BSFixedString("");
			}
			const auto& r = all[static_cast<std::size_t>(a_index)];
			return RE::BSFixedString(std::format("{},{},{},{}", r.tier, r.essCommon, r.essSchool, r.essRare));
		}
		//a UI sound by its descriptor's editor ID (UIEnchantingItemCreate, UIAlchemyFail, ...)
		void PapyrusPlayUISound(RE::StaticFunctionTag*, RE::BSFixedString a_editorID)
		{
			if (!a_editorID.empty()) {
				RE::PlaySound(a_editorID.c_str());
			}
		}
		//The satchel is used from the inventory: the inventory closes while the scribing menu is up, as the repair
		//menu's does (RepairMenu::Open). True when it was open (the quest reopens it on close).
		bool PapyrusHideInventory(RE::StaticFunctionTag*)
		{
			auto* ui = RE::UI::GetSingleton();
			auto* queue = RE::UIMessageQueue::GetSingleton();
			if (!ui || !queue || !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
				return false;
			}
			queue->AddMessage(RE::InventoryMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
			return true;
		}
		void PapyrusShowInventory(RE::StaticFunctionTag*)
		{
			SKSE::GetTaskInterface()->AddTask([] {
				auto* ui = RE::UI::GetSingleton();
				auto* queue = RE::UIMessageQueue::GetSingleton();
				if (ui && queue && !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
					queue->AddMessage(RE::InventoryMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
				}
			});
		}
	}

	void ScrollRecipeStore::LoadOnce()
	{
		if (s_loaded) {
			return;
		}
		s_loaded = true;
		s_recipes.clear();

		std::string baseMissing;
		const auto files = Files(baseMissing);
		if (!baseMissing.empty()) {
			Log::WARN("ScrollRecipeStore: {}", baseMissing);
		}

		//by scroll form: a later file replaces (or, disabled, removes) an earlier recipe; the slot keeps its place
		std::vector<Recipe> merged;
		std::vector<std::pair<std::size_t, std::size_t>> rank;  //(file index, position in file) per slot
		std::map<const RE::TESForm*, std::size_t> slotOf;
		std::vector<bool> removed;
		int skipped = 0;
		int filesRead = 0;
		for (std::size_t f = 0; f < files.size(); ++f) {
			const auto label = files[f].filename().string();
			std::ifstream file(files[f], std::ios::binary);
			if (!file) {
				Log::WARN("ScrollRecipeStore: {}: cannot be opened; skipped", label);
				continue;
			}
			std::stringstream text;
			text << file.rdbuf();
			const auto doc = json::parse(text.str(), nullptr, false);
			const auto recipes = doc.is_object() ? doc.find("recipes") : doc.end();
			if (doc.is_discarded() || !doc.is_object() || recipes == doc.end() || !recipes->is_array()) {
				Log::WARN("ScrollRecipeStore: {}: not valid JSON with a \"recipes\" array; skipped", label);
				if (f == 0 && baseMissing.empty()) {
					baseMissing = std::format("{} is not valid JSON", BaseFile);
				}
				continue;
			}
			++filesRead;
			if (f == 0) {
				ReadTables(doc, label);
			}
			std::size_t at = 0;
			for (const auto& item : *recipes) {
				Recipe r;
				bool disabled = false;
				std::string why;
				const std::string name = item.is_object() && item.contains("scroll") && item["scroll"].is_string()
											 ? item["scroll"].get<std::string>()
											 : std::format("#{}", at);
				const std::size_t pos = at++;
				if (!Parse(item, r, disabled, why)) {
					Log::WARN("ScrollRecipeStore: {}: {}: skipped: {}", label, name, why);
					++skipped;
					continue;
				}
				r.source = label;
				auto existing = slotOf.find(r.form);
				if (existing != slotOf.end()) {
					const auto slot = existing->second;
					Log::INFO("ScrollRecipeStore: {}: {}: {} the recipe from {}", label, name,
							  disabled ? "removes" : "replaces", merged[slot].source);
					if (disabled) {
						removed[slot] = true;
					} else {
						merged[slot] = std::move(r);
						rank[slot] = { f, pos };
						removed[slot] = false;
					}
					continue;
				}
				if (disabled) {
					Log::INFO("ScrollRecipeStore: {}: {}: disabled, and no earlier file has it", label, name);
					continue;
				}
				slotOf.emplace(r.form, merged.size());
				merged.push_back(std::move(r));
				rank.push_back({ f, pos });
				removed.push_back(false);
			}
		}

		//one school + wheel + delivery: the earliest file (then the earliest in it) keeps it
		std::vector<std::size_t> order;
		for (std::size_t i = 0; i < merged.size(); ++i) {
			if (!removed[i]) {
				order.push_back(i);
			}
		}
		std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return rank[a] < rank[b]; });
		std::set<std::tuple<int, std::string, std::string>> claimed;
		std::vector<bool> lost(merged.size(), false);
		for (const auto i : order) {
			const auto& r = merged[i];
			if (!claimed.insert({ r.school, r.wheel, r.delivery }).second) {
				Log::WARN("ScrollRecipeStore: {}: {}: skipped: another recipe of school {} already has the seats {} with delivery {}",
						  r.source, r.scroll, r.school, r.wheel, r.delivery);
				lost[i] = true;
				++skipped;
			}
		}
		for (std::size_t i = 0; i < merged.size(); ++i) {
			if (!removed[i] && !lost[i]) {
				s_recipes.push_back(std::move(merged[i]));
			}
		}

		if (!baseMissing.empty()) {
			s_status = baseMissing;
		} else if (skipped > 0) {
			s_status = std::format("{} recipe(s) skipped (see Skyblivion.log)", skipped);
		} else {
			s_status = std::format("ok: {} recipe(s) from {} file(s)", s_recipes.size(), filesRead);
		}
		Log::INFO("ScrollRecipeStore: {} recipe(s) from {} file(s) in {}; {} skipped", s_recipes.size(), filesRead,
				  DataFolder, skipped);
	}

	int ScrollRecipeStore::Find(const RE::TESForm* a_form)
	{
		if (!a_form) {
			return -1;
		}
		for (std::size_t i = 0; i < s_recipes.size(); ++i) {
			if (s_recipes[i].form == a_form) {
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	int ScrollRecipeStore::SeatsFor(int a_tier)
	{
		return a_tier >= 1 && a_tier <= SeatCount ? s_seats[a_tier - 1] : a_tier;
	}

	int ScrollRecipeStore::Resolve(int a_school, std::string_view a_wheel, int a_aspect)
	{
		const char want = a_aspect >= 0 && a_aspect < 5 ? kAspectDelivery[a_aspect] : '\0';
		int plain = -1;
		int first = -1;
		for (std::size_t i = 0; i < s_recipes.size(); ++i) {
			const auto& r = s_recipes[i];
			if (r.school != a_school || r.wheel != a_wheel) {
				continue;
			}
			const int index = static_cast<int>(i);
			if (want && r.delivery.find(want) != std::string::npos) {
				return index;
			}
			if (plain < 0 && r.delivery.find('T') == std::string::npos && r.delivery.find('R') == std::string::npos) {
				plain = index;
			}
			if (first < 0) {
				first = index;
			}
		}
		return plain >= 0 ? plain : first;
	}

	void ScrollRecipeStore::RegisterPapyrus(RE::BSScript::Internal::VirtualMachine* vm)
	{
		SKSEScriptRegistrar::Register(vm, ScriptName, "GetStatus", PapyrusStatus);
		SKSEScriptRegistrar::Register(vm, ScriptName, "GetScroll", PapyrusScroll);
		SKSEScriptRegistrar::Register(vm, ScriptName, "Find", PapyrusFind);
		SKSEScriptRegistrar::Register(vm, ScriptName, "Resolve", PapyrusResolve);
		SKSEScriptRegistrar::Register(vm, ScriptName, "KnownTable", PapyrusKnownTable);
		SKSEScriptRegistrar::Register(vm, ScriptName, "GetSeats", PapyrusGetSeats);
		SKSEScriptRegistrar::Register(vm, ScriptName, "GetCosts", PapyrusGetCosts);
		SKSEScriptRegistrar::Register(vm, ScriptName, "GetCost", PapyrusGetCost);
		SKSEScriptRegistrar::Register(vm, ScriptName, "PlayUISound", PapyrusPlayUISound);
		SKSEScriptRegistrar::Register(vm, ScriptName, "HideInventory", PapyrusHideInventory);
		SKSEScriptRegistrar::Register(vm, ScriptName, "ShowInventory", PapyrusShowInventory);
	}
}
