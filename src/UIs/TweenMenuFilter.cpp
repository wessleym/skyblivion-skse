#include "TweenMenuFilter.h"

#include "Papyrus/ModEvents.h"
#include "Platform/FormCache.h"
#include "Platform/MainThread.h"
#include "UIs/Scaleform/ScaleformUtility.h"

namespace UIs {
	namespace {
		constexpr const char* kMenuClip = "_root.TweenMenu_mc";
		//The movie's columns: Categories[1..4] (its TOP_STATE, LEFT_STATE, RIGHT_STATE, BOTTOM_STATE).
		constexpr const char* kColumns = "Categories";
		constexpr std::uint32_t kFirstColumn = 1;
		constexpr std::uint32_t kLastColumn = 4;
		//The movie's pick handler, and the name its own function is kept under once wrapped.
		constexpr const char* kPick = "onCategoryButtonPress";
		constexpr const char* kMoviePick = "SKYBMoviePick";
		//skills.json's papyrus_event, and the tab switch that opens the skills sheet.
		constexpr const char* kSkillsEvent = "SKYBUIOpenSkills";
		constexpr const char* kTabSwitchEvent = "SKYBMenuSwitch";
		constexpr const char* kSkillsTab = "SkillsMenu";

		//An entry's event name, or null when a_entry is no entry. a_hold keeps the string alive.
		const char* EventOf(const RE::GFxValue& a_entry, RE::GFxValue& a_hold)
		{
			if (!a_entry.IsObject() || !a_entry.GetMember("menuEvent", &a_hold) || !a_hold.IsString()) {
				return nullptr;
			}
			return a_hold.GetString();
		}

		std::string TextOf(const RE::GFxValue& a_entry)
		{
			RE::GFxValue text;
			return a_entry.IsObject() && a_entry.GetMember("text", &text) && text.IsString() ? text.GetString() : "?";
		}
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
		Log::INFO("TweenMenuFilter: registered; under TOOLS only the tools carried are listed, and SKILLS opens the skills sheet");
	}

