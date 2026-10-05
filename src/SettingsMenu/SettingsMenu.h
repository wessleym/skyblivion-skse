#pragma once
// Moved from SKYBUIDemo.dll into Skyblivion.dll, 2026-10-05 (fold-in, UI integration workspace
// ledger L-0056). Source of record until then: game\native\SKYBUIDemo\src in that workspace.
#include <string>
#include <string_view>
// The Skyblivion settings menu: a native IMenu around
// Interface\SKYBSettingsMenu.swf (built by SKYBSettingsMenu\Menu\build.sh).
// The values and presets are SettingsStore's; this class only shows them.
//
// Opening: the mod event SKYBOpenSettingsMenu, sent by the title screen's
// SETTINGS button (startmenu.swf) and the journal System tab's SKYBLIVION
// SETTINGS entry (quest_journal.swf), or from any script. It opens over
// whatever is up and closes back to it.
//
// Contract with the movie (gfx.io.GameDelegate / Invoke):
//   movie -> DLL   SKYBSettings_Ready()                  movie is up, send data
//                  SKYBSettings_Step(id, dir)            dir -1 / +1: previous / next value
//                  SKYBSettings_Preset(category, dir)    cycle the category's preset
//                  SKYBSettings_Reset(id)                back to Classic (a category id resets it to Classic;
//                                                        a hotkey id back to its default keys)
//                  SKYBSettings_Capture(id)              wait for the next key / button for hotkey id
//                  SKYBSettings_CaptureCancel()
//                  SKYBSettings_Close()
//                  PlaySound(editorID)
//   DLL -> movie   _root.Menu_mc.SetSettingsData(header, categories, rows)
//     header      {context: "save" | "defaults", gamepad, presetLabels[], presetDescs[],
//                  keys (HotkeyService map), status, statusArg, capturing (hotkey id or "")}
//     categories  [{id, label, empty, preset}]   preset = index, -1 Custom, -2 none
//     rows        [{id, category, group, type ("toggle" | "number" | "choice"),
//                   label, desc, value, classic, min, max, step, format,
//                   choices[], scope ("save" | "global"), applies}]
//                 + Hotkeys rows: {id, category "Hotkeys", group, type "key", label, desc,
//                   text, defaultText, changed, scope "global", applies "key"}
//   Every change re-sends the whole set; the movie keeps its place by id.
namespace SKYBSettingsMenu
{
	class SettingsMenu : public RE::IMenu
	{
	public:
		static constexpr const char*      MENU_NAME = "SKYBSettingsMenu";
		static constexpr std::string_view FILE_NAME = "Skyblivion/SKYBOptionsSheet";
		static constexpr std::string_view OPEN_EVENT = "SKYBOpenSettingsMenu";

		SettingsMenu();

		static void Register();  // kDataLoaded: the menu + the opener sink
		static void Open();
		static void Close();

		void Accept(RE::FxDelegateHandler::CallbackProcessor* a_cbReg) override;

		static RE::stl::owner<RE::IMenu*> Creator() { return new SettingsMenu(); }

	private:
		static void Push();

		static void OnReady(const RE::FxDelegateArgs& a_params);
		static void OnStep(const RE::FxDelegateArgs& a_params);
		static void OnPreset(const RE::FxDelegateArgs& a_params);
		static void OnReset(const RE::FxDelegateArgs& a_params);
		static void OnCapture(const RE::FxDelegateArgs& a_params);
		static void OnCaptureCancel(const RE::FxDelegateArgs& a_params);
		static void OnClose(const RE::FxDelegateArgs& a_params);
		static void OnPlaySound(const RE::FxDelegateArgs& a_params);
	};

	class SettingsMenuOpener : public RE::BSTEventSink<SKSE::ModCallbackEvent>
	{
	public:
		static SettingsMenuOpener* GetSingleton()
		{
			static SettingsMenuOpener singleton;
			return &singleton;
		}
		RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
											  RE::BSTEventSource<SKSE::ModCallbackEvent>*) override;
	};
}
