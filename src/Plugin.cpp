#include "Plugin.h"

#include "Achievements/AchievementService.h"
#include "Degradation/Breakage.h"
#include "Degradation/DecayService.h"
#include "Degradation/InventoryHook.h"
#include "Degradation/RepairMenu.h"
#include "Degradation/Settings.h"
#include "Degradation/TemperHook.h"
#include "Disposition/DispositionSystem.h"
#include "Hotkeys/HotkeyService.h"
#include "Papyrus/SKSEScriptRegistrar.h"
#include "PerkSpells/EntryPointSpells.h"
#include "Platform/FormCache.h"
#include "Platform/MainThread.h"
#include "Resources.h"
#include "Settings/SettingsService.h"
#include "SettingsMenu/CombatService.h"
#include "SettingsMenu/EconomyService.h"
#include "SettingsMenu/InterfaceAdapters.h"
#include "SettingsMenu/SettingsMenu.h"
#include "SettingsMenu/SettingsStore.h"
#include "SettingsMenu/SoundService.h"
#include "Sigil/SigilSystem.h"
#include "SpellReflect/SpellReflect.h"
#include "Storage/CoSave.h"
#include "UIs/PrismaUI/Persuasion/PersuasionSettings.h"
#include "UIs/Scaleform/Alchemy/AlchemyMenuFeed.h"
#include "UIs/Scaleform/Cursor/HoverCursor.h"
#include "UIs/Scaleform/ScrollCraft/ScrollCastXP.h"
#include "UIs/Scaleform/ScrollCraft/ScrollRecipeStore.h"
#include "UIs/Scaleform/StartMenu/StartMenuService.h"
#include "UIs/TweenMenuFilter.h"
#include "UIs/UISystem.h"

#include <Windows.h>

