#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
// Settings menu > Interface: passes the menu's values to the bundled UI mods and
// hides SkyUI's and QuickLoot's MCM pages.
//
//   moreHUD   settings are AHZ* GlobalVariables. SetGlobal writes one; the flush
//             calls ahzmainquest.UpdateSettings once per frame of changes.
//   QuickLoot SetQuickLoot writes a property of QuickLootIEMCM (quest
//             MCMQuickLootIE); the flush calls QuickLootIENative.UpdateVariables
//             once. Its first-time init on a new game overwrites every property,
//             so while IsInitialLoad is true the writes stay pending and are
//             retried on each SKICP_configManagerReady.
//   MCM       the pages are removed with SKI_ConfigManager.UnregisterMod on every
//             SKICP_configManagerReady. A removed page never re-registers and the
//             removal is saved; the scripts stay loaded. Other mods' pages are kept.
//
// Only the mods' public data (globals, script properties) and functions are used.
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
	bool _quickLootOn = true;                              // the settings menu's Loot Window switch
	std::unordered_map<std::string, float> _quickLootWanted;  // the rows' EnableFor* values
	// Every QuickLoot property as last written. QuickLoot's MCM can run its first-time setup again a
	// moment after a game starts (coc from the title screen) and put its defaults back; Recheck, a few
	// seconds after each write and on each SKICP_configManagerReady, writes back whatever differs.
	std::unordered_map<std::string, float> _quickLootApplied;
	void ScheduleRecheck();
	void Recheck();
};
