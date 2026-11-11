#include "AlchemyMenuFeed.h"
#include "AlchemyMenuContract.h"
#include "Platform/FormCache.h"
#include "Platform/Text.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <string_view>

namespace Alchemy {

	namespace {
		namespace C = AlchemyMenuContract;
		using json = nlohmann::json;

		//"03EB15:Skyrim.esm": the local FormID in the defining file, as the baseline table writes it.
		std::string RecordKey(const RE::TESForm* form) {
			auto* file = form ? form->GetFile(0) : nullptr;
			if (!file) {
				return {};
			}
			return std::format("{:06X}:{}", form->GetLocalFormID(), file->GetFilename());
		}

		//The plugin's perk-entry data as the ESM writes it: one Mod Alchemy Effectiveness entry per tier.
		const RE::BGSEntryPointPerkEntry* AsEntryPoint(const RE::BGSPerkEntry* entry) {
			return entry && entry->GetType() == RE::PERK_ENTRY_TYPE::kEntryPoint ?
			           static_cast<const RE::BGSEntryPointPerkEntry*>(entry) : nullptr;
		}
	}

	//-------------------------------------------------------------------------------------------------- handlers

	class FetchHandler : public RE::GFxFunctionHandler {
	public:
		void Call(Params& a_params) override {
			if (!a_params.retVal || !a_params.movie) {
				return;
			}
			auto* view = static_cast<RE::GFxMovieView*>(a_params.movie);
			AlchemyMenuFeed::BuildData(*view, *a_params.retVal);
		}
	};

	class RememberHandler : public RE::GFxFunctionHandler {
	public:
		void Call(Params& a_params) override {
			if (a_params.argCount > 0) {
				AlchemyMenuFeed::s_preview = a_params.args[0].IsBool() ? a_params.args[0].GetBool() : a_params.args[0].GetNumber() != 0.0;
				Log::INFO("[AlchemyMenu] Bonus preview {} for the session.", AlchemyMenuFeed::s_preview ? "on" : "off");
			}
		}
	};

	//-------------------------------------------------------------------------------------------------- install

	void AlchemyMenuFeed::InstallScaleform() {
		auto* scaleform = SKSE::GetScaleformInterface();
		if (!scaleform) {
			Log::WARN("[AlchemyMenu] No Scaleform interface: the crafting menu gets no apparatus data.");
			return;
		}
		if (!scaleform->Register(OnMovieLoad, C::RegistrationName)) {
			Log::WARN("[AlchemyMenu] Scaleform registration refused: the crafting menu gets no apparatus data.");
			return;
		}
		Log::INFO("[AlchemyMenu] Scaleform callback registered for {}.", C::MovieFile);
	}

	bool AlchemyMenuFeed::OnMovieLoad(RE::GFxMovieView* a_view, RE::GFxValue* a_root) {
		if (!a_view || !a_root) {
			return true;
		}
		auto* def = a_view->GetMovieDef();
		const std::string_view url = def && def->GetFileURL() ? def->GetFileURL() : "";
		if (!Text::IEndsWith(url, C::MovieFile)) {
			return true;
		}
		static RE::GPtr<FetchHandler> fetch = RE::make_gptr<FetchHandler>();
		static RE::GPtr<RememberHandler> remember = RE::make_gptr<RememberHandler>();
		RE::GFxValue fetchFn;
		RE::GFxValue rememberFn;
		a_view->CreateFunction(&fetchFn, fetch.get());
		a_view->CreateFunction(&rememberFn, remember.get());
		//The object SKSE hands over is the plugin's own slot (_global.skse.plugins.SKYBAlchemy), not the movie's
		//_root. The two functions go on the slot, on _root and on _global; the movie looks in all three.
		const bool onSlot = a_root->SetMember(C::Function::Fetch, fetchFn) && a_root->SetMember(C::Function::Remember, rememberFn);
		RE::GFxValue root;
		const bool onRoot = a_view->GetVariable(&root, "_root") && root.IsDisplayObject() &&
		                    root.SetMember(C::Function::Fetch, fetchFn) && root.SetMember(C::Function::Remember, rememberFn);
		RE::GFxValue globals;
		const bool onGlobal = a_view->GetVariable(&globals, "_global") && globals.IsObject() &&
		                      globals.SetMember(C::Function::Fetch, fetchFn) && globals.SetMember(C::Function::Remember, rememberFn);
		Log::INFO("[AlchemyMenu] {} loaded: {} and {} set on SKSE's slot={}, _root={}, _global={}.",
			url, C::Function::Fetch, C::Function::Remember, onSlot, onRoot, onGlobal);
		return true;
	}

