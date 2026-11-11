#include "TweenMenuFilter.h"

#include "Platform/FormCache.h"
#include "Platform/MainThread.h"
#include "UIs/Scaleform/ScaleformUtility.h"

namespace TweenTools {
	namespace {
		//The movie's four lists: the overhaul fills whichever the option's category names.
		constexpr std::array<const char*, 4> kLists{
			"_root.TweenMenu_mc.CategoryListTop",
			"_root.TweenMenu_mc.CategoryListLeft",
			"_root.TweenMenu_mc.CategoryListRight",
			"_root.TweenMenu_mc.CategoryListBottom",
		};
		constexpr const char* kMenuClip = "_root.TweenMenu_mc";
		//The category boxes and the selection bars beside them, in kLists' order (the movie's
		//TOP_STATE 1, LEFT_STATE 2, RIGHT_STATE 3, BOTTOM_STATE 4).
		constexpr std::array<const char*, 4> kBoxes{
			"_root.TweenMenu_mc.CategoryTop",
			"_root.TweenMenu_mc.CategoryLeft",
			"_root.TweenMenu_mc.CategoryRight",
			"_root.TweenMenu_mc.CategoryBottom",
		};
		constexpr std::array<const char*, 4> kOptionBars{
			"TopOptionHighlight_mc",
			"LeftOptionHighlight_mc",
			"RightOptionHighlight_mc",
			"BottomOptionHighlight_mc",
		};
		constexpr const char* kHighlight = "HighlightCategory";
		constexpr const char* kOriginalHighlight = "SKYBOriginalHighlightCategory";
		//tools.json's papyrus_event: never sent anywhere while this plugin runs, it marks the entry.
		constexpr const char* kToolsEvent = "SKYBUIToolsExpand";
		//ITEMS (the build's inventory.json): TOOLS goes directly under it.
		constexpr const char* kItemsEvent = "InventoryMenu";
		//The movie's members replaced, and the names the originals are kept under.
		constexpr const char* kPickHandler = "onCategoryButtonPress";
		constexpr const char* kOriginalPick = "SKYBOriginalCategoryPress";
		constexpr const char* kEntryText = "SetEntryText";
		constexpr const char* kOriginalEntryText = "SKYBOriginalSetEntryText";
		constexpr const char* kFoldClip = "SKYBToolsFold";
		constexpr const char* kExpandSound = "UIMenuFocus";

		//The sub-entry look: gold at rest, white while selected.
		constexpr std::uint32_t kChildColour = 0xE2CEAA;
		constexpr std::uint32_t kChildLitColour = 0xFFFFFF;
		constexpr double kChildScale = 82.0;    //% of the entry's label
		constexpr double kChildSlide = 0.10;    //of the field's width, the fold's travel

		//The fold: each child takes kStepMs to appear, the next starts kStaggerMs later.
		constexpr double kStepMs = 110.0;
		constexpr double kStaggerMs = 45.0;

		bool EntryEvent(const RE::GFxValue& a_entry, RE::GFxValue& a_event)
		{
			return a_entry.IsObject() && a_entry.GetMember("menuEvent", &a_event) && a_event.IsString();
		}

		bool IsEvent(const RE::GFxValue& a_entry, const char* a_name)
		{
			RE::GFxValue event;
			return EntryEvent(a_entry, event) && std::strcmp(event.GetString(), a_name) == 0;
		}

		bool IsChild(const RE::GFxValue& a_entry)
		{
			RE::GFxValue flag;
			return a_entry.IsObject() && a_entry.GetMember("skybToolChild", &flag) && flag.IsBool() && flag.GetBool();
		}

		double Number(const RE::GFxValue& a_obj, const char* a_name, double a_default)
		{
			RE::GFxValue v;
			return a_obj.GetMember(a_name, &v) && v.IsNumber() ? v.GetNumber() : a_default;
		}

		void Redraw(RE::GFxMovie* a_movie, const std::string& a_listPath)
		{
			const std::string redraw = a_listPath + ".InvalidateData";
			a_movie->Invoke(redraw.c_str(), nullptr, nullptr, 0);
		}

