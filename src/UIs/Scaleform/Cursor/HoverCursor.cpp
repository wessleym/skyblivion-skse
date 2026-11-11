#include "HoverCursor.h"

#include "Platform/IniFile.h"
#include "Platform/MainThread.h"
#include "Platform/Text.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace HoverCursor {

	namespace {
		constexpr const char* kRegistrationName = "SKYBHoverCursor";
		constexpr std::string_view kCursorMovieFile = "cursormenu.swf";
		//Relative to the cursor menu's own file (Data\Interface\cursormenu.swf).
		constexpr const char* kHandUrl = "Skyblivion/SKYBCursorHand.swf";
		constexpr const char* kIniPath = "Data/SKSE/Plugins/SKYBHoverCursor.ini";

		//The clip cursormenu.swf moves to the mouse (the vanilla file's name, kept by ours).
		constexpr const char* kCursorClip = "mc_Cursor";
		//Ours in the cursor movie: the hand's holder, and the arrow's name while the hand stands in for it.
		constexpr const char* kHandClip = "skybHoverHand";
		constexpr const char* kArrowAside = "skybHoverArrow";
		constexpr const char* kHandArt = "art";
		constexpr std::int32_t kHandDepth = 1000;

		//A reading older than this, next to the newest, is from a movie the mouse no longer reaches.
		constexpr std::int64_t kFreshMs = 250;
		constexpr int kMaxClimb = 64;

		constexpr std::array kButtonEvents{
			"onPress", "onRelease", "onReleaseOutside", "onRollOver", "onRollOut", "onDragOver", "onDragOut"
		};

		struct Config {
			bool enabled = true;
			std::vector<std::string> excluded;  //menu names, lower case
		};
		Config g_config;

		//One movie's last look under the pointer. over: something of the movie is there; hand: it is clickable.
		//forced: the movie said so itself (skse.plugins.SKYBHoverCursor.hover(true)), for a menu that finds
		//its targets with its own hit tests instead of button handlers; it counts as over and clickable
		//until the movie calls hover(false).
		struct Reading {
			bool over = false;
			bool hand = false;
			bool forced = false;
			std::int64_t at = 0;

			bool Over() const { return over || forced; }
			bool Hand() const { return hand || forced; }
		};

		std::mutex g_lock;
		std::unordered_map<const RE::GFxMovie*, Reading> g_readings;
		bool g_updateQueued = false;
		std::atomic<bool> g_cursorOpen{ false };

		std::int64_t NowMs() {
			using namespace std::chrono;
			return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
		}

		void ReadConfig() {
			//Without the file: on, nothing excluded.
			IniFile::Read(kIniPath, "HoverCursor", [](const IniFile::Entry& a_entry) {
				if (!Text::IEquals(a_entry.section, "HoverCursor")) {
					return;
				}
				const auto key = Text::Lower(a_entry.key);
				if (key == "benabled") {
					g_config.enabled = Text::ParseBool(a_entry.value, true);
				} else if (key == "sexcludemenus") {
					for (const auto& name : Text::Split(a_entry.value, ',')) {
						g_config.excluded.push_back(Text::Lower(name));
					}
				}
			});
		}

		//------------------------------------------------------------------------------------------ reading a movie

		bool Defined(const RE::GFxValue& a_value) {
			return !a_value.IsUndefined() && !a_value.IsNull();
		}

		bool Has(const RE::GFxValue& a_object, const char* a_name) {
			RE::GFxValue v;
			return a_object.GetMember(a_name, &v) && Defined(v);
		}

		bool IsBool(const RE::GFxValue& a_object, const char* a_name, bool a_which) {
			RE::GFxValue v;
			return a_object.GetMember(a_name, &v) && v.IsBool() && v.GetBool() == a_which;
		}

		//A Button symbol: no timeline (movie clips have _currentframe) and no text (text fields have htmlText).
		bool IsButtonSymbol(const RE::GFxValue& a_entity) {
			return !Has(a_entity, "_currentframe") && !Has(a_entity, "htmlText");
		}

		bool TakesButtonEvents(const RE::GFxValue& a_entity) {
			for (const auto* name : kButtonEvents) {
				if (Has(a_entity, name)) {
					return true;
				}
			}
			return IsButtonSymbol(a_entity);
		}

		bool Clickable(const RE::GFxValue& a_entity) {
			const bool acts = Has(a_entity, "onPress") || Has(a_entity, "onRelease") || IsButtonSymbol(a_entity);
			return acts && !IsBool(a_entity, "enabled", false) && !IsBool(a_entity, "disabled", true) &&
			       !IsBool(a_entity, "useHandCursor", false);
		}

		//The movie's Mouse object, as _global.Mouse or Mouse.
		bool FindMouse(RE::GFxMovie* a_movie, RE::GFxValue& a_mouse) {
			return (a_movie->GetVariable(&a_mouse, "_global.Mouse") && a_mouse.IsObject()) ||
			       (a_movie->GetVariable(&a_mouse, "Mouse") && a_mouse.IsObject());
		}

		Reading Read(RE::GFxMovie* a_movie) {
			Reading r;
			r.at = NowMs();
			RE::GFxValue mouse;
			if (!FindMouse(a_movie, mouse)) {
				return r;
			}
			RE::GFxValue top;
			if (!mouse.Invoke("getTopMostEntity", &top) || !top.IsDisplayObject()) {
				return r;  //nothing of this movie under the pointer
			}
			r.over = true;
			//With testAll false GFx skips entities that take no mouse events, as its own roll-over does
			//(a plain overlay above a button does not hide the button). Where that form is not
			//understood the result is the same entity as above, or nothing; the climb covers both.
			RE::GFxValue start = top;
			RE::GFxValue testAll(false);
			RE::GFxValue target;
			if (mouse.Invoke("getTopMostEntity", &target, &testAll, 1) && target.IsDisplayObject()) {
				start = target;
			}
			RE::GFxValue e = start;
			for (int i = 0; i < kMaxClimb && e.IsDisplayObject(); ++i) {
				if (TakesButtonEvents(e)) {
					r.hand = Clickable(e);
					break;
				}
				RE::GFxValue parent;
				if (!e.GetMember("_parent", &parent)) {
					break;
				}
				e = parent;
			}
			return r;
		}

		//------------------------------------------------------------------------------------------ the cursor movie

		void SetFrom(RE::GFxValue& a_to, const RE::GFxValue& a_from, const char* a_name) {
			RE::GFxValue v;
			if (a_from.GetMember(a_name, &v) && Defined(v)) {
				a_to.SetMember(a_name, v);
			}
		}

		bool HandLoaded(const RE::GFxValue& a_hand) {
			RE::GFxValue art;
			RE::GFxValue width;
			return a_hand.GetMember(kHandArt, &art) && art.IsDisplayObject() && art.GetMember("_width", &width) &&
			       width.IsNumber() && width.GetNumber() > 0.0;
		}

		//Main thread. Shows the hand (a_hand) or the arrow. Nothing happens in a cursor movie we have not
		//put the hand into (another mod's cursormenu.swf without mc_Cursor) or before the hand has loaded.
		void Apply(RE::GFxMovieView* a_cursor, bool a_hand) {
			if (!a_cursor) {
				return;
			}
			RE::GFxValue root;
			if (!a_cursor->GetVariable(&root, "_root") || !root.IsDisplayObject()) {
				return;
			}
			RE::GFxValue aside;
			const bool showing = root.GetMember(kArrowAside, &aside) && aside.IsDisplayObject();
			if (a_hand == showing) {
				return;
			}
			if (a_hand) {
				RE::GFxValue hand;
				RE::GFxValue arrow;
				if (!root.GetMember(kHandClip, &hand) || !hand.IsDisplayObject() || !HandLoaded(hand) ||
					!root.GetMember(kCursorClip, &arrow) || !arrow.IsDisplayObject()) {
					return;
				}
				SetFrom(hand, arrow, "_x");
				SetFrom(hand, arrow, "_y");
				SetFrom(hand, arrow, "_visible");
				arrow.SetMember("_visible", RE::GFxValue(false));
				arrow.SetMember("_name", RE::GFxValue(kArrowAside));
				hand.SetMember("_name", RE::GFxValue(kCursorClip));
			} else {
				RE::GFxValue hand;
				if (!root.GetMember(kCursorClip, &hand) || !hand.IsDisplayObject()) {
					return;
				}
				SetFrom(aside, hand, "_x");
				SetFrom(aside, hand, "_y");
				SetFrom(aside, hand, "_visible");
				hand.SetMember("_visible", RE::GFxValue(false));
				hand.SetMember("_name", RE::GFxValue(kHandClip));
				aside.SetMember("_name", RE::GFxValue(kCursorClip));
			}
		}

		//Main thread, the frame after the cursor movie loaded: the hand beside the arrow, hidden.
		void PutHandIn(RE::GFxMovieView* a_cursor) {
			RE::GFxValue root;
			if (!a_cursor->GetVariable(&root, "_root") || !root.IsDisplayObject()) {
				return;
			}
			RE::GFxValue arrow;
			if (!root.GetMember(kCursorClip, &arrow) || !arrow.IsDisplayObject()) {
				Log::WARN("HoverCursor: the cursor menu has no {}; the hand stays off.", kCursorClip);
				return;
			}
			RE::GFxValue hand;
			if (root.GetMember(kHandClip, &hand) && hand.IsDisplayObject()) {
				return;
			}
			RE::GFxValue art;
			if (!root.CreateEmptyMovieClip(&hand, kHandClip, kHandDepth) || !hand.IsDisplayObject() ||
				!hand.CreateEmptyMovieClip(&art, kHandArt, 1) || !art.IsDisplayObject()) {
				Log::WARN("HoverCursor: could not add the hand to the cursor menu.");
				return;
			}
			hand.SetMember("_visible", RE::GFxValue(false));
			RE::GFxValue url(kHandUrl);
			art.Invoke("loadMovie", nullptr, &url, 1);
		}

		//Main thread: the topmost menu with something under the pointer decides.
		void Update() {
			std::unordered_map<const RE::GFxMovie*, Reading> readings;
			{
				std::scoped_lock lock(g_lock);
				g_updateQueued = false;
				readings = g_readings;
			}
			auto* ui = RE::UI::GetSingleton();
			if (!ui) {
				return;
			}
			const auto cursor = ui->GetMovieView(RE::CursorMenu::MENU_NAME);
			std::int64_t newest = 0;
			for (const auto& [movie, reading] : readings) {
				newest = (std::max)(newest, reading.at);
			}
			bool hand = false;
			for (std::uint32_t i = ui->menuStack.size(); i-- > 0;) {
				const auto& menu = ui->menuStack[i];
				if (!menu || !menu->uiMovie || menu->uiMovie.get() == cursor.get()) {
					continue;
				}
				const auto found = readings.find(menu->uiMovie.get());
				if (found == readings.end() || newest - found->second.at > kFreshMs || !found->second.Over()) {
					continue;
				}
				hand = found->second.Hand();
				if (hand && !g_config.excluded.empty()) {
					for (const auto& [name, entry] : ui->menuMap) {
						if (entry.menu.get() == menu.get()) {
							const auto lower = Text::Lower(name.c_str());
							hand = std::find(g_config.excluded.begin(), g_config.excluded.end(), lower) == g_config.excluded.end();
							break;
						}
					}
				}
				break;
			}
			Apply(cursor.get(), hand);
		}

		void QueueUpdate() {
			if (!g_updateQueued) {
				g_updateQueued = true;
				MainThread::Post(Update);
			}
		}

		void Note(const RE::GFxMovie* a_movie, const Reading& a_reading) {
			std::scoped_lock lock(g_lock);
			auto& last = g_readings[a_movie];
			const bool changed = last.at == 0 || last.over != a_reading.over || last.hand != a_reading.hand;
			const bool forced = last.forced;
			last = a_reading;
			last.forced = forced;
			if (changed) {
				QueueUpdate();
			}
		}

		void Force(const RE::GFxMovie* a_movie, bool a_on) {
			std::scoped_lock lock(g_lock);
			auto& last = g_readings[a_movie];
			const bool changed = last.at == 0 || last.forced != a_on;
			last.forced = a_on;
			last.at = NowMs();
			if (changed) {
				QueueUpdate();
			}
		}

		//skse.plugins.SKYBHoverCursor.hover(on): a menu with its own hit tests says whether the pointer is on
		//one of its targets. hover(false) hands the decision back to the general rule.
		class HoverHandler : public RE::GFxFunctionHandler {
		public:
			void Call(Params& a_params) override {
				if (!a_params.movie) {
					return;
				}
				bool on = false;
				if (a_params.argCount > 0) {
					const auto& a = a_params.args[0];
					on = a.IsBool() ? a.GetBool() : (a.IsNumber() && a.GetNumber() != 0.0);
				}
				Force(a_params.movie, on);
			}
		};

		//onMouseMove reads at once; a click or the wheel can change what is under a still pointer, so
		//those read again at the start of the next frame, after the menu has acted on them.
		class ProbeHandler : public RE::GFxFunctionHandler {
		public:
			explicit ProbeHandler(bool a_later) :
				later(a_later) {}

			void Call(Params& a_params) override {
				if (!a_params.movie || !g_cursorOpen.load(std::memory_order_relaxed)) {
					return;
				}
				if (!later) {
					Note(a_params.movie, Read(a_params.movie));
					return;
				}
				RE::GPtr<RE::GFxMovie> movie(a_params.movie);
				MainThread::Post([movie] { Note(movie.get(), Read(movie.get())); });
			}

		private:
			bool later;
		};

		bool OnMovieLoad(RE::GFxMovieView* a_view, RE::GFxValue* a_slot) {
			if (!a_view || !g_config.enabled) {
				return true;
			}
			auto* def = a_view->GetMovieDef();
			const char* url = def ? def->GetFileURL() : nullptr;
			if (url && Text::IEndsWith(url, kCursorMovieFile)) {
				RE::GPtr<RE::GFxMovieView> cursor(a_view);
				MainThread::Post([cursor] { PutHandIn(cursor.get()); });
				return true;
			}
			static RE::GPtr<ProbeHandler> now = RE::make_gptr<ProbeHandler>(false);
			static RE::GPtr<ProbeHandler> later = RE::make_gptr<ProbeHandler>(true);
			static RE::GPtr<HoverHandler> hover = RE::make_gptr<HoverHandler>();
			if (a_slot) {
				RE::GFxValue hoverFn;
				a_view->CreateFunction(&hoverFn, hover.get());
				a_slot->SetMember("hover", hoverFn);
			}
			RE::GFxValue mouse;
			if (!FindMouse(a_view, mouse)) {
				return true;
			}
			RE::GFxValue listener;
			RE::GFxValue nowFn;
			RE::GFxValue laterFn;
			a_view->CreateObject(&listener);
			a_view->CreateFunction(&nowFn, now.get());
			a_view->CreateFunction(&laterFn, later.get());
			listener.SetMember("onMouseMove", nowFn);
			listener.SetMember("onMouseDown", laterFn);
			listener.SetMember("onMouseUp", laterFn);
			listener.SetMember("onMouseWheel", laterFn);
			mouse.Invoke("addListener", nullptr, &listener, 1);
			if (a_slot) {
				a_slot->SetMember("listener", listener);
			}
			return true;
		}

		class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
				RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (a_event->menuName == RE::CursorMenu::MENU_NAME) {
					g_cursorOpen.store(a_event->opening, std::memory_order_relaxed);
				}
				//A menu came or went: what is under the pointer is unknown until it moves again.
				{
					std::scoped_lock lock(g_lock);
					g_readings.clear();
				}
				MainThread::Post([] {
					if (auto* ui = RE::UI::GetSingleton()) {
						Apply(ui->GetMovieView(RE::CursorMenu::MENU_NAME).get(), false);
					}
				});
				return RE::BSEventNotifyControl::kContinue;
			}
		};
		MenuSink g_menuSink;
	}

	void InstallScaleform() {
		ReadConfig();
		if (!g_config.enabled) {
			Log::INFO("HoverCursor: off (SKYBHoverCursor.ini bEnabled=0).");
			return;
		}
		auto* scaleform = SKSE::GetScaleformInterface();
		if (!scaleform || !scaleform->Register(OnMovieLoad, kRegistrationName)) {
			Log::WARN("HoverCursor: Scaleform registration refused; the cursor keeps its arrow.");
			return;
		}
		Log::INFO("HoverCursor: registered ({} menu(s) excluded).", g_config.excluded.size());
	}

	void Register() {
		if (!g_config.enabled) {
			return;
		}
		auto* ui = RE::UI::GetSingleton();
		if (!ui) {
			Log::WARN("HoverCursor: no UI; the hand will not show.");
			return;
		}
		ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_menuSink);
		g_cursorOpen.store(ui->IsMenuOpen(RE::CursorMenu::MENU_NAME), std::memory_order_relaxed);
	}
}