bool Plugin::OnLoad(const SKSE::LoadInterface* a_skse)
{
	auto logsFolder = SKSE::log::log_directory();
	if (!logsFolder) {
		Log::CRITICAL("!logsFolder");
		return false;
	}

	SKSE::Init(a_skse);

	Log::INFO("Skyblivion SKSE Plugin Loading...");

	//Loads block the main thread, which owns the window, so after five seconds a click makes Windows
	//show "(Not Responding)" although the load is proceeding. Ghosting is turned off for the process.
	::DisableProcessWindowsGhosting();
	Log::INFO("Window ghosting disabled: no \"(Not Responding)\" during long loads.");

	if (a_skse->IsEditor()) {
		Log::CRITICAL("Loaded in editor (Creation Kit)");
		return false;
	}

	if (!SKSEScriptRegistrar::Initialize()) {
		return false;
	}

	DispositionSystem::Initialize();

	//One SKSE trampoline for every call-site hook in the plugin (the frame hook, the temper
	//patches, PerkSpells, SpellReflect): AllocTrampoline replaces the previous allocation, so it is
	//called once, here. The trampoline keeps one 14-byte jump per destination: the frame hook 1, the
	//temper patches 5, PerkSpells 1 (all its call sites share one thunk), SpellReflect 6 (five
	//projectile-hit thunks, one melee-hit thunk). 13 x 14 bytes; 512 leaves room.
	SKSE::AllocTrampoline(1 << 9);
	//Calls into live Scaleform movies run on the main thread, once a frame (SKSE's task queues
	//can run on worker threads).
	MainThread::Install();

	//Weapon and armor degradation. The temper patches install on Anniversary Edition runtimes
	//only; elsewhere they log why and stay off.
	SKYBDeg::InventoryHook::Install();
	SKYBDeg::TemperHook::Install();

	//Perk "Apply ... Spell" entry points stack and the perk owner casts (the former standalone
	//SKYBPerkSpells.dll, folded in; settings in SKYBPerkSpells.ini). At plugin load, as it was there.
	PerkSpells::EntryPointSpells::Install();

	//Timed block (ranked by the Block mastery perks), arrow and spell parries, chance-based spell
	//reflection (the former standalone SKYBSpellReflect.dll, folded in; settings in
	//SKYBSpellReflect.ini). At plugin load, as it was there.
	SpellReflect::Install();

	UISystem::Initialize();

	//The vanilla crafting menu's alchemy data: SKSE takes Scaleform callbacks only while loading.
	Alchemy::AlchemyMenuFeed::InstallScaleform();

	//The pointing-hand cursor over anything clickable in any menu: a Mouse listener in every movie.
	HoverCursor::InstallScaleform();

	//One co-save for the plugin: every store adds its record first (UISystem: spell making).
	SKYBSettingsMenu::Store::InstallSerialization();
	//Achievements: this save's unlocks (definitions in ConfigFiles\Achievements).
	Achievements::AchievementService::InstallSerialization();
	Storage::CoSave::Install();

	//SKSE::GetMessagingInterface()->RegisterListener only calls back to the first registered listener.
	auto OnMessage = [](SKSE::MessagingInterface::Message* msg) {
		switch (msg->type) {
		case SKSE::MessagingInterface::kPostLoad:
			UISystem::OnPostLoad();
			break;
		case SKSE::MessagingInterface::kInputLoaded:
			UISystem::OnInputLoaded();
			//The block-press listener for timed blocks and parries.
			SpellReflect::OnInputLoaded();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			UISystem::OnPreLoadGame({ static_cast<const char*>(msg->data), msg->dataLen });
			break;
		case SKSE::MessagingInterface::kNewGame:
			UISystem::OnGameStarting();
			//The new game's globals exist a frame later.
			SKYBDeg::Settings::ReloadIfChanged();
			Persuasion::Settings::ReloadIfChanged();
			ScrollCraft::ScrollCastXP::GetSingleton()->Reload();
			SKSE::GetTaskInterface()->AddTask([] {
				SKYBSettingsMenu::Store::GetSingleton()->OnGameReady(true);
				StartMenu::StartMenuService::OnNewGame();  // the new-game panel's presets, if chosen
			});
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			//Reload the degradation and persuasion INIs if they changed, then apply every settings-menu
			//row (its degradation and persuasion overrides among them).
			SKYBDeg::Settings::ReloadIfChanged();
			Persuasion::Settings::ReloadIfChanged();
			ScrollCraft::ScrollCastXP::GetSingleton()->Reload();
			SKYBSettingsMenu::Store::GetSingleton()->OnGameReady(false);
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			Log::INFO("SKSE Data Loaded...");
			//The systems below use Resources and the form table (po3 Tweaks' editor-ID map is built by now).
			Resources::Install();
			FormCache::OnDataLoaded();
			//Timed block ranks, parry perks, feedback records (SKYBSpellReflect.ini).
			SpellReflect::OnDataLoaded();
			//The apparatus perks and the effect baselines the alchemy menu shows.
			Alchemy::AlchemyMenuFeed::OnDataLoaded();
			//The scroll recipes (ConfigFiles/ScrollCrafting/recipes.json); the forms through FormCache.
			ScrollCraft::ScrollRecipeStore::LoadOnce();
			//Casting a scroll or firing a staff trains Enchanting (SKYBScrollCrafting.ini [CastingXP]).
			ScrollCraft::ScrollCastXP::GetSingleton()->Register();
			DispositionSystem::OnDataLoaded();
			Sigil::SigilSystem::Register();
			Achievements::AchievementService::Register();
			//Before UISystem: the title screen and the journal read the settings.
			Settings::SettingsService::Register();
			//The settings menu and the services it drives.
			//Combat before the store: the store applies through it.
			CombatService::GetSingleton()->Register();
			SKYBSettingsMenu::SoundService::GetSingleton()->Register();
			EconomyService::GetSingleton()->Register();
			HotkeyService::GetSingleton()->Register();
			InterfaceAdapters::GetSingleton()->Register();
			SKYBSettingsMenu::Store::GetSingleton()->Register();
			SKYBSettingsMenu::SettingsMenu::Register();
			//Degradation: its INI and tool list, the repair menu and its opener, the decay itself.
			SKYBDeg::Settings::Resolve();
			SKYBDeg::RepairMenu::Register();
			SKYBDeg::RepairMenuOpener::GetSingleton()->Register();
			SKYBDeg::DecayService::GetSingleton()->Register();
			SKYBDeg::Breakage::EquipGuard::GetSingleton()->Register();
			//Persuasion: its INI and perk rules (UISystem opens the wheel).
			Persuasion::Settings::Resolve();
			UISystem::OnDataLoaded();
			//The Tween menu's tool options (TOOLS). Here, not at kNewGame / kPostLoadGame: 'coc' from the
			//title screen sends neither. Its pass runs the frame after the open, after Tween Menu Overhaul's sink.
			TweenTools::TweenMenuFilter::Register();
			Log::INFO("SKSE Data Loaded Complete");
			break;
		default:
			break;
		}
	};
	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		return false;
	}

	Log::INFO("Listener Registered. Waiting for Listener to Be Called...");

	return true;
}
