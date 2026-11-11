#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

//Scroll crafting (SKYBScrollCrafting.esp, SKYBScrollCraft.psc, SKYBScrollCraftMenu.swf): the recipes come from
//JSON files, not from the plugin, so they can be edited or added without rebuilding the ESP, and other mods can
//add their own.
//
//Folder (relative to the game folder): Data/Interface/Skyblivion/ConfigFiles/ScrollCrafting
//  recipes.json is read first (the base set, generated from Skyblivion.esm's scrolls); then every other *.json
//  in the folder, in name order (case-insensitive). A mod adds recipes by shipping its own file there, e.g.
//  ScrollCrafting/MyMod.json, with recipes for its own SCRL records ("form": "000801|MyMod.esp", light plugins
//  included). The file format:
//  { "version": 1,
//    "recipes": [ { "scroll": "MyModScrollOfFoo",                  editor ID, for people and the log
//                   "form": "000801|MyMod.esp",                     local form ID | plugin (what the game uses)
//                   "school": 0,                                    0 Alteration .. 4 Restoration
//                   "tier": 1,                                      1 Novice .. 5 Master = the seats filled
//                   "delivery": "A",                                A aimed, T touch, R area, S self, G ground
//                   "seats": { "top": "Y" },                        seat (top, bottom, left, right, centre) -> A..Z
//                   "disabled": false },                            optional: true removes the scroll's recipe
//                 ... ] }
//  A recipe for a scroll that an earlier file already has replaces it (a mod can re-seat a base scroll); with
//  "disabled": true it removes it. Two recipes of one school with the same seats and delivery: the one from the
//  later file (or later in the same file) is skipped and logged.
//Read once at kDataLoaded (LoadOnce). Fail-soft per recipe: a recipe with a bad or missing field, a form that
//does not resolve or is not a scroll, or a tier unlike its seat count is logged with its file and scroll id and
//skipped. A file that is missing or not JSON is logged and skipped; recipes.json missing is reported in Status().
//
//Papyrus (Source/Scripts/SKYBScrollCraftData.psc, all global native): GetStatus, GetScroll(index), Find(scroll),
//Resolve(school, wheel, aspect), KnownTable(known list), GetSeats, GetCosts, GetCost(index), PlayUISound(editor ID),
//HideInventory, ShowInventory. Adding to the known list stays in Papyrus (FormList.AddForm), the game's own binding.
//The wheel is five characters in seat order (top, bottom, left, right, centre), "." for an empty seat. Indices are
//positions in this session's store: the quest keeps known recipes as scroll forms (a FormList), never as indices.
//
//Loaded on the main thread at kDataLoaded; read from the Papyrus VM thread afterwards: the vector never changes
//after LoadOnce, so no lock.
namespace ScrollCraft {

	struct Recipe {
		std::string scroll;    //editor ID as written in the file
		std::string formRef;   //"HEX|Plugin"
		std::string source;    //the file it came from
		int school = 0;
		int tier = 0;
		std::string delivery;
		std::string wheel;     //five characters in seat order, "." empty
		int essCommon = 0;     //the essences a craft takes: Verdant, of the school, Radiant
		int essSchool = 1;
		int essRare = 0;
		RE::TESForm* form = nullptr;
	};

	class ScrollRecipeStore {
	public:
		static constexpr const char* DataFolder = "Data/Interface/Skyblivion/ConfigFiles/ScrollCrafting";
		static constexpr const char* BaseFile = "recipes.json";
		static constexpr const char* ScriptName = "SKYBScrollCraftData";
		static constexpr int SeatCount = 5;

		//kDataLoaded: parses the folder once. Later calls do nothing.
		static void LoadOnce();
		static void RegisterPapyrus(RE::BSScript::Internal::VirtualMachine* vm);

		[[nodiscard]] static const std::vector<Recipe>& Recipes() { return s_recipes; }
		[[nodiscard]] static const std::string& Status() { return s_status; }
		[[nodiscard]] static int Find(const RE::TESForm* a_form);
		//the recipe of this school with these seats: the aspect's delivery when one has it, else the plain form
		//(neither touch nor area), else the first; -1 when none
		[[nodiscard]] static int Resolve(int a_school, std::string_view a_wheel, int a_aspect);
		//the seats a tier fills ("seatsPerTier", default 2 3 4 5 5) and its essence cost ("essenceCost",
		//default 2-1-0 1-2-0 1-3-0 0-3-1 0-2-3 as Verdant-school-Radiant), from recipes.json; a recipe's own
		//"essences" override its tier's cost
		[[nodiscard]] static int SeatsFor(int a_tier);
		static inline int s_seats[SeatCount] = { 2, 3, 4, 5, 5 };
		static inline int s_cost[SeatCount][3] = { { 2, 1, 0 }, { 1, 2, 0 }, { 1, 3, 0 }, { 0, 3, 1 }, { 0, 2, 3 } };

	private:
		static inline bool s_loaded = false;
		static inline std::vector<Recipe> s_recipes;
		static inline std::string s_status = "not loaded";
	};
}
