#include "LoadingScreenService.h"
#include "LoadingScreenContract.h"
#include "Papyrus/ModEvents.h"
#include "UIs/Scaleform/ScaleformUtility.h"

#include <array>
#include <string>
#include <string_view>

namespace LoadingScreen {

	namespace {
		//Bounds the parent walk: a cycle in the location chain would hang the load.
		constexpr std::size_t kParentChainMax = 8;

		//DirectInput scan codes, not ASCII.
		constexpr std::uint32_t kScanQ = 0x10;
		constexpr std::uint32_t kScanE = 0x12;

		RE::GPtr<RE::GFxMovieView> LoadingMovie() {
			return ScaleformUtility::OpenMenuMovie(RE::LoadingMenu::MENU_NAME);
		}

		RE::BGSLocation* DestinationLocation() {
			auto* ui = RE::UI::GetSingleton();
			if (!ui) {
				return nullptr;
			}
			auto menu = ui->GetMenu<RE::LoadingMenu>();
			if (!menu) {
				return nullptr;
			}
			auto* location = REBridge::LoadingMenuLocation(menu.get());
			//Checked before use: this slot was read by raw offset until CommonLib named it.
			if (!location || location->GetFormType() != RE::FormType::Location) {
				return nullptr;
			}
			return location;
		}

		//Leaf locations are often unnamed ("BrumaCastleHallWing02"), so walk up to one that has a name.
		std::string NameOf(RE::BGSLocation* location) {
			auto* current = location;
			for (std::size_t depth = 0; current && depth < kParentChainMax; ++depth) {
				const char* name = current->GetName();
				if (name && *name) {
					return name;
				}
				current = current->parentLoc;
			}
			return {};
		}
	}

	void LoadingScreenService::Register() {
		if (!s_modEventSinkAdded && ModEvents::AddSink(&s_sink)) {
			s_modEventSinkAdded = true;
			Log::INFO("LoadingScreenService: Mod-event sink registered.");
		}
		if (!s_menuSinkAdded) {
			if (auto* ui = RE::UI::GetSingleton()) {
				ui->AddEventSink<RE::MenuOpenCloseEvent>(&s_sink);
				s_menuSinkAdded = true;
				Log::INFO("LoadingScreenService: Menu sink registered.");
			}
		}
	}

	void LoadingScreenService::RegisterInput() {
		if (s_inputSinkAdded) {
			return;
		}
		if (auto* input = RE::BSInputDeviceManager::GetSingleton()) {
			input->AddEventSink<RE::InputEvent*>(&s_sink);
			s_inputSinkAdded = true;
			Log::INFO("LoadingScreenService: Input sink registered.");
		}
	}

