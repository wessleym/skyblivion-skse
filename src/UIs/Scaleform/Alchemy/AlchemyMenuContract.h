#pragma once

//The alchemy side of the vanilla Crafting Menu (Interface\craftingmenu.swf, SkyUI's code, patched): the
//apparatus strip and the ingredient effect colours.
//
//The movie has no skse object of its own that could ask for data, so the plugin registers with SKSE's
//Scaleform interface and, every time craftingmenu.swf is loaded, puts two functions on its _root.
//The patched CraftingMenu class calls them from Initialize and on the toggle key.
namespace Alchemy::AlchemyMenuContract {
	//The movie this applies to (the end of GFxMovieDef::GetFileURL, compared case-insensitively).
	inline constexpr const char* MovieFile = "craftingmenu.swf";
	//The name SKSE logs the registration under.
	inline constexpr const char* RegistrationName = "SKYBAlchemy";

	//ActionScript -> C++, members of _root:
	namespace Function {
		//() -> the data object below, built at the time of the call (the menu is open by then).
		inline constexpr const char* Fetch = "SKYBAlchemyFetch";
		//(bool) remembers whether the bonus preview is open, for the session only.
		inline constexpr const char* Remember = "SKYBAlchemyRemember";
	}

	//The data object SKYBAlchemyFetch returns:
	//  station    Boolean  true when the crafting sub-menu's furniture is the portable station the perks need
	//  preview    Boolean  the remembered toggle
	//  tools      Array    one per slot, in strip order: mortar, retort, alembic, calcinator
	//     id        String   "mortar" | "retort" | "alembic" | "calcinator"
	//     tier      Number   0 (none carried) .. 5 (Master): the quest's global, or the inventory when the
	//                        globals read 0 (a fixed lab)
	//     name      String   the carried item's name ("" when none)
	//     scope     String   "all" | "beneficial" | "harmful" | "" (the perk could not be read)
	//     potion    Boolean  the bonus applies to potions only
	//     mult      Number   the multiplier the perk applies at that tier (0 when none / unread)
	//     tiers     Array    the five multipliers, Novice first (for the preview's ladder)
	//  strengths  Object   keyed by the ingredient's FormID as a decimal string:
	//     e         Array    the effect names, record order (the item card's EffectLabel order)
	//     m         Array    value / baseline per effect (0 when the effect has no axis: cures, dispels)
	//  colours    Object   potent, strong, weak: the RGB numbers the movie paints with
	//  thresholds Object   potent, strong, weak: the ratios
	namespace Field {
		inline constexpr const char* Station = "station";
		inline constexpr const char* Preview = "preview";
		inline constexpr const char* Tools = "tools";
		inline constexpr const char* Strengths = "strengths";
		inline constexpr const char* Colours = "colours";
		inline constexpr const char* Thresholds = "thresholds";
	}

	//Where the baseline table ships:
	//{ "effects": { "03EB15:Skyrim.esm": { "axis": "mag"|"dur"|"none", "mag": 5.0, "dur": 0 }, ... } }
	//The key is the effect's six-digit local FormID (hex, upper case) and the file that defines it.
	inline constexpr const char* BaselineFile = "Data/Interface/Skyblivion/Data/SKYBAlchemyEffectBaselines.json";

	//The colours and thresholds the movie uses (x2 green, x1.5 pale green, x0.5 amber; 1.9 / 1.4 / 0.6 so
	//rounding cannot move a tier).
	inline constexpr std::uint32_t PotentColour = 0x7DDC6E;
	inline constexpr std::uint32_t StrongColour = 0xC2E8A8;
	inline constexpr std::uint32_t WeakColour = 0xD7925F;
	inline constexpr float PotentRatio = 1.9f;
	inline constexpr float StrongRatio = 1.4f;
	inline constexpr float WeakRatio = 0.6f;
}