	void AlchemyMenuFeed::OnDataLoaded() {
		ParsePerks();
		ReadBaselines();
	}

	//-------------------------------------------------------------------------------------------------- perks

	//The four slots. The list is Master first (the quest's checkTool returns 5 for index 0).
	void AlchemyMenuFeed::ParsePerks() {
		s_tools = {
			{ "mortar", "SKYBAlchemyMortarPestleGlob", "SKYBAlchemyPerkMortarPestle", "SKYBAlchemyMortarPestleList" },
			{ "retort", "SKYBAlchemyRetortGlob", "SKYBAlchemyPerkRetort", "SKYBAlchemyRetortList" },
			{ "alembic", "SKYBAlchemyAlembicGlob", "SKYBAlchemyPerkAlembic", "SKYBAlchemyAlembicList" },
			{ "calcinator", "SKYBAlchemyCalcinatorGlob", "SKYBAlchemyPerkCalcinator", "SKYBAlchemyCalcinatorList" },
		};
		s_parsed = true;
		for (auto& tool : s_tools) {
			auto* perk = FormCache::Get<RE::BGSPerk>(tool.perkEditorID, "AlchemyMenu");
			auto* global = FormCache::Get<RE::TESGlobal>(tool.globalEditorID, "AlchemyMenu");
			if (!perk || !global) {
				Log::WARN("[AlchemyMenu] {}: perk or global missing; no bonus can be shown.", tool.id);
				continue;
			}
			int read = 0;
			for (auto* raw : perk->perkEntries) {
				auto* entry = AsEntryPoint(raw);
				if (!entry || entry->entryData.entryPoint.get() != RE::BGSEntryPoint::ENTRY_POINTS::kModAlchemyEffectiveness) {
					continue;
				}
				auto* fn = entry->functionData;
				if (!fn || fn->GetType() != RE::BGSEntryPointFunctionData::ENTRY_POINT_FUNCTION_DATA::kOneValue) {
					continue;
				}
				const float mult = static_cast<const RE::BGSEntryPointFunctionDataOneValue*>(fn)->data;
				//Walk the entry's conditions: the tier is the comparison value of GetGlobalValue on our
				//global; a keyword condition names the scope; GetMakingPoison == 0 means potions only.
				int tier = 0;
				std::string scope = "all";
				bool potionOnly = false;
				for (std::uint32_t c = 0; c < entry->conditions.size(); ++c) {
					for (auto* item = entry->conditions[c].head; item; item = item->next) {
						const auto& d = item->data;
						const auto function = d.functionData.function.get();
						if (function == RE::FUNCTION_DATA::FunctionID::kGetGlobalValue) {
							if (d.functionData.params[0] == global && !d.flags.global) {
								tier = static_cast<int>(std::lround(d.comparisonValue.f));
							}
						} else if (function == RE::FUNCTION_DATA::FunctionID::kEPAlchemyEffectHasKeyword) {
							auto* keyword = static_cast<RE::BGSKeyword*>(d.functionData.params[0]);
							const char* edid = keyword ? keyword->GetFormEditorID() : nullptr;
							const std::string_view name = edid ? edid : "";
							if (name == "MagicAlchBeneficial") {
								scope = "beneficial";
							} else if (name == "MagicAlchHarmful") {
								scope = "harmful";
							} else {
								scope = std::string(name);
							}
						} else if (function == RE::FUNCTION_DATA::FunctionID::kEPAlchemyGetMakingPoison) {
							const bool wantsZero = !d.flags.global && d.comparisonValue.f == 0.0f &&
							                       (d.flags.opCode == RE::CONDITION_ITEM_DATA::OpCode::kEqualTo);
							potionOnly = wantsZero;
						}
					}
				}
				if (tier >= 1 && tier <= 5) {
					tool.tiers[static_cast<std::size_t>(tier - 1)] = mult;
					tool.scope = scope;
					tool.potionOnly = potionOnly;
					++read;
				}
			}
			Log::INFO("[AlchemyMenu] {}: {} tier(s) read from {}: scope {}{}, x{:.2f} / {:.2f} / {:.2f} / {:.2f} / {:.2f}.",
				tool.id, read, tool.perkEditorID, tool.scope.empty() ? "?" : tool.scope, tool.potionOnly ? " (potions only)" : "",
				tool.tiers[0], tool.tiers[1], tool.tiers[2], tool.tiers[3], tool.tiers[4]);
		}
	}

