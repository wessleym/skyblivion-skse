#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
// The values behind the Skyblivion settings menu (SKYBSettingsMenu).
//
// Every option is a row of SettingsMenuTable.h (generated from
// SKYBSettingsMenu\Settings.csv). Where a value lives depends on its scope:
//
//   global  SKYBSettings.ini [SettingsMenu] <id>=<value>, shared by every
//           character (UI, hotkeys, accessibility)
//   save    this service's SKSE co-save record, one set per character
//           (progression, combat). On the title screen (no character loaded)
//           a save row edits the new-game defaults instead:
//           SKYBSettings.ini [SettingsMenuNewGame] <id>=<value>
//
// A value never written is the row's Classic preset. A save loaded without
// this record (an older save, or a new game) takes the new-game defaults then
// and keeps them.
//
// After every change and on every game load each value is pushed to what acts
// on it: SKYBDeg::Settings, Persuasion::Settings, CombatService, EconomyService,
// InterfaceAdapters (moreHUD globals, QuickLoot properties) and SettingsService
// (rows that mirror an existing SKYBSettings.ini key read and write that key).
#include "SettingsMenu/SettingsMenuTable.h"

namespace SKYBSettingsMenu
{
	class Store : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		static Store* GetSingleton();

		static void InstallSerialization();  // plugin load: adds its record to Storage::CoSave
		void        Register();              // kDataLoaded
		void        OnGameReady();           // kPostLoadGame / kNewGame: fill the save's values, apply every row

		// True on the title screen / before any game: save rows then edit the
		// new-game defaults.
		[[nodiscard]] bool EditingDefaults() const;

		[[nodiscard]] float Get(const RowDef& a_row) const;
		void                Set(const RowDef& a_row, float a_value);
		void                Step(const RowDef& a_row, int a_dir);  // number: +-step (clamped); toggle/choice: wraps
		void                ApplyPreset(std::string_view a_category, std::size_t a_preset);
		// The category's save rows take the preset on the character now loaded (the new-game panel's
		// choice, after OnGameReady), whatever menu is open.
		void                ApplyPresetToCharacter(std::string_view a_category, std::size_t a_preset);

		// Preset the category's rows match right now: 0..kPresetCount-1,
		// kCustom when they match none, kNoPresets for categories without them.
		static constexpr int kCustom = -1;
		static constexpr int kNoPresets = -2;
		[[nodiscard]] int PresetOf(std::string_view a_category) const;

		[[nodiscard]] static const RowDef*      FindRow(std::string_view a_id);
		[[nodiscard]] static const CategoryDef* FindCategory(std::string_view a_id);
		[[nodiscard]] static float              Snap(const RowDef& a_row, float a_value);

		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
											  RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

		static constexpr std::string_view kGlobalSection = "SettingsMenu";
		static constexpr std::string_view kDefaultsSection = "SettingsMenuNewGame";

	private:
		Store() = default;

		[[nodiscard]] float StoredDefault(const RowDef& a_row) const;  // INI value or Classic
		static void         Apply(const RowDef& a_row, float a_value);

		static void OnSave(SKSE::SerializationInterface* a_intfc);
		static void OnLoad(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t a_length);
		static void OnLoaded();
		static void OnRevert(SKSE::SerializationInterface* a_intfc);

		std::unordered_map<std::string, float> _save;  // this character's save-scope values
		mutable std::mutex                     _lock;
		bool                                   _gameLoaded = false;
		bool                                   _applied = false;  // OnGameReady ran for the game now loaded
		std::size_t                            _read = 0;  // values read from the save being loaded
	};
}
