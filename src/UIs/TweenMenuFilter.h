#pragma once

//The Tween menu's TOOLS entry (Interface\tweenoptions\tools.json), directly under ITEMS: it folds
//out, in place, to the tools the player carries (Sigil Stone, Repair, Alchemy: sigil.json,
//repair.json, alchemy.json), and is hidden while none is carried.
//
//Tween Menu Overhaul reads its option files once at load and, on every TweenMenu open, pushes the
//whole list into the team's movie (_root.TweenMenu_mc.onDataReceived), which fills its four
//category lists at once (the right list lowest priority first: AS2 walks the array last-first).
//It has no condition, no grouping and no filter hook. So, on the same open event, after the
//overhaul's sink:
//  - the three tool entries are taken out of the movie's lists and kept (text, event) as the
//    TOOLS entry's children, in a fixed order, the absent tools dropped; with no child left,
//    TOOLS is removed too; otherwise TOOLS is moved directly under ITEMS;
//  - the movie's pick handler (onCategoryButtonPress) and the TOOLS list's SetEntryText, both
//    looked up by name on every call, are replaced by ours. A pick of TOOLS folds the children
//    out after it or back in; any other pick, a child's among them, goes to the
//    original handler, which has the overhaul close the menu and send the option's event.
//    SetEntryText draws a child as a sub-entry: the November gold (#E2CEAA, white while
//    selected), a smaller label centred like every row, and its fold progress as alpha and slide;
//    every other entry is drawn as the movie draws it, its field put back if a child used it.
//  - the fold is a one-shot onEnterFrame on an empty clip made for it, ~0.2 s, the children one
//    after another; it deletes itself and its clip when done (no polling; nothing outlives it).
//Lone options (round 9): a category column left with exactly one entry after the above (today
//MAGIC: SPELLS; INVENTORY: ITEMS when no tool is carried) shows that entry's label in its
//category box in place of the category name, and the one-entry list beside it is hidden. A pick
//of the box, or Enter on that side, already presses the column's focused entry (the movie's
//onCategoryMaskPress / PressFocusedEntry, handleInput ENTER), so it opens the option as before.
//HighlightCategory (by name) is wrapped so the selection bar beside a lone box stays hidden.
//
//The sink is registered at kDataLoaded and again on kNewGame / kPostLoadGame (idempotent); should
//the lists still be empty when it runs, it tries once more a frame later from the UI task queue.
//
//What counts as a tool is what each tool itself counts: the ESM form lists SKYBSigilStonesAll
//(the sigil quest's), SKYBDegRepairTools (the repair menu's) and SKYBAlchemyMortarPestleList
//(the alchemy quest's mortar list), resolved once by editor ID.
namespace TweenTools {
	class TweenMenuFilter : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
	public:
		static TweenMenuFilter* GetSingleton();

		//Idempotent. Adds the sink the first time; later calls do nothing.
		static void Register();

		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
			RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;

	private:
		struct Tool {
			const char* menuEvent;  //the option's papyrus_event, as the movie keeps it (entry.menuEvent)
			const char* listEditorID;
			RE::BGSListForm* list{ nullptr };
			bool resolved{ false };
		};
		//A tool entry taken out of the list at open, re-made when TOOLS folds out.
		struct Child {
			std::string text;
			std::string menuEvent;
		};

		//onCategoryButtonPress on TweenMenu_mc.
		class PickHandler : public RE::GFxFunctionHandler {
		public:
			void Call(Params& a_params) override;
		};
		//SetEntryText on the TOOLS list: the original, then the sub-entry look.
		class EntryTextHandler : public RE::GFxFunctionHandler {
		public:
			void Call(Params& a_params) override;
		};
		//HighlightCategory on TweenMenu_mc: the original, then the lone side's selection bar hidden.
		class HighlightHandler : public RE::GFxFunctionHandler {
		public:
			void Call(Params& a_params) override;
		};
		//onEnterFrame of the fold's own clip.
		class FoldHandler : public RE::GFxFunctionHandler {
		public:
			void Call(Params& a_params) override;
		};

		//Takes the tool entries out of every category list of the open TweenMenu, hides TOOLS
		//when no tool is carried, puts it under ITEMS, installs the handlers. a_retry: this is the
		//one deferred attempt; do not defer again.
		static void Prepare(bool a_retry);
		static void InstallHandlers(RE::GFxMovie* a_movie);
		//Folds the children out after TOOLS, or back in.
		static void Toggle(RE::GFxMovie* a_movie);
		//Starts the fold clip; a_out: folding out.
		static void StartFold(RE::GFxMovie* a_movie, bool a_out);
		//One frame of the fold; true when it is over.
		static bool StepFold(RE::GFxMovie* a_movie, bool a_finish);
		static void EndFold(RE::GFxMovie* a_movie);
		//Draws one entry clip as a child (a_reveal 0..1) or puts its field back.
		static void StyleClip(RE::GFxValue& a_clip, bool a_child, bool a_lit, double a_reveal);
		static void SetToolsLabel(RE::GFxValue& a_list);
		//Round 9: every column with exactly one entry takes it into its category box.
		static void ApplyLoneHeaders(RE::GFxMovie* a_movie);
		static Tool* Find(const char* a_menuEvent);
		static std::int32_t Carried(Tool& a_tool);

		static inline bool s_registered{ false };
		static inline std::array<Tool, 3> s_tools{ {
			{ "SKYBUIOpenSigilMenu", "SKYBSigilStonesAll" },
			{ "SKYBUIOpenRepair", "SKYBDegRepairTools" },
			{ "SKYBUIOpenAlchemy", "SKYBAlchemyMortarPestleList" },
		} };
		//Per open: the carried tools in s_tools order, the list TOOLS sits in, TOOLS' own label,
		//whether it is folded out, and the fold under way (if any).
		static inline std::vector<Child> s_children;
		static inline std::string s_toolsList;
		static inline std::string s_toolsText;
		static inline bool s_expanded{ false };
		static inline bool s_folding{ false };
		static inline bool s_foldOut{ false };
		static inline std::chrono::steady_clock::time_point s_foldStart;
		//Per open: which columns (top, left, right, bottom = the movie's states 1 to 4) are lone.
		static inline std::array<bool, 4> s_lone{};
	};
}