	//-------------------------------------------------------------------------------------------------- baselines

	void AlchemyMenuFeed::ReadBaselines() {
		s_baselines.clear();
		std::ifstream in(C::BaselineFile);
		if (!in) {
			Log::WARN("[AlchemyMenu] {} not found: effect strengths use the mode of each effect's uses instead.", C::BaselineFile);
			return;
		}
		try {
			auto parsed = json::parse(in, nullptr, true, true);
			const auto effects = parsed.find("effects");
			if (effects == parsed.end() || !effects->is_object()) {
				Log::WARN("[AlchemyMenu] {} has no \"effects\" object: ignored.", C::BaselineFile);
				return;
			}
			for (auto it = effects->begin(); it != effects->end(); ++it) {
				Baseline b;
				const std::string axis = it.value().value("axis", "none");
				b.axis = axis == "mag" ? 'm' : axis == "dur" ? 'd' : 'n';
				b.mag = it.value().value("mag", 0.0f);
				b.dur = it.value().value("dur", 0u);
				s_baselines[it.key()] = b;
			}
			Log::INFO("[AlchemyMenu] {} baselines read from {}.", s_baselines.size(), C::BaselineFile);
		} catch (const std::exception& e) {
			Log::WARN("[AlchemyMenu] {} could not be parsed ({}): ignored.", C::BaselineFile, e.what());
			s_baselines.clear();
		}
	}

	//The mode of each effect's magnitude (and, when that mode is 0, of its duration) over every
	//ingredient in the load order: the fallback for an effect the table does not know.
	std::unordered_map<RE::FormID, AlchemyMenuFeed::Baseline> AlchemyMenuFeed::ModesInLoadOrder() {
		std::unordered_map<RE::FormID, std::map<float, int>> mags;
		std::unordered_map<RE::FormID, std::map<std::uint32_t, int>> durs;
		auto* data = RE::TESDataHandler::GetSingleton();
		if (!data) {
			return {};
		}
		for (auto* ingredient : data->GetFormArray<RE::IngredientItem>()) {
			if (!ingredient) {
				continue;
			}
			for (auto* effect : ingredient->effects) {
				if (!effect || !effect->baseEffect) {
					continue;
				}
				const auto id = effect->baseEffect->GetFormID();
				++mags[id][effect->effectItem.magnitude];
				++durs[id][effect->effectItem.duration];
			}
		}
		std::unordered_map<RE::FormID, Baseline> out;
		for (const auto& [id, counts] : mags) {
			Baseline b;
			const auto mode = std::max_element(counts.begin(), counts.end(),
				[](const auto& a, const auto& c) { return a.second < c.second || (a.second == c.second && a.first > c.first); });
			b.mag = mode->first;
			const auto& dcounts = durs[id];
			const auto dmode = std::max_element(dcounts.begin(), dcounts.end(),
				[](const auto& a, const auto& c) { return a.second < c.second || (a.second == c.second && a.first > c.first); });
			b.dur = dmode->first;
			b.axis = b.mag > 0.0f ? 'm' : b.dur > 0 ? 'd' : 'n';
			out[id] = b;
		}
		return out;
	}

