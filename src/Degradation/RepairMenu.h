#pragma once
// Moved from SKYBDegradation.dll into Skyblivion.dll, 2026-10-05 (fold-in, UI integration workspace
// ledger L-0058). Source of record until then: game\native\SKYBDegradation\src in that workspace.
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The repair menu: a native IMenu around Interface\SKYBRepairMenu.swf.
//
// Contract with the movie (all through gfx.io.GameDelegate / Invoke):
//   movie -> DLL   SKYBRepair_Ready()                 movie is up, send data
//                  SKYBRepair_Strike(row, toLimit, generation)  one strike, or strike until cap / out of
//                                     hammers; generation = header.generation of the table the row index is from
//                  SKYBRepair_Hammer(dir)             switch to the next / previous carried hammer
//                  SKYBRepair_Close()                 close the menu
//                  PlaySound(editorID)
//                  SKYBRequestHotkeys("<menu>|<path>")  re-sent as the SKSE mod event of the
//                                     same name, which the settings menu's HotkeyService answers
//                                     (a movie our IMenu loads has no skse object)
//   DLL -> movie   _root.gearService.SetRepairData(header, rows, selectRow)
//                  _root.gearService.SetStatus(lines)     lines = [{key, args:[...]}]
//
// Opening: InventoryHook (repair tool activated), the mod event
// SKYBDegOpenRepairMenu from any script, or - VENDOR MODE - the native
// SKYBDegradation.OpenVendorRepair(Actor) the dialogue Repair button calls:
// the vendor's Smithing sets the limit, hammers are unlimited, the player
// pays gold (Condition::VendorCost) to 100% or to the vendor's limit.
// RECHARGE MODE (SKYBDegradation.OpenVendorRecharge, the Recharge button):
// the same menu lists every charged weapon / staff; Recharge (E) fills the
// selected one, Recharge All (R) every one; price = missing charge
// (Condition::RechargeCost). Opening from the inventory closes
// it first (no inventory mutation happens under a live item list) and, with
// ReturnToInventory (INI) = 1, reopens it when the repair menu closes.
namespace SKYBDeg
{
	class RepairMenu : public RE::IMenu
	{
	public:
		static constexpr const char*      MENU_NAME = "SKYBRepairMenu";
		static constexpr std::string_view FILE_NAME = "Skyblivion/SKYBGearService";

		RepairMenu();

		static void Register();
		static void Open(bool a_fromInventory, const RE::TESForm* a_tool);  // a_tool = hammer to use (or null)
		static void OpenVendor(RE::Actor* a_vendor);    // vendor repair service (dialogue)
		static void OpenRecharge(RE::Actor* a_vendor);  // vendor recharge service (dialogue)
		static void Close();
		static bool RegisterPapyrus(RE::BSScript::IVirtualMachine* a_vm);

		void                   Accept(RE::FxDelegateHandler::CallbackProcessor* a_cbReg) override;
		RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override;

		static RE::stl::owner<RE::IMenu*> Creator() { return new RepairMenu(); }

	private:
		static void OnReady(const RE::FxDelegateArgs& a_params);
		static void OnStrike(const RE::FxDelegateArgs& a_params);
		static void OnClose(const RE::FxDelegateArgs& a_params);
		static void OnHammer(const RE::FxDelegateArgs& a_params);
		static void OnPlaySound(const RE::FxDelegateArgs& a_params);
		static void OnRequestHotkeys(const RE::FxDelegateArgs& a_params);
	};

	// Opens the menu on the SKYBDegOpenRepairMenu mod event.
	class RepairMenuOpener : public RE::BSTEventSink<SKSE::ModCallbackEvent>
	{
	public:
		static RepairMenuOpener* GetSingleton()
		{
			static RepairMenuOpener singleton;
			return std::addressof(singleton);
		}
		void                     Register();
		RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
											  RE::BSTEventSource<SKSE::ModCallbackEvent>*) override;
	};

	// Diagnostics: logs what the inventory item card will show for every
	// equipped charged weapon when the inventory opens and closes.
	class ChargeWatcher : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		static ChargeWatcher* GetSingleton()
		{
			static ChargeWatcher singleton;
			return std::addressof(singleton);
		}
		void                     Register();
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
											  RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;
	};
}