	void LoadingScreenService::EndBootPhase() {
		if (s_bootDone) {
			return;
		}
		s_bootDone = true;
		Log::INFO("LoadingScreenService: Boot phase over. A game is starting without the title screen.");
		auto* ui = RE::UI::GetSingleton();
		PushChromeVisible(!(ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME)));
	}

	//The Loading Menu is kAlwaysOpen: at the title screen it sits behind the Main Menu with no tip and no destination.
	//
	//The movie-level SetVisible is what hides it. Once the Main Menu is on top, the paused Loading Menu is not advanced,
	//and GFx keeps drawing the frame captured at its last advance, so an ActionScript _visible change never reaches the screen.
	//The ActionScript call is kept so the movie's own state agrees when it does advance.
	void LoadingScreenService::PushChromeVisible(bool visible) {
		auto movie = LoadingMovie();
		if (!movie) {
			return;
		}
		movie->SetVisible(visible);

		RE::GFxValue arg{ visible };
		const bool delivered = movie->Invoke(LoadingScreenContract::AsPath::SetChromeVisible, nullptr, &arg, 1);
		Log::INFO("LoadingScreenService: Chrome visible={} (movie visible={}, ActionScript delivered={}).",
			visible, movie->GetVisible(), delivered);
	}

	//Diagnostic: the menu stack is the only thing that tells the title screen apart from a real load.
	void LoadingScreenService::LogOpenMenus() {
		auto* ui = RE::UI::GetSingleton();
		if (!ui) {
			return;
		}
		static constexpr std::array<std::string_view, 8> kProbedMenus{
			RE::MainMenu::MENU_NAME, RE::LoadingMenu::MENU_NAME, RE::FaderMenu::MENU_NAME, RE::HUDMenu::MENU_NAME,
			RE::Console::MENU_NAME, RE::JournalMenu::MENU_NAME, RE::CursorMenu::MENU_NAME, RE::TweenMenu::MENU_NAME
		};
		std::string openMenuNames;
		for (const auto& name : kProbedMenus) {
			if (ui->IsMenuOpen(name)) {
				if (!openMenuNames.empty()) {
					openMenuNames += ", ";
				}
				openMenuNames += name;
			}
		}
		Log::INFO("LoadingScreenService: Opened. Menus up: [{}].", openMenuNames);
	}

	void LoadingScreenService::PushStep(const char* asPath) {
		auto movie = LoadingMovie();
		if (!movie) {
			return;
		}
		movie->Invoke(asPath, nullptr, nullptr, 0);
		//Identifies which of the two input paths delivered a tip step.
		Log::INFO("LoadingScreenService: {} (native input).", asPath);
	}

	void LoadingScreenService::PushLocation() {
		auto movie = LoadingMovie();
		if (!movie) {
			Log::INFO("LoadingScreenService: Location requested, but the menu has closed.");
			return;
		}

		auto* location = DestinationLocation();
		const std::string name = location ? NameOf(location) : std::string{};

		RE::GFxValue arg{ name.c_str() };
		//False means Menu_mc was not there to take it, which looks identical on screen to having no location.
		const bool delivered = movie->Invoke(LoadingScreenContract::AsPath::SetLocationText, nullptr, &arg, 1);

		if (name.empty()) {
			Log::INFO("LoadingScreenService: No named destination location (LCTN {:08X}, delivered={}).",
				location ? location->GetFormID() : 0u, delivered);
		}
		else {
			Log::INFO("LoadingScreenService: Destination '{}' ({:08X}, delivered={}).", name, location->GetFormID(), delivered);
		}
	}

	void LoadingScreenService::PushGamepad() {
		auto movie = LoadingMovie();
		if (!movie) {
			return;
		}
		auto* input = RE::BSInputDeviceManager::GetSingleton();
		const bool gamepad = input && input->IsGamepadEnabled();

		RE::GFxValue arg{ gamepad };
		movie->Invoke(LoadingScreenContract::AsPath::SetGamepad, nullptr, &arg, 1);
		Log::INFO("LoadingScreenService: Gamepad={}.", gamepad);
	}

	RE::BSEventNotifyControl LoadingScreenService::Sink::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) {
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		//Handled directly: skse.SendModEvent dispatches on the thread running the movie,
		//and invoking from inside an ExternalInterface callback is the ordinary pattern.
		if (a_event->eventName == LoadingScreenContract::ModEvent::RequestLocation) {
			PushLocation();
		}
		else if (a_event->eventName == LoadingScreenContract::ModEvent::RequestPlatform) {
			PushGamepad();
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BSEventNotifyControl LoadingScreenService::Sink::ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
		RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (a_event->opening && a_event->menuName == RE::LoadingMenu::MENU_NAME) {
			//The earliest moment the destination is known. Menu_mc may not exist yet,
			//in which case the pushes do nothing and the SWF's own requests from FadeInMenu carry them.
			LogOpenMenus();
			PushLocation();
			PushGamepad();
			auto* ui = RE::UI::GetSingleton();
			PushChromeVisible(s_bootDone && !(ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME)));
		}
		else if (a_event->menuName == RE::MainMenu::MENU_NAME) {
			//The state comes from the event, not IsMenuOpen: the Main Menu can still report open on its closing event.
			//The title screen going away also ends the boot phase.
			if (!a_event->opening) {
				s_bootDone = true;
			}
			PushChromeVisible(!a_event->opening);
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	//Runs for every input event in the game, so the Loading Menu check runs first.
	RE::BSEventNotifyControl LoadingScreenService::Sink::ProcessEvent(RE::InputEvent* const* a_event,
		RE::BSTEventSource<RE::InputEvent*>*) {
		auto* ui = RE::UI::GetSingleton();
		if (!a_event || !ui || !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
			return RE::BSEventNotifyControl::kContinue;
		}

		for (auto* event = *a_event; event; event = event->next) {
			auto* button = event->AsButtonEvent();
			//The press edge only: a held key must not step the tips repeatedly.
			if (!button || !button->IsDown()) {
				continue;
			}

			const char* asPath = nullptr;
			const auto code = button->GetIDCode();
			switch (button->GetDevice()) {
			case RE::INPUT_DEVICE::kKeyboard:
				if (code == kScanE) {
					asPath = LoadingScreenContract::AsPath::NextTip;
				}
				else if (code == kScanQ) {
					asPath = LoadingScreenContract::AsPath::PreviousTip;
				}
				break;
			case RE::INPUT_DEVICE::kGamepad:
				if (code == static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Key::kRightShoulder)) {
					asPath = LoadingScreenContract::AsPath::NextTip;
				}
				else if (code == static_cast<std::uint32_t>(RE::BSWin32GamepadDevice::Key::kLeftShoulder)) {
					asPath = LoadingScreenContract::AsPath::PreviousTip;
				}
				break;
			default:
				break;
			}

			if (asPath) {
				PushStep(asPath);
			}
		}
		return RE::BSEventNotifyControl::kContinue;
	}

}
