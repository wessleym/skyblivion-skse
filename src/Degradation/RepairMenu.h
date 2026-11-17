#pragma once
#include <string_view>

// The repair menu: a native IMenu around Interface\Skyblivion\SKYBGearService.swf.
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
// Opened by InventoryHook (repair tool activated), the SKYBDegOpenRepairMenu mod event, or the
// dialogue services strip (DialogueServices: repair, recharge; the SKYBDegradation.OpenVendor* natives
// of earlier versions are gone, 2026-11-06):
//   bench mode     OpenAtBench: the mod event with strArg "bench" and numArg 1 (grindstone) or 2
//                  (armour workbench), sent by the bench's script (SKYBRepairBench). The player's
//                  menu with header.bench = "grindstone" | "workbench" (mode stays "player"): a
//                  grindstone lists weapons, a workbench armour and shields. One repair takes the
//                  item to Condition::BenchCap and spends one hammer, no break roll (a hammer that
//                  never breaks is not spent); toLimit means the same. The enchanted-gear rule holds.
//   vendor mode    OpenVendor: the vendor's Smithing sets the limit, no hammers,
//                  the player pays gold (Condition::VendorCost) to 100% or to the vendor's limit
//   recharge mode  OpenRecharge: lists every charged weapon / staff; Recharge (E)
//                  fills the selected one, Recharge All (R) every one; price = missing charge
//                  (Condition::RechargeCost)
// Opening from the inventory closes it first (no inventory changes under a live item list); with
// ReturnToInventory = 1 in the INI it reopens when the repair menu closes.
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
		// A repair bench in the world: which gear it takes.
		enum class Bench
		{
			kNone,     // not at a bench (the hammer menu, a vendor)
			kWeapons,  // grindstone
			kArmor     // armour workbench: armour and shields
		};
		static void OpenAtBench(Bench a_bench);
		static void Close();

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

	// Opens the menu on the SKYBDegOpenRepairMenu mod event: no argument = the hammer menu,
	// strArg "bench" + numArg 1 / 2 = at a grindstone / an armour workbench.
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

}