		//The array is the list's own (EntriesA): it is edited in place, never replaced.
		void Rebuild(RE::GFxValue& a_list, const std::vector<RE::GFxValue>& a_entries)
		{
			a_list.RemoveElements(0, -1);
			for (const auto& entry : a_entries) {
				a_list.PushBack(entry);
			}
		}

		double Clamp01(double a_v) { return a_v < 0.0 ? 0.0 : a_v > 1.0 ? 1.0 : a_v; }
	}

	TweenMenuFilter* TweenMenuFilter::GetSingleton()
	{
		static TweenMenuFilter instance;
		return &instance;
	}

	void TweenMenuFilter::Register()
	{
		auto ui = RE::UI::GetSingleton();
		if (!ui) {
			return;
		}
		ui->AddEventSink<RE::MenuOpenCloseEvent>(GetSingleton());
		Log::INFO("TweenMenuFilter: registered; the Tween menu's TOOLS entry folds out to the tools carried and hides when none is");
	}

	RE::BSEventNotifyControl TweenMenuFilter::ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
		RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
	{
		//Menu events arrive on worker threads: the movie's lists, and the state its handlers read, are
		//changed on the main thread (Platform\MainThread.h). Posting both keeps an open and a close in order.
		if (a_event && a_event->menuName == RE::TweenMenu::MENU_NAME) {
			if (a_event->opening) {
				MainThread::Post([] { Prepare(false); });
			} else {
				MainThread::Post(Reset);
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	void TweenMenuFilter::Reset()
	{
		s_children.clear();
		s_toolsList.clear();
		s_expanded = false;
		s_folding = false;
		s_lone = {};
	}

	const TweenMenuFilter::Tool* TweenMenuFilter::Find(const char* a_menuEvent)
	{
		if (!a_menuEvent) {
			return nullptr;
		}
		for (const auto& tool : s_tools) {
			if (std::strcmp(tool.menuEvent, a_menuEvent) == 0) {
				return &tool;
			}
		}
		return nullptr;
	}

	std::int32_t TweenMenuFilter::Carried(const Tool& a_tool)
	{
		//A list that does not resolve is logged once by the form table; the tool is then left listed.
		auto* list = FormCache::Get<RE::BGSListForm>(a_tool.listEditorID, "TweenMenuFilter");
		auto  player = RE::PlayerCharacter::GetSingleton();
		if (!list || !player) {
			return -1;  //unknown: keep the tool
		}
		std::int32_t total = 0;
		for (auto* form : list->forms) {
			auto obj = form ? form->As<RE::TESBoundObject>() : nullptr;
			if (obj) {
				total += std::max(0, player->GetItemCount(obj));
			}
		}
		return total;
	}

	void TweenMenuFilter::Prepare(bool a_retry)
	{
		const auto view = ScaleformUtility::OpenMenuMovie(RE::TweenMenu::MENU_NAME);
		if (!view) {
			return;
		}
		auto* movie = view.get();
		Reset();
		int seen = 0;
		int dropped = 0;
		bool toolsSeen = false;
		std::array<std::optional<Child>, 3> found{};
		//Whether the carried tools can go under TOOLS: the entry is in a list and its pick can be taken over.
		//When not (tools.json missing, or an overhaul version without the handler), the carried tools stay
		//where the overhaul put them, so they are still reachable; only the ones not carried are taken out.
		for (const char* listPath : kLists) {
			RE::GFxValue list;
			if (!movie->GetVariable(&list, (std::string(listPath) + ".entryList").c_str()) || !list.IsArray()) {
				continue;
			}
			for (std::uint32_t i = 0; i < list.GetArraySize() && !toolsSeen; ++i) {
				RE::GFxValue entry;
				if (list.GetElement(i, &entry) && IsEvent(entry, kToolsEvent)) {
					toolsSeen = true;
					s_toolsList = listPath;
				}
			}
			if (toolsSeen) {
				break;
			}
		}
		RE::GFxValue clip;
		RE::GFxValue pick;
		const bool pickable = movie->GetVariable(&clip, kMenuClip) && clip.IsObject() && clip.GetMember(kPickHandler, &pick) && !pick.IsUndefined();
		const bool canFold = toolsSeen && pickable;
		static bool s_pickWarned = false;
		if (toolsSeen && !pickable && !std::exchange(s_pickWarned, true)) {
			Log::WARN("TweenMenuFilter: {}.{} not found; the carried tools stay as their own entries", kMenuClip, kPickHandler);
		}
		//Pass 1: the tool entries out of the lists, the carried ones kept (in s_tools order).
		for (const char* listPath : kLists) {
			const std::string entries = std::string(listPath) + ".entryList";
			RE::GFxValue list;
			if (!movie->GetVariable(&list, entries.c_str()) || !list.IsArray()) {
				continue;
			}
			bool changed = false;
			for (std::uint32_t i = list.GetArraySize(); i > 0; --i) {
				const std::uint32_t idx = i - 1;
				RE::GFxValue entry;
				RE::GFxValue event;
				if (!list.GetElement(idx, &entry) || !EntryEvent(entry, event)) {
					continue;
				}
				const Tool* tool = Find(event.GetString());  //null for TOOLS itself and for every other entry
				if (!tool) {
					continue;
				}
				++seen;
				RE::GFxValue text;
				const char* label = entry.GetMember("text", &text) && text.IsString() ? text.GetString() : tool->menuEvent;
				if (Carried(*tool) != 0) {
					if (!canFold) {
						continue;  //left in place: there is no TOOLS to put it under
					}
					found[static_cast<std::size_t>(tool - s_tools.data())] = Child{ label, tool->menuEvent };
				} else {
					++dropped;
				}
				list.RemoveElements(idx, 1);
				changed = true;
			}
			if (changed) {
				Redraw(movie, listPath);
			}
		}
		for (auto& child : found) {
			if (child) {
				s_children.push_back(*child);
			}
		}
		if (seen == 0 && !toolsSeen && !a_retry) {
			//The overhaul has not filled the lists yet: once more at the start of the next frame, on the main thread.
			MainThread::Post([] { Prepare(true); });
			return;
		}
		//Pass 2: TOOLS itself: gone when nothing would be under it, else directly under ITEMS.
		bool underItems = false;
		if (toolsSeen) {
			const std::string entries = s_toolsList + ".entryList";
			RE::GFxValue list;
			if (movie->GetVariable(&list, entries.c_str()) && list.IsArray()) {
				std::vector<RE::GFxValue> rest;
				RE::GFxValue tools;
				for (std::uint32_t i = 0; i < list.GetArraySize(); ++i) {
					RE::GFxValue entry;
					if (!list.GetElement(i, &entry)) {
						continue;
					}
					if (IsEvent(entry, kToolsEvent)) {
						tools = entry;
					} else {
						rest.push_back(entry);
					}
				}
				std::vector<RE::GFxValue> ordered;
				if (!s_children.empty()) {
					for (const auto& entry : rest) {
						ordered.push_back(entry);
						if (IsEvent(entry, kItemsEvent)) {
							ordered.push_back(tools);
							underItems = true;
						}
					}
					if (!underItems) {
						ordered.push_back(tools);  //no ITEMS in this list: last
					}
				} else {
					ordered = rest;
				}
				Rebuild(list, ordered);
				Redraw(movie, s_toolsList);
			}
			if (s_children.empty()) {
				s_toolsList.clear();
			} else {
				InstallHandlers(movie);
			}
		}
		ApplyLoneHeaders(movie);
		std::string state = "absent";
		if (toolsSeen) {
			state = s_children.empty() ? "hidden" : std::format("with {} tool(s){}", s_children.size(), underItems ? ", under ITEMS" : ", ITEMS not found");
		}
		Log::INFO("TweenMenuFilter: {} tool option(s) seen, {} not carried; TOOLS {}{}", seen, dropped, state,
			a_retry ? " (deferred pass)" : "");
	}

	void TweenMenuFilter::ApplyLoneHeaders(RE::GFxMovie* a_movie)
	{
		static RE::GPtr<HighlightHandler> highlight = RE::make_gptr<HighlightHandler>();
		std::string lone;
		for (std::size_t i = 0; i < kLists.size(); ++i) {
			const std::string entries = std::string(kLists[i]) + ".entryList";
			RE::GFxValue list;
			RE::GFxValue listClip;
			if (!a_movie->GetVariable(&list, entries.c_str()) || !list.IsArray() || list.GetArraySize() != 1 ||
				!a_movie->GetVariable(&listClip, kLists[i]) || !listClip.IsObject()) {
				continue;
			}
			RE::GFxValue entry;
			RE::GFxValue text;
			if (!list.GetElement(0, &entry) || !entry.IsObject() || !entry.GetMember("text", &text) || !text.IsString()) {
				continue;
			}
			//The box's own field takes the entry's label, already upper-cased by the movie; only its
			//text changes (its size, shrink-to-fit and centring are the movie's).
			const std::string field = std::string(kBoxes[i]) + ".textField";
			RE::GFxValue box;
			if (!a_movie->GetVariable(&box, field.c_str()) || !box.IsObject()) {
				continue;
			}
			box.SetMember("text", text);
			listClip.SetMember("_visible", RE::GFxValue(false));
			s_lone[i] = true;
			lone += std::format("{}{}", lone.empty() ? "" : ", ", text.GetString());
		}
		if (lone.empty()) {
			return;
		}
		RE::GFxValue clip;
		RE::GFxValue original;
		if (a_movie->GetVariable(&clip, kMenuClip) && clip.IsObject() && clip.GetMember(kHighlight, &original) && !original.IsUndefined()) {
			clip.SetMember(kOriginalHighlight, original);
			RE::GFxValue fn;
			a_movie->CreateFunction(&fn, highlight.get());
			clip.SetMember(kHighlight, fn);
		} else {
			Log::WARN("TweenMenuFilter: {}.{} not found; a lone column's selection bar may show", kMenuClip, kHighlight);
		}
		Log::INFO("TweenMenuFilter: lone option(s) in their category box: {}", lone);
	}

	void TweenMenuFilter::HighlightHandler::Call(Params& a_params)
	{
		if (!a_params.thisPtr) {
			return;
		}
		a_params.thisPtr->Invoke(kOriginalHighlight, nullptr, a_params.args, a_params.argCount);
		if (a_params.argCount < 1 || !a_params.args[0].IsNumber()) {
			return;
		}
		const int state = static_cast<int>(a_params.args[0].GetNumber());
		if (state < 1 || state > 4 || !s_lone[static_cast<std::size_t>(state - 1)]) {
			return;
		}
		RE::GFxValue bar;
		if (a_params.thisPtr->GetMember(kOptionBars[static_cast<std::size_t>(state - 1)], &bar) && bar.IsObject()) {
			bar.SetMember("_visible", RE::GFxValue(false));
		}
	}

	void TweenMenuFilter::InstallHandlers(RE::GFxMovie* a_movie)
	{
		static RE::GPtr<PickHandler> pick = RE::make_gptr<PickHandler>();
		static RE::GPtr<EntryTextHandler> entryText = RE::make_gptr<EntryTextHandler>();
		//A fresh movie each open: the members found here are still the movie's own.
		RE::GFxValue clip;
		RE::GFxValue original;
		//Prepare checked that the handler exists (canFold) before calling this.
		if (a_movie->GetVariable(&clip, kMenuClip) && clip.IsObject() && clip.GetMember(kPickHandler, &original) && !original.IsUndefined()) {
			clip.SetMember(kOriginalPick, original);
			RE::GFxValue fn;
			a_movie->CreateFunction(&fn, pick.get());
			clip.SetMember(kPickHandler, fn);
		}
		RE::GFxValue list;
		if (a_movie->GetVariable(&list, s_toolsList.c_str()) && list.IsObject() && list.GetMember(kEntryText, &original) && !original.IsUndefined()) {
			list.SetMember(kOriginalEntryText, original);
			RE::GFxValue fn;
			a_movie->CreateFunction(&fn, entryText.get());
			list.SetMember(kEntryText, fn);
		} else {
			Log::WARN("TweenMenuFilter: {}.{} not found; the tools are drawn as plain entries", s_toolsList, kEntryText);
		}
	}

	void TweenMenuFilter::PickHandler::Call(Params& a_params)
	{
		auto movie = a_params.movie;
		if (!movie || !a_params.thisPtr) {
			return;
		}
		RE::GFxValue entry;
		const bool tools = a_params.argCount > 0 && a_params.args[0].IsObject() && a_params.args[0].GetMember("entry", &entry) &&
		                   IsEvent(entry, kToolsEvent);
		if (tools) {
			RE::GFxValue opening;
			if (a_params.thisPtr->GetMember("bOpeningMenu", &opening) && opening.IsBool() && opening.GetBool()) {
				return;  //a pick is already on its way out
			}
			Toggle(movie);
			return;
		}
		//Every other pick: the movie's own handler (it has the overhaul close the menu and send the event).
		a_params.thisPtr->Invoke(kOriginalPick, nullptr, a_params.args, a_params.argCount);
	}

	void TweenMenuFilter::EntryTextHandler::Call(Params& a_params)
	{
		if (!a_params.thisPtr || a_params.argCount < 2) {
			return;
		}
		a_params.thisPtr->Invoke(kOriginalEntryText, nullptr, a_params.args, a_params.argCount);
		RE::GFxValue clip = a_params.args[0];
		const RE::GFxValue& entry = a_params.args[1];
		if (!clip.IsObject()) {
			return;
		}
		//SetEntry sets the clip's alpha just before this: 100 is the selected entry on every platform.
		const double alpha = Number(clip, "_alpha", 100.0);
		clip.SetMember("SKYBBaseAlpha", RE::GFxValue(alpha));
		const bool child = IsChild(entry);
		StyleClip(clip, child, alpha >= 99.5, child ? Number(entry, "skybReveal", 1.0) : 1.0);
	}

	void TweenMenuFilter::StyleClip(RE::GFxValue& a_clip, bool a_child, bool a_lit, double a_reveal)
	{
		RE::GFxValue field;
		if (!a_clip.GetMember("textField", &field) || !field.IsObject()) {
			return;
		}
		//The field as the movie placed it, taken once per clip, before anything of ours has touched it
		//(only this function moves a field, and it takes this first). The movie places every entry's
		//field at 87.05 % scale (PlaceObject in the entry sprites), so the scale is part of the place:
		//putting a row back at an absolute 100 % grows it from its left edge and moves its centred words.
		if (!a_clip.HasMember("SKYBBaseX")) {
			a_clip.SetMember("SKYBBaseX", RE::GFxValue(Number(field, "_x", 0.0)));
			a_clip.SetMember("SKYBBaseY", RE::GFxValue(Number(field, "_y", 0.0)));
			a_clip.SetMember("SKYBBaseW", RE::GFxValue(Number(field, "_width", 0.0)));
			a_clip.SetMember("SKYBBaseH", RE::GFxValue(Number(field, "_height", 0.0)));
			a_clip.SetMember("SKYBBaseXScale", RE::GFxValue(Number(field, "_xscale", 100.0)));
			a_clip.SetMember("SKYBBaseYScale", RE::GFxValue(Number(field, "_yscale", 100.0)));
		}
		const double x = Number(a_clip, "SKYBBaseX", 0.0);
		const double y = Number(a_clip, "SKYBBaseY", 0.0);
		const double w = Number(a_clip, "SKYBBaseW", 0.0);
		const double h = Number(a_clip, "SKYBBaseH", 0.0);
		const double xs = Number(a_clip, "SKYBBaseXScale", 100.0);
		const double ys = Number(a_clip, "SKYBBaseYScale", 100.0);
		RE::GFxValue styled;
		const bool wasChild = a_clip.GetMember("SKYBStyled", &styled) && styled.IsBool() && styled.GetBool();
		if (!a_child) {
			//A row that is not a tool: untouched unless a tool used this clip before, then exactly
			//the movie's place again (the text and colour are the original SetEntryText's).
			if (wasChild) {
				field.SetMember("_xscale", RE::GFxValue(xs));
				field.SetMember("_yscale", RE::GFxValue(ys));
				field.SetMember("_x", RE::GFxValue(x));
				field.SetMember("_y", RE::GFxValue(y));
				a_clip.SetMember("SKYBStyled", RE::GFxValue(false));
			}
			return;
		}
		//A tool: kChildScale of the movie's own scale, centred on the movie's box (a field's
		//_x / _y is its top-left, its width scales with _xscale).
		const double f = kChildScale / 100.0;
		field.SetMember("_xscale", RE::GFxValue(xs * f));
		field.SetMember("_yscale", RE::GFxValue(ys * f));
		field.SetMember("_x", RE::GFxValue(x + w * (1.0 - f) / 2.0 - w * kChildSlide * (1.0 - a_reveal)));
		field.SetMember("_y", RE::GFxValue(y + h * (1.0 - f) / 2.0));
		a_clip.SetMember("SKYBStyled", RE::GFxValue(true));
		field.SetMember("textColor", RE::GFxValue(static_cast<double>(a_lit ? kChildLitColour : kChildColour)));
		a_clip.SetMember("_alpha", RE::GFxValue(Number(a_clip, "SKYBBaseAlpha", 100.0) * a_reveal));
	}

	void TweenMenuFilter::Toggle(RE::GFxMovie* a_movie)
	{
		if (s_toolsList.empty()) {
			return;
		}
		if (s_folding) {
			StepFold(a_movie, true);  //a fold under way ends at once
			EndFold(a_movie);
		}
		const std::string entries = s_toolsList + ".entryList";
		RE::GFxValue list;
		if (!a_movie->GetVariable(&list, entries.c_str()) || !list.IsArray()) {
			return;
		}
		if (s_expanded) {
			//Back in: the children fade out (last first), then leave the list (EndFold).
			s_expanded = false;
			Redraw(a_movie, s_toolsList);
			StartFold(a_movie, false);
		} else {
			//Out, right after TOOLS: the array has no insert, so it is rebuilt in order.
			std::vector<RE::GFxValue> kept;
			const std::uint32_t n = list.GetArraySize();
			kept.reserve(n + s_children.size());
			for (std::uint32_t i = 0; i < n; ++i) {
				RE::GFxValue entry;
				if (!list.GetElement(i, &entry)) {
					continue;
				}
				kept.push_back(entry);
				if (IsEvent(entry, kToolsEvent)) {
					for (const auto& child : s_children) {
						RE::GFxValue made;
						a_movie->CreateObject(&made);
						made.SetMember("text", RE::GFxValue(child.text.c_str()));
						made.SetMember("menuEvent", RE::GFxValue(child.menuEvent.c_str()));
						made.SetMember("papyrus", RE::GFxValue(true));
						made.SetMember("skybToolChild", RE::GFxValue(true));
						made.SetMember("skybReveal", RE::GFxValue(0.0));
						kept.push_back(made);
					}
				}
			}
			Rebuild(list, kept);
			s_expanded = true;
			Redraw(a_movie, s_toolsList);
			StartFold(a_movie, true);
		}
		RE::PlaySound(kExpandSound);
		Log::INFO("TweenMenuFilter: TOOLS {} ({} tool(s))", s_expanded ? "folded out" : "folded in", s_children.size());
	}

	void TweenMenuFilter::StartFold(RE::GFxMovie* a_movie, bool a_out)
	{
		static RE::GPtr<FoldHandler> fold = RE::make_gptr<FoldHandler>();
		s_foldOut = a_out;
		s_foldStart = std::chrono::steady_clock::now();
		s_folding = true;
		RE::GFxValue list;
		RE::GFxValue clip;
		if (a_movie->GetVariable(&list, s_toolsList.c_str()) && list.IsObject()) {
			RE::GFxValue depth;
			list.Invoke("getNextHighestDepth", &depth);
			RE::GFxValue args[2];
			args[0] = kFoldClip;
			args[1] = depth;
			list.Invoke("createEmptyMovieClip", &clip, args, 2);
		}
		if (!clip.IsObject()) {
			//No clip: no animation, the end state at once.
			StepFold(a_movie, true);
			EndFold(a_movie);
			return;
		}
		RE::GFxValue fn;
		a_movie->CreateFunction(&fn, fold.get());
		clip.SetMember("onEnterFrame", fn);
		StepFold(a_movie, false);  //frame 0, so nothing shows at full alpha first
	}

	void TweenMenuFilter::FoldHandler::Call(Params& a_params)
	{
		if (!a_params.movie) {
			return;
		}
		if (!s_folding || StepFold(a_params.movie, false)) {
			EndFold(a_params.movie);
		}
	}

	bool TweenMenuFilter::StepFold(RE::GFxMovie* a_movie, bool a_finish)
	{
		RE::GFxValue list;
		if (s_toolsList.empty() || !a_movie->GetVariable(&list, s_toolsList.c_str()) || !list.IsObject()) {
			return true;
		}
		RE::GFxValue entries;
		if (!list.GetMember("entryList", &entries) || !entries.IsArray()) {
			return true;
		}
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s_foldStart).count();
		//The children in list order, numbered for the stagger (out: first first; in: last first).
		std::vector<std::uint32_t> children;
		for (std::uint32_t i = 0; i < entries.GetArraySize(); ++i) {
			RE::GFxValue entry;
			if (entries.GetElement(i, &entry) && IsChild(entry)) {
				children.push_back(i);
			}
		}
		const std::size_t n = children.size();
		const double total = kStepMs + kStaggerMs * (n > 0 ? static_cast<double>(n - 1) : 0.0);
		const bool done = a_finish || ms >= total;
		for (std::size_t k = 0; k < n; ++k) {
			const double order = static_cast<double>(s_foldOut ? k : n - 1 - k);
			double t = done ? 1.0 : Clamp01((ms - order * kStaggerMs) / kStepMs);
			t = 1.0 - (1.0 - t) * (1.0 - t);  //ease out
			const double reveal = s_foldOut ? t : 1.0 - t;
			RE::GFxValue entry;
			entries.GetElement(children[k], &entry);
			entry.SetMember("skybReveal", RE::GFxValue(reveal));
		}
		//The clips on screen now: restyle the children's (the list itself is not redrawn).
		const double shown = Number(list, "iMaxItemsShown", 0.0);
		const double selected = Number(list, "iSelectedIndex", -1.0);
		for (int c = 0; c < static_cast<int>(shown); ++c) {
			RE::GFxValue clip;
			const std::string name = std::format("Entry{}", c);
			if (!list.GetMember(name.c_str(), &clip) || !clip.IsObject()) {
				continue;
			}
			RE::GFxValue index;
			RE::GFxValue visible;
			if (!clip.GetMember("itemIndex", &index) || !index.IsNumber() || !clip.GetMember("_visible", &visible) || !visible.GetBool()) {
				continue;
			}
			RE::GFxValue entry;
			if (!entries.GetElement(static_cast<std::uint32_t>(index.GetNumber()), &entry) || !IsChild(entry)) {
				continue;
			}
			const bool lit = Number(clip, "SKYBBaseAlpha", 0.0) >= 99.5 || index.GetNumber() == selected;
			StyleClip(clip, true, lit, Number(entry, "skybReveal", 1.0));
		}
		return done;
	}

	void TweenMenuFilter::EndFold(RE::GFxMovie* a_movie)
	{
		const bool wasFolding = s_folding;
		s_folding = false;
		RE::GFxValue list;
		if (s_toolsList.empty() || !a_movie->GetVariable(&list, s_toolsList.c_str()) || !list.IsObject()) {
			return;
		}
		RE::GFxValue clip;
		if (list.GetMember(kFoldClip, &clip) && clip.IsObject()) {
			clip.DeleteMember("onEnterFrame");
			clip.Invoke("removeMovieClip");
		}
		if (!wasFolding || s_foldOut) {
			return;
		}
		//Folded in: the children leave the list.
		RE::GFxValue entries;
		if (list.GetMember("entryList", &entries) && entries.IsArray()) {
			for (std::uint32_t i = entries.GetArraySize(); i > 0; --i) {
				RE::GFxValue entry;
				if (entries.GetElement(i - 1, &entry) && IsChild(entry)) {
					entries.RemoveElements(i - 1, 1);
				}
			}
			Redraw(a_movie, s_toolsList);
		}
	}
}