	AlchemyMenuFeed::Baseline AlchemyMenuFeed::BaselineFor(RE::EffectSetting* a_effect, const std::unordered_map<RE::FormID, Baseline>& a_modes) {
		if (!a_effect) {
			return {};
		}
		if (!s_baselines.empty()) {
			const auto it = s_baselines.find(RecordKey(a_effect));
			if (it != s_baselines.end()) {
				return it->second;
			}
		}
		const auto it = a_modes.find(a_effect->GetFormID());
		return it != a_modes.end() ? it->second : Baseline{};
	}

	//-------------------------------------------------------------------------------------------------- tools

	//The quest's global while the portable station is up; otherwise (a fixed lab, where the globals are
	//0) the best carried item of the tool's list, so the strip can still show what is carried.
	int AlchemyMenuFeed::CarriedTier(const Tool& a_tool) {
		auto* global = FormCache::Get<RE::TESGlobal>(a_tool.globalEditorID, "AlchemyMenu");
		if (global && global->value >= 1.0f) {
			return std::clamp(static_cast<int>(std::lround(global->value)), 1, 5);
		}
		auto* list = FormCache::Get<RE::BGSListForm>(a_tool.listEditorID, "AlchemyMenu");
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!list || !player) {
			return 0;
		}
		const int size = static_cast<int>(list->forms.size());
		for (int i = 0; i < size; ++i) {
			auto* form = list->forms[static_cast<std::uint32_t>(i)];
			auto* bound = form ? form->As<RE::TESBoundObject>() : nullptr;
			if (bound && player->GetItemCount(bound) > 0) {
				return size - i;  //the quest's rule: index 0 (Master) reads 5
			}
		}
		return 0;
	}

	//-------------------------------------------------------------------------------------------------- data

	void AlchemyMenuFeed::BuildData(RE::GFxMovieView& a_view, RE::GFxValue& a_out) {
		a_view.CreateObject(&a_out);
		if (!s_parsed) {
			Log::WARN("[AlchemyMenu] Fetch before data load: nothing to give.");
			return;
		}
		//The station: the perks need the portable one (IsCurrentFurnitureObj SKYBCraftingAlchemyPortable).
		bool portable = false;
		if (auto* ui = RE::UI::GetSingleton()) {
			auto menu = ui->GetMenu<RE::CraftingMenu>();
			auto* subMenu = menu ? menu->GetCraftingSubMenu() : nullptr;
			RE::TESFurniture* furniture = subMenu ? subMenu->furniture : nullptr;
			portable = furniture && furniture == FormCache::Get<RE::TESFurniture>("SKYBCraftingAlchemyPortable", "AlchemyMenu");
		}
		a_out.SetMember(C::Field::Station, RE::GFxValue(portable));
		a_out.SetMember(C::Field::Preview, RE::GFxValue(s_preview));

		//The tools.
		RE::GFxValue tools;
		a_view.CreateArray(&tools);
		std::string tiers;
		for (const auto& tool : s_tools) {
			RE::GFxValue t;
			a_view.CreateObject(&t);
			const int tier = CarriedTier(tool);
			tiers += std::format("{}{}", tiers.empty() ? "" : "/", tier);
			std::string name;
			if (tier > 0) {
				auto* list = FormCache::Get<RE::BGSListForm>(tool.listEditorID, "AlchemyMenu");
				const int size = list ? static_cast<int>(list->forms.size()) : 0;
				auto* form = list && tier <= size ? list->forms[static_cast<std::uint32_t>(size - tier)] : nullptr;
				const char* full = form ? form->GetName() : nullptr;
				name = full ? full : "";
			}
			t.SetMember("id", RE::GFxValue(tool.id));
			t.SetMember("tier", RE::GFxValue(static_cast<double>(tier)));
			RE::GFxValue nameValue;
			a_view.CreateString(&nameValue, name.c_str());
			t.SetMember("name", nameValue);
			RE::GFxValue scopeValue;
			a_view.CreateString(&scopeValue, tool.scope.c_str());
			t.SetMember("scope", scopeValue);
			t.SetMember("potion", RE::GFxValue(tool.potionOnly));
			const float mult = tier >= 1 && tier <= 5 ? tool.tiers[static_cast<std::size_t>(tier - 1)] : 0.0f;
			t.SetMember("mult", RE::GFxValue(static_cast<double>(mult)));
			RE::GFxValue ladder;
			a_view.CreateArray(&ladder);
			for (const float m : tool.tiers) {
				ladder.PushBack(RE::GFxValue(static_cast<double>(m)));
			}
			t.SetMember("tiers", ladder);
			tools.PushBack(t);
		}
		a_out.SetMember(C::Field::Tools, tools);

		//The strengths: every ingredient, every effect, value over baseline.
		RE::GFxValue strengths;
		a_view.CreateObject(&strengths);
		int ingredients = 0;
		if (auto* data = RE::TESDataHandler::GetSingleton()) {
			const auto modes = ModesInLoadOrder();
			for (auto* ingredient : data->GetFormArray<RE::IngredientItem>()) {
				if (!ingredient || ingredient->effects.empty()) {
					continue;
				}
				RE::GFxValue names;
				RE::GFxValue mults;
				a_view.CreateArray(&names);
				a_view.CreateArray(&mults);
				for (auto* effect : ingredient->effects) {
					auto* base = effect ? effect->baseEffect : nullptr;
					const char* full = base ? base->GetFullName() : nullptr;
					RE::GFxValue nameValue;
					a_view.CreateString(&nameValue, full ? full : "");
					names.PushBack(nameValue);
					double ratio = 0.0;
					if (base) {
						const Baseline b = BaselineFor(base, modes);
						if (b.axis == 'm' && b.mag > 0.0f) {
							ratio = effect->effectItem.magnitude / b.mag;
						} else if (b.axis == 'd' && b.dur > 0) {
							ratio = static_cast<double>(effect->effectItem.duration) / b.dur;
						}
					}
					mults.PushBack(RE::GFxValue(ratio));
				}
				RE::GFxValue row;
				a_view.CreateObject(&row);
				row.SetMember("e", names);
				row.SetMember("m", mults);
				strengths.SetMember(std::to_string(ingredient->GetFormID()).c_str(), row);
				++ingredients;
			}
		}
		a_out.SetMember(C::Field::Strengths, strengths);

		RE::GFxValue colours;
		a_view.CreateObject(&colours);
		colours.SetMember("potent", RE::GFxValue(static_cast<double>(C::PotentColour)));
		colours.SetMember("strong", RE::GFxValue(static_cast<double>(C::StrongColour)));
		colours.SetMember("weak", RE::GFxValue(static_cast<double>(C::WeakColour)));
		a_out.SetMember(C::Field::Colours, colours);
		RE::GFxValue thresholds;
		a_view.CreateObject(&thresholds);
		thresholds.SetMember("potent", RE::GFxValue(static_cast<double>(C::PotentRatio)));
		thresholds.SetMember("strong", RE::GFxValue(static_cast<double>(C::StrongRatio)));
		thresholds.SetMember("weak", RE::GFxValue(static_cast<double>(C::WeakRatio)));
		a_out.SetMember(C::Field::Thresholds, thresholds);

		Log::INFO("[AlchemyMenu] Fetch: station {}, preview {}, {} ingredient(s), tiers {}.",
			portable ? "portable" : "fixed", s_preview ? "on" : "off", ingredients, tiers);
	}
}
