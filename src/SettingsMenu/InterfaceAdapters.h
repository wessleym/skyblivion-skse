#pragma once
// Moved from SKYBUIDemo.dll into Skyblivion.dll, 2026-10-05 (fold-in, UI integration workspace
// ledger L-0056). Source of record until then: game\native\SKYBUIDemo\src in that workspace.
#include <string>
#include <string_view>
#include <unordered_map>
// Settings menu › Interface: the adapters that hand our values to the UI mods
// Skyblivion bundles, plus hiding Skyblivion's own pages from the MCM list.
//
//   moreHUD   its settings are GlobalVariables (AHZ*, merged into
//             Skyblivion.esm). SetGlobal writes one by editor ID; moreHUD
//             reads them in ahzmainquest.UpdateSettings(false), which this
//             service calls once per frame of changes (coalesced).
//   QuickLoot its DLL reads the auto-properties of the QuickLootIEMCM script
//             (quest MCMQuickLootIE) when QuickLootIENative.UpdateVariables()
//             runs. SetQuickLoot writes one property, then the update is
//             called once (coalesced). On a NEW game (and coc from the title
//             screen) QuickLoot's script runs its first-time init a second or
//             two after game ready (OnConfigInit -> ImportSettings(true) ->
//             ResetSettings) and overwrites every property. So while its
//             IsInitialLoad is still true the writes stay pending and are
//             retried on each SKICP_configManagerReady (every 5 s at first).
//   MCM       Skyblivion itself needs no MCM page, so SkyUI's own page
//             (SKI_ConfigMenuInstance) and QuickLoot's (MCMQuickLootIE) are
//             removed from the list with SkyUI's public
//             SKI_ConfigManager.UnregisterMod every time SkyUI announces
//             SKICP_configManagerReady. An unregistered page keeps its own
//             "registered" mark, so it never re-registers, and the removal is
//             saved with the game. The scripts stay loaded, so QuickLoot keeps
//             reading its properties. Other mods' MCM pages are untouched.
//
// Nothing here copies code from those mods: only their public data (globals,
// script properties) and public functions are used.
class InterfaceAdapters : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
public:
	static InterfaceAdapters* GetSingleton();
	void Register();  // kDataLoaded

	void SetGlobal(std::string_view a_editorID, float a_value);
	void SetQuickLoot(std::string_view a_property, float a_value);

	RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
										  RE::BSTEventSource<SKSE::ModCallbackEvent>*) override;

private:
	InterfaceAdapters() = default;
	void ScheduleFlush();
	void Flush();
	static void HideMcmPages();

	bool _moreHudDirty = false;
	bool _quickLootDirty = false;
	bool _flushQueued = false;
	bool _quickLootWaitLogged = false;
	std::unordered_map<std::string, float> _quickLoot;  // pending property writes (applied on flush)
};
