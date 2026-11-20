#pragma once

//The Tween menu's TOOLS entry and SKILLS option, for the radial Tween menu (tweenmenu.swf of 2026-11-15).
//
//That movie builds its own structure from the option files Tween Menu Overhaul hands it
//(_root.TweenMenu_mc.onDataReceived): four columns in TweenMenu_mc.Categories[1..4], each with
//an "entries" array; an entry whose event names an expander (tools.json, SKYBUIToolsExpand)
//takes the entries after it as its "children" and folds them out under itself. It has no
//condition on an option. So, at the start of the frame after the menu opens (on the main
//thread, MainThread::Post), once the movie has its data:
//  - a child whose tool the player does not carry is taken out of its expander's children,
//    and so is a child nothing opens yet (CRAFTING); the column is then rebuilt by the movie's
//    own buildCategory. The expander itself always stays, with an empty fold when nothing is
//    left under it. Entries that are not under an expander are never touched (ALCHEMY, in the
//    magic column, is always listed; its script says so when no mortar is carried).
//  - the movie's pick handler (onCategoryButtonPress, looked up by name on every call) is
//    wrapped: a pick with no entry behind it (the selected row of an empty fold) folds the
//    expander back in instead of being sent out as an option without a name, and a pick of
//    SKILLS is noted.
//SKILLS: its option file names the event SKYBUIOpenSkills, which no script listens for. When
//the Tween menu has closed after that pick, the tab switch the sheets themselves use is sent
//(SKYBMenuSwitch, "SkillsMenu"; SKYBUIUtility.OnTabSwitch), which opens the skills sheet.
//
//Should the movie have no entries yet when the pass runs, it tries once more at the start of
//the next frame. A Tween menu movie without Categories is left alone (logged once).
//
//What counts as a tool is what each tool itself counts: the ESM form lists SKYBSigilStonesAll
//(the sigil quest's) and SKYBDegRepairTools (the repair menu's), through FormCache.
namespace UIs {
	class TweenMenuFilter : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
	public:
		static TweenMenuFilter* GetSingleton();

		//Call once, at kDataLoaded.
		static void Register();

		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
			RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;

	private:
		struct Tool {
			const char* menuEvent;     //the option's papyrus_event, as the movie keeps it (entry.menuEvent)
			const char* listEditorID;  //the form list of what counts as the tool; null: never listed
		};

		//onCategoryButtonPress on TweenMenu_mc.
		class PickHandler : public RE::GFxFunctionHandler {
		public:
			void Call(Params& a_params) override;
		};

		//Takes the children without their tool out of the open Tween menu's expanders, has the
		//movie rebuild those columns, wraps the pick handler. a_retry: this is the one deferred
		//attempt; do not defer again.
		static void Prepare(bool a_retry);
		//The menu has closed: sends the skills tab switch when SKILLS was what closed it.
		static void Closed();
		static void WrapPick(RE::GFxMovie* a_movie, RE::GFxValue& a_clip);
		static const Tool* Find(const char* a_menuEvent);
		//How many of the tool are carried; -1 when its list does not resolve (the tool is then kept).
		static std::int32_t Carried(const Tool& a_tool);

		static constexpr std::array<Tool, 3> s_tools{ {
			{ "SKYBUIOpenSigilMenu", "SKYBSigilStonesAll" },
			{ "SKYBUIOpenRepair", "SKYBDegRepairTools" },
			{ "SKYBUIOpenCrafting", nullptr },  //no script opens anything for it yet
		} };
		//Per open, main thread only: SKILLS was picked and the menu is on its way out.
		static inline bool s_skillsPicked{ false };
	};
}