	RE::BSEventNotifyControl TweenMenuFilter::ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
		RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
	{
		//Menu events arrive on worker threads: the movie, and the state its handler reads, are
		//touched on the main thread (Platform\MainThread.h). Posting both keeps an open and a close in order.
		if (a_event && a_event->menuName == RE::TweenMenu::MENU_NAME) {
			if (a_event->opening) {
				MainThread::Post([] { Prepare(false); });
			} else {
				MainThread::Post(Closed);
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	void TweenMenuFilter::Closed()
	{
		if (!std::exchange(s_skillsPicked, false)) {
			return;
		}
		Log::INFO("TweenMenuFilter: SKILLS picked; sending {} \"{}\"", kTabSwitchEvent, kSkillsTab);
		Papyrus::ModEvents::Send(kTabSwitchEvent, kSkillsTab);
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
		if (!a_tool.listEditorID) {
			return 0;
		}
		//A list that does not resolve is logged once by the form table; the tool is then left listed.
		auto* list = FormCache::Get<RE::BGSListForm>(a_tool.listEditorID, "TweenMenuFilter");
		auto  player = RE::PlayerCharacter::GetSingleton();
		if (!list || !player) {
			return -1;
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
		const auto view = Scaleform::ScaleformUtility::OpenMenuMovie(RE::TweenMenu::MENU_NAME);
		if (!view) {
			return;
		}
		auto* movie = view.get();
		s_skillsPicked = false;
		RE::GFxValue clip;
		RE::GFxValue columns;
		if (!movie->GetVariable(&clip, kMenuClip) || !clip.IsObject() || !clip.GetMember(kColumns, &columns) || !columns.IsArray()) {
			static bool s_warned = false;
			if (!std::exchange(s_warned, true)) {
				Log::WARN("TweenMenuFilter: {}.{} not found; this Tween menu movie is left as it is", kMenuClip, kColumns);
			}
			return;
		}
		int options = 0;
		int expanders = 0;
		std::string hidden;
		std::vector<std::uint32_t> changed;
		for (std::uint32_t c = kFirstColumn; c <= kLastColumn; ++c) {
			RE::GFxValue column;
			RE::GFxValue entries;
			if (!columns.GetElement(c, &column) || !column.IsObject() || !column.GetMember("entries", &entries) || !entries.IsArray()) {
				continue;
			}
			bool touched = false;
			for (std::uint32_t i = 0; i < entries.GetArraySize(); ++i) {
				++options;
				RE::GFxValue entry;
				RE::GFxValue children;
				if (!entries.GetElement(i, &entry) || !entry.IsObject() || !entry.GetMember("children", &children) || !children.IsArray()) {
					continue;
				}
				++expanders;
				for (std::uint32_t k = children.GetArraySize(); k > 0; --k) {
					RE::GFxValue child;
					RE::GFxValue hold;
					if (!children.GetElement(k - 1, &child)) {
						continue;
					}
					const Tool* tool = Find(EventOf(child, hold));  //null for an option that needs no tool
					if (!tool || Carried(*tool) != 0) {
						continue;
					}
					hidden += (hidden.empty() ? "" : ", ") + TextOf(child);
					//The array is the entry's own: edited in place, an empty one stays an array, so
					//the movie still draws the entry as an expander.
					children.RemoveElements(k - 1, 1);
					touched = true;
				}
			}
			if (touched) {
				//Nothing is folded out or selected under an expander the frame after the open.
				column.SetMember("expanded", RE::GFxValue(false));
				column.SetMember("subSel", RE::GFxValue(-1.0));
				changed.push_back(c);
			}
		}
		if (options == 0 && !a_retry) {
			//The movie has no data yet: once more at the start of the next frame, on the main thread.
			MainThread::Post([] { Prepare(true); });
			return;
		}
		for (const auto c : changed) {
			RE::GFxValue column;
			RE::GFxValue title;
			if (!columns.GetElement(c, &column) || !column.GetMember("title", &title) || !title.IsString()) {
				Log::WARN("TweenMenuFilter: column {} has no title; not redrawn", c);
				continue;
			}
			//The movie's own builder: it removes the column's clip and makes it again from entries.
			const std::array<RE::GFxValue, 2> args{ RE::GFxValue(static_cast<double>(c)), title };
			clip.Invoke("buildCategory", nullptr, args.data(), args.size());
		}
		if (!changed.empty()) {
			//A column the pointer was already over gets its highlighted look back.
			RE::GFxValue current;
			if (clip.GetMember("iCurrentState", &current) && current.IsNumber()) {
				clip.Invoke("refreshCategory", nullptr, &current, 1);
			}
			clip.Invoke("refreshArrows", nullptr, nullptr, 0);
		}
		WrapPick(movie, clip);
		Log::INFO("TweenMenuFilter: {} option(s), {} expander(s); hidden: {}{}", options, expanders,
			hidden.empty() ? "none" : hidden, a_retry ? " (deferred pass)" : "");
	}

	void TweenMenuFilter::WrapPick(RE::GFxMovie* a_movie, RE::GFxValue& a_clip)
	{
		//Once per open (the menu's movie is a fresh one each time): on the first pass, or on the deferred one.
		static RE::GPtr<PickHandler> pick = RE::make_gptr<PickHandler>();
		RE::GFxValue original;
		if (!a_clip.GetMember(kPick, &original) || original.IsUndefined()) {
			static bool s_warned = false;
			if (!std::exchange(s_warned, true)) {
				Log::WARN("TweenMenuFilter: {}.{} not found; SKILLS and an empty TOOLS are as the movie has them", kMenuClip, kPick);
			}
			return;
		}
		a_clip.SetMember(kMoviePick, original);
		RE::GFxValue fn;
		a_movie->CreateFunction(&fn, pick.get());
		a_clip.SetMember(kPick, fn);
	}

	void TweenMenuFilter::PickHandler::Call(Params& a_params)
	{
		if (!a_params.thisPtr) {
			return;
		}
		RE::GFxValue entry;
		RE::GFxValue hold;
		const char* event = a_params.argCount > 0 && a_params.args[0].IsObject() && a_params.args[0].GetMember("entry", &entry)
		                        ? EventOf(entry, hold)
		                        : nullptr;
		if (!event) {
			//The selected row of an empty fold: there is nothing to open, so the press folds it back in
			//(the movie's own toggle only does that while no row under the expander is selected).
			RE::GFxValue current;
			if (a_params.thisPtr->GetMember("iCurrentState", &current) && current.IsNumber()) {
				const std::array<RE::GFxValue, 2> fold{ current, RE::GFxValue(false) };
				a_params.thisPtr->Invoke("setExpanded", nullptr, fold.data(), fold.size());
			}
			return;
		}
		RE::GFxValue opening;
		if (a_params.thisPtr->GetMember("bOpeningMenu", &opening) && opening.IsBool() && opening.GetBool()) {
			return;  //a pick is already on its way out
		}
		if (std::strcmp(event, kSkillsEvent) == 0) {
			s_skillsPicked = true;
		}
		a_params.thisPtr->Invoke(kMoviePick, nullptr, a_params.args, a_params.argCount);
	}
}
