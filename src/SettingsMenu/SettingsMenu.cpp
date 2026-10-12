#include "SettingsMenu/SettingsMenu.h"

#include "Hotkeys/HotkeyService.h"
#include "Platform/MainThread.h"
#include "SettingsMenu/SettingsStore.h"
#include "SettingsMenu/SoundService.h"
#include "UIs/Scaleform/ScaleformUtility.h"

// <Windows.h>, which this plugin's CommonLib build includes, maps PlaySound to PlaySoundA.
#ifdef PlaySound
#	undef PlaySound
#endif

namespace SKYBSettingsMenu
{
	namespace
	{
		const char* TypeName(Type a_type)
		{
			switch (a_type) {
			case Type::kToggle:
				return "toggle";
			case Type::kChoice:
				return "choice";
			default:
				return "number";
			}
		}

		RE::GPtr<RE::GFxMovieView> Movie()
		{
			auto ui = RE::UI::GetSingleton();
			auto menu = ui ? ui->GetMenu(SettingsMenu::MENU_NAME) : nullptr;
			return menu ? menu->uiMovie : nullptr;
		}

		std::string ArgString(const RE::FxDelegateArgs& a_params, std::uint32_t a_i)
		{
			return a_params.GetArgCount() > a_i && a_params[a_i].IsString() ? a_params[a_i].GetString() : "";
		}

		int ArgDir(const RE::FxDelegateArgs& a_params, std::uint32_t a_i)
		{
			return a_params.GetArgCount() > a_i && a_params[a_i].IsNumber() && a_params[a_i].GetNumber() < 0.0 ? -1 : 1;
		}
	}

	SettingsMenu::SettingsMenu()
	{
		auto scaleform = RE::BSScaleformManager::GetSingleton();
		const bool loaded = scaleform->LoadMovieEx(this, FILE_NAME, [this](RE::GFxMovieDef* a_def) {
			fxDelegate.reset(new RE::FxDelegate());
			fxDelegate->RegisterHandler(this);
			a_def->SetState(RE::GFxState::StateType::kExternalInterface, fxDelegate.get());
			fxDelegate->Release();
		});
		if (!loaded) {
			//Closed at once: a menu without its movie would sit invisible and modal.
			Log::ERROR("settings menu: Interface\\{}.swf failed to load - closing", FILE_NAME);
			if (auto queue = RE::UIMessageQueue::GetSingleton()) {
				queue->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
			}
		}

		// above the journal and the title screen, which stay open underneath
		depthPriority = 10;
		inputContext = Context::kMenuMode;
		menuFlags.set(RE::UI_MENU_FLAGS::kPausesGame, RE::UI_MENU_FLAGS::kDisablePauseMenu, RE::UI_MENU_FLAGS::kModal,
					  RE::UI_MENU_FLAGS::kUsesMenuContext, RE::UI_MENU_FLAGS::kTopmostRenderedMenu,
					  RE::UI_MENU_FLAGS::kRequiresUpdate, RE::UI_MENU_FLAGS::kUpdateUsesCursor);
		auto input = RE::BSInputDeviceManager::GetSingleton();
		if (!input || !input->IsGamepadEnabled()) {
			menuFlags.set(RE::UI_MENU_FLAGS::kUsesCursor);
		}
	}

	void SettingsMenu::Register()
	{
		if (auto ui = RE::UI::GetSingleton()) {
			ui->Register(MENU_NAME, Creator);
		}
		if (auto src = SKSE::GetModCallbackEventSource()) {
			src->AddEventSink(SettingsMenuOpener::GetSingleton());
		}
		// a finished capture re-pushes the menu (if it is still open)
		HotkeyService::GetSingleton()->SetOnChange([] { Push(); });
		Log::INFO("settings menu: registered as '{}', opened by the mod event {}", MENU_NAME, OPEN_EVENT);
	}

	void SettingsMenu::Open()
	{
		auto queue = RE::UIMessageQueue::GetSingleton();
		auto ui = RE::UI::GetSingleton();
		if (!queue || !ui || ui->IsMenuOpen(MENU_NAME)) {
			return;
		}
		if (!ScaleformUtility::MovieExists(FILE_NAME)) {
			Log::ERROR("settings menu: Interface\\{}.swf not found - not opened", FILE_NAME);
			return;
		}
		Log::INFO("settings menu: opening ({})", Store::GetSingleton()->EditingDefaults() ? "new-game defaults" : "this character");
		queue->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
	}

