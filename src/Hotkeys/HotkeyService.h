#pragma once
#include <string>
#include <string_view>
#include <vector>
// Rebindable keys for Skyblivion's menus (settings menu > Hotkeys).
//
// Each action is a row of SKYBSettingsMenu\Hotkeys.csv, generated into
// SettingsMenuTable.h (kHotkeys, kHotkeyGroups, kKeyNames). A binding is one
// keyboard key and one gamepad button, as the Flash key codes menus see
// (gamepad buttons are 96..107). Stored for all characters in
// SKYBSettings.ini [Hotkeys] <id>=<pc>,<pad> (-1 = none, absent = default).
//
// A menu asks with skse.SendModEvent("SKYBRequestHotkeys", "<IMenu name>|<AS function path>");
// next frame <path>(map) is called in that menu's movie, with
// map = {<action id>: {pc, pad, pcGlyph, padGlyph, ps3Glyph}} (glyphs are
// SKYBLibInputGlyphs export names, "" = none). Menus keep their built-in keys
// until the answer arrives.
//
// In-world keys (groups with game = true: QuickLoot, moreHUD) use DirectInput
// scan codes and SKSE gamepad codes (kGameKeyNames). Every change and every
// game load (ApplyGameKeys) writes them to the action's targets through
// InterfaceAdapters ("quickloot:<property>" / "global:<editor id>"). An action
// may take only a key or only a button (HotkeyDef::devices).
//
// Capture: BeginCapture(id) takes the next key or button press, ignoring the
// press that started it. Esc cancels, the mouse is ignored, numpad keys and the
// d-pad cannot be bound (they collide with the menus' own codes). A key the menu
// reserves, or one used by a linked group (on screen at the same time) or a
// shared action, is refused; one used by another action of the same menu is swapped.
//
// Shared actions (group "shared"): one binding that several menus' actions
// follow (Hotkeys.csv `shares`, optionally for one device). Menus still receive
// their own ids (Resolve). The Hotkeys tab lists the shared actions plus rows
// that still bind a device of their own (Listed). A capture for a shared action
// checks every member's menu.
#include <chrono>
#include <functional>
#include <span>

#include "SettingsMenu/SettingsMenuTable.h"

class HotkeyService :
	public RE::BSTEventSink<SKSE::ModCallbackEvent>,
	public RE::BSTEventSink<RE::InputEvent*> {
public:
	struct Binding {
		int pc = -1;
		int pad = -1;
	};
	struct Status {
		std::string key;  // string key, "" = none
		std::string arg;  // {0}; a string key is translated by the menu
	};

	static HotkeyService* GetSingleton();
	void Register();  // kDataLoaded: both sinks

	[[nodiscard]] static const SKYBSettingsMenu::HotkeyDef*      Find(std::string_view a_id);
	[[nodiscard]] static const SKYBSettingsMenu::HotkeyGroupDef* Group(std::string_view a_id);
	[[nodiscard]] static const SKYBSettingsMenu::KeyName*        Name(int a_code, bool a_game = false);
	[[nodiscard]] static bool                                    IsGame(const SKYBSettingsMenu::HotkeyDef& a_def);
	[[nodiscard]] static bool IsShared(const SKYBSettingsMenu::HotkeyDef& a_def);  // a shared action itself
	// devices this row binds itself (1 keyboard, 2 gamepad; 0 = it only follows a shared action)
	[[nodiscard]] static int  OwnDevices(const SKYBSettingsMenu::HotkeyDef& a_def);
	[[nodiscard]] static bool Listed(const SKYBSettingsMenu::HotkeyDef& a_def) { return OwnDevices(a_def) != 0; }
	[[nodiscard]] static std::string Text(const SKYBSettingsMenu::HotkeyDef& a_def, Binding a_binding);  // "R  /  Y"

	[[nodiscard]] Binding Get(const SKYBSettingsMenu::HotkeyDef& a_def) const;      // this row's own (stored) binding
	[[nodiscard]] Binding Resolve(const SKYBSettingsMenu::HotkeyDef& a_def) const;  // what its menu receives
	void                  Reset(const SKYBSettingsMenu::HotkeyDef& a_def);
	void                  ApplyGameKeys();  // kPostLoadGame / kNewGame: every in-world binding to its targets

	void                      BeginCapture(std::string_view a_id);
	void                      CancelCapture();
	[[nodiscard]] std::string CapturingId() const;
	[[nodiscard]] Status      TakeStatus();  // one-shot message for the settings menu

	// called after a capture ends or a binding changes (the settings menu re-pushes)
	void SetOnChange(std::function<void()> a_fn) { _onChange = std::move(a_fn); }

	// {<id>: {pc, pad, pcGlyph, padGlyph, ps3Glyph}} in a_movie
	void BuildMap(RE::GFxMovieView* a_movie, RE::GFxValue& a_out) const;

	RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
										  RE::BSTEventSource<SKSE::ModCallbackEvent>*) override;
	RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
										  RE::BSTEventSource<RE::InputEvent*>*) override;

	static constexpr std::string_view kSection = "Hotkeys";
	static constexpr std::string_view kRequestEvent = "SKYBRequestHotkeys";
	static constexpr std::string_view kSharedGroup = "shared";

private:
	// a rebind: refused (status set), or the rows that give up the key
	struct Plan {
		Status                                        refuse;
		std::vector<const SKYBSettingsMenu::HotkeyDef*> swaps;
	};

	HotkeyService() = default;
	void Store(const SKYBSettingsMenu::HotkeyDef& a_def, Binding a_binding);
	[[nodiscard]] Plan PlanAssign(const SKYBSettingsMenu::HotkeyDef& a_def, bool a_pad, int a_code, bool a_reset) const;
	void Assign(const SKYBSettingsMenu::HotkeyDef& a_def, bool a_pad, int a_code, const Plan& a_plan);
	void EndCapture(Status a_status);
	void ApplyGame(const SKYBSettingsMenu::HotkeyDef& a_def, Binding a_binding);
	void DropSharedMemberEntries();  // bindings stored before an action became shared
	// the menu groups that receive a_def's binding on that device
	[[nodiscard]] static std::vector<const SKYBSettingsMenu::HotkeyGroupDef*> Groups(const SKYBSettingsMenu::HotkeyDef& a_def,
																					bool a_pad);
	// the row that decides a_def's binding on that device (its shared action, or itself)
	[[nodiscard]] static const SKYBSettingsMenu::HotkeyDef* Owner(const SKYBSettingsMenu::HotkeyDef& a_def, bool a_pad);

	std::string                           _capturing;  // action id, "" = not capturing
	std::chrono::steady_clock::time_point _armedAt{};
	Status                                _status;
	std::function<void()>                 _onChange;
};