	void SettingsMenu::Close()
	{
		if (auto queue = RE::UIMessageQueue::GetSingleton()) {
			queue->AddMessage(MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
		}
	}

	void SettingsMenu::Accept(RE::FxDelegateHandler::CallbackProcessor* a_cbReg)
	{
		a_cbReg->Process("SKYBSettings_Ready", OnReady);
		a_cbReg->Process("SKYBSettings_Step", OnStep);
		a_cbReg->Process("SKYBSettings_Preset", OnPreset);
		a_cbReg->Process("SKYBSettings_Reset", OnReset);
		a_cbReg->Process("SKYBSettings_Capture", OnCapture);
		a_cbReg->Process("SKYBSettings_CaptureCancel", OnCaptureCancel);
		a_cbReg->Process("SKYBSettings_Close", OnClose);
		a_cbReg->Process("PlaySound", OnPlaySound);
	}

	// what Reset sets a row to: its Classic value, or for a volume the category's own default
	static float ResetValue(const RowDef& a_row)
	{
		if (a_row.applier == Applier::kSound) {
			if (const auto d = SoundService::GetSingleton()->Default(a_row.key)) {
				return Store::Snap(a_row, *d);
			}
		}
		return a_row.presets[0];
	}

	void SettingsMenu::Push()
	{
		auto movie = Movie();
		if (!movie) {
			return;
		}
		auto* store = Store::GetSingleton();

		RE::GFxValue header;
		movie->CreateObject(&header);
		header.SetMember("context", RE::GFxValue(store->EditingDefaults() ? "defaults" : "save"));
		auto input = RE::BSInputDeviceManager::GetSingleton();
		header.SetMember("gamepad", RE::GFxValue(input && input->IsGamepadEnabled()));
		RE::GFxValue presetLabels, presetDescs;
		movie->CreateArray(&presetLabels);
		movie->CreateArray(&presetDescs);
		for (std::size_t p = 0; p < kPresetCount; ++p) {
			presetLabels.PushBack(RE::GFxValue(kPresetLabels[p]));
			// the desc key is the label key + _desc (generated in pairs)
			presetDescs.PushBack(RE::GFxValue(std::string(kPresetLabels[p]) + "_desc"));
		}
		header.SetMember("presetLabels", presetLabels);
		header.SetMember("presetDescs", presetDescs);
		// the menu's own rebindable keys, and a one-shot capture message
		auto*        hotkeys = HotkeyService::GetSingleton();
		RE::GFxValue keys;
		hotkeys->BuildMap(movie.get(), keys);
		header.SetMember("keys", keys);
		const auto status = hotkeys->TakeStatus();
		header.SetMember("status", RE::GFxValue(status.key.c_str()));
		header.SetMember("statusArg", RE::GFxValue(status.arg.c_str()));
		header.SetMember("capturing", RE::GFxValue(hotkeys->CapturingId().c_str()));

		RE::GFxValue cats;
		movie->CreateArray(&cats);
		for (const auto& c : kCategories) {
			RE::GFxValue o;
			movie->CreateObject(&o);
			o.SetMember("id", RE::GFxValue(c.id));
			o.SetMember("label", RE::GFxValue(c.label));
			o.SetMember("empty", RE::GFxValue(c.empty));
			o.SetMember("preset", RE::GFxValue(static_cast<double>(store->PresetOf(c.id))));
			cats.PushBack(o);
		}

		RE::GFxValue rows;
		movie->CreateArray(&rows);
		for (const auto& r : kRows) {
			RE::GFxValue o;
			movie->CreateObject(&o);
			o.SetMember("id", RE::GFxValue(r.id));
			o.SetMember("category", RE::GFxValue(r.category));
			o.SetMember("group", RE::GFxValue(r.group));
			o.SetMember("type", RE::GFxValue(TypeName(r.type)));
			o.SetMember("label", RE::GFxValue(r.label));
			o.SetMember("desc", RE::GFxValue(std::string(r.label) + "_desc"));
			o.SetMember("value", RE::GFxValue(static_cast<double>(store->Get(r))));
			o.SetMember("classic", RE::GFxValue(static_cast<double>(ResetValue(r))));
			o.SetMember("min", RE::GFxValue(static_cast<double>(r.min)));
			o.SetMember("max", RE::GFxValue(static_cast<double>(r.max)));
			o.SetMember("step", RE::GFxValue(static_cast<double>(r.step)));
			o.SetMember("format", RE::GFxValue(r.format));
			RE::GFxValue choices;
			movie->CreateArray(&choices);
			if (r.type == Type::kToggle) {
				choices.PushBack(RE::GFxValue("$SKYB_settings_off"));
				choices.PushBack(RE::GFxValue("$SKYB_settings_on"));
			} else {
				for (std::uint8_t i = 0; i < r.choices; ++i) {
					choices.PushBack(RE::GFxValue(std::format("{}_c{}", r.label, i)));
				}
			}
			o.SetMember("choices", choices);
			o.SetMember("scope", RE::GFxValue(r.scope == Scope::kGlobal ? "global" : "save"));
			o.SetMember("applies", RE::GFxValue(r.applies));
			//the group's switch: at 0 the menu hides the rest of the group
			o.SetMember("gate", RE::GFxValue(r.gate));
			rows.PushBack(o);
		}

		// Hotkeys category: the shared actions ("All Menus") first, then each
		// menu's rows that still bind a device of their own
		for (const auto& h : kHotkeys) {
			if (!HotkeyService::Listed(h)) {
				continue;
			}
			const auto* group = HotkeyService::Group(h.group);
			const int   own = HotkeyService::OwnDevices(h);
			const auto  cur = hotkeys->Get(h);
			const HotkeyService::Binding def{ h.pc, h.pad };
			RE::GFxValue o;
			movie->CreateObject(&o);
			o.SetMember("id", RE::GFxValue(h.id));
			o.SetMember("category", RE::GFxValue("Hotkeys"));
			o.SetMember("group", RE::GFxValue(group ? group->label : ""));
			o.SetMember("type", RE::GFxValue("key"));
			o.SetMember("label", RE::GFxValue(h.label));
			o.SetMember("desc", RE::GFxValue(std::string(h.label) + "_desc"));
			o.SetMember("text", RE::GFxValue(HotkeyService::Text(h, cur).c_str()));
			o.SetMember("defaultText", RE::GFxValue(HotkeyService::Text(h, def).c_str()));
			o.SetMember("changed", RE::GFxValue(((own & 1) && cur.pc != def.pc) || ((own & 2) && cur.pad != def.pad)));
			o.SetMember("scope", RE::GFxValue("global"));
			o.SetMember("applies", RE::GFxValue(HotkeyService::IsGame(h) ? "keyworld" : "key"));
			rows.PushBack(o);
		}

		RE::GFxValue args[3]{ header, cats, rows };
		if (!movie->Invoke("_root.optionsSheet.SetSettingsData", nullptr, args, 3)) {
			Log::WARN("settings menu: SetSettingsData was not delivered");
		}
	}

	// Every callback is deferred to the main thread's next frame (MainThread::Post), after
	// the movie's script has returned and on the thread that advances the movie. Values,
	// the co-save map and the setting overrides are only touched there.
	void SettingsMenu::OnReady(const RE::FxDelegateArgs&)
	{
		MainThread::Post([] { Push(); });
	}

	void SettingsMenu::OnStep(const RE::FxDelegateArgs& a_params)
	{
		const auto id = ArgString(a_params, 0);
		const int  dir = ArgDir(a_params, 1);
		MainThread::Post([id, dir] {
			if (const auto* row = Store::FindRow(id)) {
				const float before = Store::GetSingleton()->Get(*row);
				Store::GetSingleton()->Step(*row, dir);
				if (Store::GetSingleton()->Get(*row) != before) {
					RE::PlaySound("UIMenuFocus");
				}
				Push();
			}
		});
	}

	void SettingsMenu::OnPreset(const RE::FxDelegateArgs& a_params)
	{
		const auto cat = ArgString(a_params, 0);
		const int  dir = ArgDir(a_params, 1);
		MainThread::Post([cat, dir] {
			auto*     store = Store::GetSingleton();
			const int cur = store->PresetOf(cat);
			if (cur == Store::kNoPresets) {
				return;
			}
			const int n = static_cast<int>(kPresetCount);
			// from Custom, Right lands on the first preset and Left on the last
			const int next = cur == Store::kCustom ? (dir > 0 ? 0 : n - 1) : ((cur + dir) % n + n) % n;
			store->ApplyPreset(cat, static_cast<std::size_t>(next));
			RE::PlaySound("UIMenuFocus");
			Push();
		});
	}

	void SettingsMenu::OnReset(const RE::FxDelegateArgs& a_params)
	{
		const auto id = ArgString(a_params, 0);
		MainThread::Post([id] {
			auto* store = Store::GetSingleton();
			if (const auto* row = Store::FindRow(id)) {
				store->Set(*row, ResetValue(*row));
			} else if (const auto* key = HotkeyService::Find(id)) {
				HotkeyService::GetSingleton()->Reset(*key);
			} else if (Store::FindCategory(id)) {
				store->ApplyPreset(id, 0);
			} else {
				return;
			}
			RE::PlaySound("UIMenuOK");
			Push();
		});
	}

	void SettingsMenu::OnCapture(const RE::FxDelegateArgs& a_params)
	{
		const auto id = ArgString(a_params, 0);
		// armed next frame: the press that asked for it is already behind us
		MainThread::Post([id] {
			HotkeyService::GetSingleton()->BeginCapture(id);
			Push();
		});
	}

	void SettingsMenu::OnCaptureCancel(const RE::FxDelegateArgs&)
	{
		MainThread::Post([] { HotkeyService::GetSingleton()->CancelCapture(); });
	}

	void SettingsMenu::OnClose(const RE::FxDelegateArgs&)
	{
		MainThread::Post([] { HotkeyService::GetSingleton()->CancelCapture(); });
		Close();
	}

	void SettingsMenu::OnPlaySound(const RE::FxDelegateArgs& a_params)
	{
		if (a_params.GetArgCount() >= 1 && a_params[0].IsString()) {
			RE::PlaySound(a_params[0].GetString());
		}
	}

	RE::BSEventNotifyControl SettingsMenuOpener::ProcessEvent(const SKSE::ModCallbackEvent* a_event,
															  RE::BSTEventSource<SKSE::ModCallbackEvent>*)
	{
		if (a_event && a_event->eventName == SettingsMenu::OPEN_EVENT) {
			SettingsMenu::Open();
		}
		return RE::BSEventNotifyControl::kContinue;
	}
}
