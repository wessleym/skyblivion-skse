#include "SettingsMenu/SettingsStore.h"

#include <charconv>
#include <cstdlib>
#include <cstring>
#include <format>

#include <Windows.h>
#include <array>
#include <cmath>
#include <optional>
#include <unordered_set>

#include "SettingsMenu/CombatService.h"
#include "SettingsMenu/EconomyService.h"
#include "SettingsMenu/SoundService.h"
#include "Hotkeys/HotkeyService.h"
#include "SettingsMenu/InterfaceAdapters.h"
#include "Degradation/Settings.h"
#include "UIs/PrismaUI/Persuasion/PersuasionSettings.h"
#include "Settings/SettingsService.h"
#include "Storage/CoSave.h"

namespace SKYBSettingsMenu
{
	namespace
	{
		// this store's record in the plugin's co-save (Storage/CoSave)
		constexpr std::uint32_t kRecord = 'SVAL';
		constexpr std::uint32_t kVersion = 1;

		bool Same(float a, float b, float step) { return std::fabs(a - b) <= step * 0.01f; }

		std::optional<float> ParseFloat(const std::string& a_s)
		{
			float v = 0.0f;
			const auto [ptr, err] = std::from_chars(a_s.data(), a_s.data() + a_s.size(), v);
			if (a_s.empty() || err != std::errc{} || ptr != a_s.data() + a_s.size()) {
				return std::nullopt;
			}
			return v;
		}
	}

	Store* Store::GetSingleton()
	{
		static Store singleton;
		return &singleton;
	}

	const RowDef* Store::FindRow(std::string_view a_id)
	{
		for (const auto& r : kRows) {
			if (a_id == r.id) {
				return &r;
			}
		}
		return nullptr;
	}

	const CategoryDef* Store::FindCategory(std::string_view a_id)
	{
		for (const auto& c : kCategories) {
			if (a_id == c.id) {
				return &c;
			}
		}
		return nullptr;
	}

	float Store::Snap(const RowDef& a_row, float a_value)
	{
		const float k = std::round((a_value - a_row.min) / a_row.step);
		return std::clamp(a_row.min + k * a_row.step, a_row.min, a_row.max);
	}

	void Store::InstallSerialization()
	{
		Storage::CoSave::Store store;
		store.name = "SettingsMenu";
		store.recordType = kRecord;
		store.save = OnSave;
		store.load = OnLoad;
		store.revert = OnRevert;
		store.loaded = OnLoaded;
		Storage::CoSave::Add(store);
	}

	void Store::Register()
	{
		if (auto ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
		}
		Log::INFO("settings menu: store registered ({} rows, {} categories, {} presets)", kRows.size(),
					 kCategories.size(), kPresetCount);
	}

	RE::BSEventNotifyControl Store::ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
												 RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
	{
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		// back on the title screen: no character is loaded any more (a load
		// or new game from here reverts + refills the map anyway)
		if (a_event->opening && a_event->menuName == RE::MainMenu::MENU_NAME) {
			std::scoped_lock lock(_lock);
			_gameLoaded = false;
			_save.clear();
			return RE::BSEventNotifyControl::kContinue;
		}
		// 'coc' from the title screen starts a game without kPostLoadGame / kNewGame:
		// when a loading screen closes in a world, set the game up if nothing else did.
		if (!a_event->opening && a_event->menuName == RE::LoadingMenu::MENU_NAME) {
			SKSE::GetTaskInterface()->AddTask([this] {
				{
					std::scoped_lock lock(_lock);
					if (_gameLoaded) {
						return;
					}
				}
				auto ui = RE::UI::GetSingleton();
				auto player = RE::PlayerCharacter::GetSingleton();
				if (!player || !player->GetParentCell() || (ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME))) {
					return;
				}
				Log::INFO("settings menu: a game started without a load / new-game message (coc from the title screen?)");
				OnGameReady();
			});
		}
		return RE::BSEventNotifyControl::kContinue;
	}

	// ---------------------------------------------------------- the game's own settings (applier gameini)
	// "name:Section" as the game names it; the prefs collection (SkyrimPrefs.ini) first, then Skyrim.ini's.
	// The game's value is the stored one: the row reads it and writes it back, and a change is saved to
	// SkyrimPrefs.ini as the game's own menu does.
	namespace
	{
		// the setting of this name under any section (bCrosshairEnabled, bSaveOn*, fHUDOpacity are not found
		// as "name:Main", though SkyrimPrefs.ini keeps them under [MAIN]: the game registers them otherwise)
		RE::Setting* FindByName(RE::INISettingCollection* a_coll, std::string_view a_name)
		{
			if (!a_coll) {
				return nullptr;
			}
			for (auto* setting : a_coll->settings) {
				const char* n = setting ? setting->GetName() : nullptr;
				if (n && std::strlen(n) > a_name.size() && n[a_name.size()] == ':' &&
					_strnicmp(n, a_name.data(), a_name.size()) == 0) {
					return setting;
				}
			}
			return nullptr;
		}

		RE::Setting* GameSetting(std::string_view a_name, bool* a_isPref = nullptr)
		{
			auto* prefs = RE::INIPrefSettingCollection::GetSingleton();
			auto* ini = RE::INISettingCollection::GetSingleton();
			RE::Setting* found = nullptr;
			bool         pref = false;
			if (prefs && (found = prefs->GetSetting(a_name)) != nullptr) {
				pref = true;
			} else if (ini && (found = ini->GetSetting(a_name)) != nullptr) {
				pref = false;
			} else {
				const auto bare = a_name.substr(0, a_name.find(':'));
				if ((found = FindByName(prefs, bare)) != nullptr) {
					pref = true;
				} else {
					found = FindByName(ini, bare);
				}
				static std::unordered_set<std::string> told;
				if (told.insert(std::string(a_name)).second) {
					if (found) {
						Log::INFO("settings menu: {} is the game's {} ({})", a_name, found->GetName(), pref ? "SkyrimPrefs.ini" : "Skyrim.ini");
					} else {
						Log::INFO("settings menu: the game registers no setting {} - read and written in SkyrimPrefs.ini itself (takes effect at the next start)", a_name);
					}
				}
			}
			if (found && a_isPref) {
				*a_isPref = pref;
			}
			return found;
		}

		// a setting the game does not register: SkyrimPrefs.ini's own line ("name:Section" -> [Section] name)
		const char* PrefsFile()
		{
			auto* prefs = RE::INIPrefSettingCollection::GetSingleton();
			return prefs && prefs->subKey[0] ? prefs->subKey : nullptr;
		}

		std::optional<float> ReadPrefsLine(std::string_view a_name)
		{
			const char* path = PrefsFile();
			const auto  colon = a_name.find(':');
			if (!path || colon == std::string_view::npos) {
				return std::nullopt;
			}
			char buf[64];
			const std::string key(a_name.substr(0, colon)), sec(a_name.substr(colon + 1));
			GetPrivateProfileStringA(sec.c_str(), key.c_str(), "", buf, sizeof(buf), path);
			if (!buf[0]) {
				return std::nullopt;
			}
			return static_cast<float>(std::atof(buf));
		}

		bool WritePrefsLine(std::string_view a_name, float a_value)
		{
			const char* path = PrefsFile();
			const auto  colon = a_name.find(':');
			if (!path || colon == std::string_view::npos) {
				return false;
			}
			const std::string key(a_name.substr(0, colon)), sec(a_name.substr(colon + 1));
			const std::string text = key[0] == 'f' ? std::format("{:.4f}", a_value) : std::to_string(std::lround(a_value));
			return WritePrivateProfileStringA(sec.c_str(), key.c_str(), text.c_str(), path) != 0;
		}

		std::optional<float> ReadOneGameSetting(std::string_view a_name);
		void                 WriteOneGameSetting(std::string_view a_name, float a_value);

		// The two [MAIN] options the HUD shows: the game reads them at start only, so a change is also put on
		// the HUD at once - the crosshair through the HUD's own SetCrosshairEnabled (what the game calls), the opacity
		// as the HUD's alpha - and again when a game is ready (the game would put its start-up value back).
		void ApplyToHud(std::string_view a_name, float a_value)
		{
			const bool crosshair = _strnicmp(a_name.data(), "bCrosshairEnabled:", 18) == 0;
			const bool opacity = _strnicmp(a_name.data(), "fHUDOpacity:", 12) == 0;
			if (!crosshair && !opacity) {
				return;
			}
			auto* ui = RE::UI::GetSingleton();
			auto  hud = ui ? ui->GetMovieView(RE::HUDMenu::MENU_NAME) : nullptr;
			if (!hud) {
				return;
			}
			if (crosshair) {
				RE::GFxValue arg(a_value >= 0.5f);
				hud->Invoke("_root.HUDMovieBaseInstance.SetCrosshairEnabled", nullptr, &arg, 1);
			} else {
				hud->SetVariable("_root.HUDMovieBaseInstance._alpha", RE::GFxValue(static_cast<double>(a_value) * 100.0));
			}
		}

		// the autosave row's intervals (minutes), after "Off"; the game's own list
		constexpr std::array<float, 6> kAutosaveMinutes{ 5.0f, 10.0f, 15.0f, 30.0f, 45.0f, 60.0f };

		// a gameini target: "name:Section", "name:Section*other:Section" (a share of the other) or "autosave"
		std::optional<float> ReadGameSetting(std::string_view a_target)
		{
			if (a_target == "autosave") {
				const auto on = ReadOneGameSetting("bSaveOnPause:Main");
				if (!on) {
					return std::nullopt;
				}
				if (*on < 0.5f) {
					return 0.0f;
				}
				const float minutes = ReadOneGameSetting("fAutosaveEveryXMins:SaveGame").value_or(10.0f);
				std::size_t best = 0;
				for (std::size_t i = 1; i < kAutosaveMinutes.size(); ++i) {
					if (std::abs(kAutosaveMinutes[i] - minutes) < std::abs(kAutosaveMinutes[best] - minutes)) {
						best = i;
					}
				}
				return static_cast<float>(best + 1);
			}
			if (const auto star = a_target.find('*'); star != std::string_view::npos) {
				const auto v = ReadOneGameSetting(a_target.substr(0, star));
				const auto m = ReadOneGameSetting(a_target.substr(star + 1));
				if (!v || !m || *m <= 0.0f) {
					return std::nullopt;
				}
				return *v / *m;
			}
			return ReadOneGameSetting(a_target);
		}

		void WriteGameSetting(std::string_view a_target, float a_value)
		{
			if (a_target == "autosave") {
				const int i = static_cast<int>(std::lround(a_value));
				WriteOneGameSetting("bSaveOnPause:Main", i > 0 ? 1.0f : 0.0f);
				if (i > 0 && i <= static_cast<int>(kAutosaveMinutes.size())) {
					WriteOneGameSetting("fAutosaveEveryXMins:SaveGame", kAutosaveMinutes[i - 1]);
				}
				return;
			}
			if (const auto star = a_target.find('*'); star != std::string_view::npos) {
				const auto m = ReadOneGameSetting(a_target.substr(star + 1));
				if (!m) {
					Log::WARN("settings menu: the game has no setting {} - {} not changed", a_target.substr(star + 1),
							  a_target.substr(0, star));
					return;
				}
				WriteOneGameSetting(a_target.substr(0, star), a_value * *m);
				return;
			}
			WriteOneGameSetting(a_target, a_value);
		}

		std::optional<float> ReadOneGameSetting(std::string_view a_name)
		{
			const auto* s = GameSetting(a_name);
			if (!s) {
				return ReadPrefsLine(a_name);
			}
			switch (s->GetType()) {
			case RE::Setting::Type::kBool:
				return s->data.b ? 1.0f : 0.0f;
			case RE::Setting::Type::kFloat:
				return s->data.f;
			case RE::Setting::Type::kInteger:
				return static_cast<float>(s->data.i);
			case RE::Setting::Type::kUnsignedInteger:
				return static_cast<float>(s->data.u);
			default:
				return std::nullopt;
			}
		}

		void WriteOneGameSetting(std::string_view a_name, float a_value)
		{
			bool  pref = false;
			auto* s = GameSetting(a_name, &pref);
			if (!s) {
				const bool saved = WritePrefsLine(a_name, a_value);
				ApplyToHud(a_name, a_value);
				Log::INFO("settings menu: {} = {} in SkyrimPrefs.ini ({})", a_name, a_value,
						  saved ? "the game takes it at the next start" : "could not be written");
				return;
			}
			switch (s->GetType()) {
			case RE::Setting::Type::kBool:
				s->data.b = a_value >= 0.5f;
				break;
			case RE::Setting::Type::kFloat:
				s->data.f = a_value;
				break;
			case RE::Setting::Type::kInteger:
				s->data.i = static_cast<std::int32_t>(std::lround(a_value));
				break;
			case RE::Setting::Type::kUnsignedInteger:
				s->data.u = static_cast<std::uint32_t>(std::max(0L, std::lround(a_value)));
				break;
			default:
				Log::WARN("settings menu: {} is not a number or true/false setting - not changed", a_name);
				return;
			}
			if (pref) {
				RE::INIPrefSettingCollection::GetSingleton()->WriteSetting(s);
			}
			Log::INFO("settings menu: game setting {} = {}", a_name, a_value);
		}
	}

	// a global by editor ID (applier gameglobal): its value, nullopt when no loaded plugin has it
	namespace
	{
		std::optional<float> ReadGlobal(std::string_view a_editorID)
		{
			auto* g = RE::TESForm::LookupByEditorID<RE::TESGlobal>(std::string(a_editorID));
			return g ? std::optional<float>(g->value) : std::nullopt;
		}
	}

	bool Store::EditingDefaults() const
	{
		// ask the UI before taking our lock: the UI's own lock may be held by
		// a thread that is about to deliver a menu event to ProcessEvent
		auto       ui = RE::UI::GetSingleton();
		const bool title = ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME);
		std::scoped_lock lock(_lock);
		return !_gameLoaded || title;
	}

	float Store::StoredDefault(const RowDef& a_row) const
	{
		if (a_row.applier == Applier::kGameIni) {
			if (const auto v = ReadGameSetting(a_row.key)) {
				return Snap(a_row, *v);
			}
			return a_row.presets[0];
		}
		if (a_row.applier == Applier::kSound) {
			if (const auto v = SoundService::GetSingleton()->Get(a_row.key)) {
				return Snap(a_row, *v);
			}
			return a_row.presets[0];
		}
		// a row that mirrors an existing SKYBSettings.ini key reads that key,
		// so the title screen / journal toggles and this menu agree
		if (a_row.applier == Applier::kSettingsIni) {
			const auto s = Settings::SettingsService::GetString(a_row.section, a_row.key, "");
			if (const auto v = ParseFloat(s)) {
				return Snap(a_row, *v);
			}
			return a_row.presets[0];
		}
		const auto section = a_row.scope == Scope::kGlobal ? kGlobalSection : kDefaultsSection;
		const auto s = Settings::SettingsService::GetString(section, a_row.id, "");
		if (const auto v = ParseFloat(s)) {
			return Snap(a_row, *v);
		}
		return a_row.presets[0];
	}

	float Store::Get(const RowDef& a_row) const
	{
		// in a game a gameglobal row is the global itself (the game's own switch may have changed it)
		if (a_row.applier == Applier::kGameGlobal && !EditingDefaults()) {
			if (const auto v = ReadGlobal(a_row.key)) {
				return Snap(a_row, *v);
			}
		}
		if (a_row.scope == Scope::kSave && !EditingDefaults()) {
			std::scoped_lock lock(_lock);
			if (const auto it = _save.find(a_row.id); it != _save.end()) {
				return it->second;
			}
		}
		return StoredDefault(a_row);
	}

	void Store::Set(const RowDef& a_row, float a_value)
	{
		const float v = Snap(a_row, a_value);
		const bool  defaults = EditingDefaults();
		if (a_row.scope == Scope::kSave && !defaults) {
			{
				std::scoped_lock lock(_lock);
				_save[a_row.id] = v;
			}
			Log::INFO("settings menu: {} = {} (this character)", a_row.id, v);
			Apply(a_row, v);
			return;
		}
		if (a_row.applier == Applier::kSettingsIni) {
			Settings::SettingsService::SetInt(a_row.section, a_row.key, static_cast<int>(std::lround(v)));
			Apply(a_row, v);
			return;
		}
		if (a_row.applier == Applier::kGameIni) {
			WriteGameSetting(a_row.key, v);
			return;
		}
		if (a_row.applier == Applier::kSound) {
			SoundService::GetSingleton()->Set(a_row.key, v);
			return;
		}
		const auto section = a_row.scope == Scope::kGlobal ? kGlobalSection : kDefaultsSection;
		Settings::SettingsService::SetString(section, a_row.id, std::format("{}", v));
		if (a_row.scope == Scope::kGlobal) {
			Apply(a_row, v);
		}
	}

	void Store::Step(const RowDef& a_row, int a_dir)
	{
		const float cur = Get(a_row);
		float       next = cur + a_row.step * static_cast<float>(a_dir);
		if (a_row.type != Type::kNumber) {
			// toggles and choices wrap around
			if (next > a_row.max + a_row.step * 0.5f) {
				next = a_row.min;
			} else if (next < a_row.min - a_row.step * 0.5f) {
				next = a_row.max;
			}
		}
		Set(a_row, next);
	}

	int Store::PresetOf(std::string_view a_category) const
	{
		const auto* cat = FindCategory(a_category);
		if (!cat || !cat->presets) {
			return kNoPresets;
		}
		bool any = false;
		std::array<bool, kPresetCount> match{};
		match.fill(true);
		for (const auto& r : kRows) {
			if (a_category != r.category) {
				continue;
			}
			any = true;
			const float v = Get(r);
			for (std::size_t p = 0; p < kPresetCount; ++p) {
				match[p] = match[p] && Same(v, r.presets[p], r.step);
			}
		}
		if (!any) {
			return kNoPresets;
		}
		for (std::size_t p = 0; p < kPresetCount; ++p) {
			if (match[p]) {
				return static_cast<int>(p);
			}
		}
		return kCustom;
	}

	void Store::ApplyPreset(std::string_view a_category, std::size_t a_preset)
	{
		if (a_preset >= kPresetCount) {
			return;
		}
		Log::INFO("settings menu: {} preset -> {}", a_category, kPresetIds[a_preset]);
		for (const auto& r : kRows) {
			if (a_category == r.category) {
				Set(r, r.presets[a_preset]);
			}
		}
	}

	void Store::ApplyPresetToCharacter(std::string_view a_category, std::size_t a_preset)
	{
		if (a_preset >= kPresetCount) {
			return;
		}
		for (const auto& r : kRows) {
			if (a_category != r.category || r.scope != Scope::kSave) {
				continue;
			}
			const float v = Snap(r, r.presets[a_preset]);
			{
				std::scoped_lock lock(_lock);
				_save[r.id] = v;
			}
			Apply(r, v);
		}
		Log::INFO("settings menu: {} preset {} on this character", a_category, kPresetIds[a_preset]);
	}

	void Store::Apply(const RowDef& a_row, float a_value)
	{
		switch (a_row.applier) {
		case Applier::kDegradation:
			if (!SKYBDeg::Settings::SetOverride(a_row.section, a_row.key, a_value)) {
				Log::WARN("settings menu: degradation has no key [{}] {}", a_row.section, a_row.key);
			}
			break;
		case Applier::kCombat:
			CombatService::GetSingleton()->Set(a_row.key, a_value);
			break;
		case Applier::kGlobalVar:
			// one row may set several globals: "A+B" (moreHUD's two enemy meters)
			for (std::string_view rest = a_row.key; !rest.empty();) {
				const auto plus = rest.find('+');
				InterfaceAdapters::GetSingleton()->SetGlobal(rest.substr(0, plus), a_value);
				rest = plus == std::string_view::npos ? std::string_view{} : rest.substr(plus + 1);
			}
			break;
		case Applier::kQuickLoot:
			InterfaceAdapters::GetSingleton()->SetQuickLoot(a_row.key, a_value);
			break;
		case Applier::kEconomy:
			EconomyService::GetSingleton()->Set(a_row.section, a_row.key, a_value);
			break;
		case Applier::kChanceNone:
			EconomyService::GetSingleton()->SetChanceNone(a_row.key, a_value);
			break;
		case Applier::kPersuasion:
			if (!Persuasion::Settings::SetOverride(a_row.section, a_row.key, a_value)) {
				Log::WARN("settings menu: persuasion has no key [{}] {}", a_row.section, a_row.key);
			}
			break;
		case Applier::kGameGlobal:
			InterfaceAdapters::GetSingleton()->SetGlobal(a_row.key, a_value);
			if (const auto v = ReadGlobal(a_row.key)) {
				Log::INFO("settings menu: global {} = {}", a_row.key, *v);
			}
			break;
		case Applier::kGameIni:
			// the game's setting is the value itself (Set wrote it); the HUD's two are put on the HUD again
			ApplyToHud(a_row.key, a_value);
			break;
		case Applier::kSound:
			// the game's setting is the value itself (Set wrote it; the game restores volumes at start)
			break;
		case Applier::kSettingsIni:
			// the value already sits in the file (Set); its readers look there
			// (the achievement toast and the menu's starting view)
			break;
		case Applier::kNone:
		default:
			break;
		}
	}

	void Store::OnGameReady()
	{
		std::size_t                     filled = 0;
		std::unordered_set<std::string> filledIds;
		{
			std::scoped_lock lock(_lock);
			_gameLoaded = true;
		}
		for (const auto& r : kRows) {
			if (r.scope != Scope::kSave) {
				continue;
			}
			bool have = false;
			{
				std::scoped_lock lock(_lock);
				have = _save.contains(r.id);
			}
			if (!have) {
				const float d = StoredDefault(r);
				std::scoped_lock lock(_lock);
				_save[r.id] = d;
				filledIds.insert(r.id);
				++filled;
			}
		}
		EconomyService::GetSingleton()->OnGameReady();  // the player's price perk
		for (const auto& r : kRows) {
			// a gameglobal row is set only for a new character (filled from the new-game defaults just now);
			// a loaded game keeps its own value of the global
			if (r.applier == Applier::kGameGlobal) {
				if (filledIds.contains(r.id)) {
					float v = 0.0f;
					{
						std::scoped_lock lock(_lock);
						v = _save[r.id];
					}
					Apply(r, v);
				}
				continue;
			}
			Apply(r, Get(r));
		}
		HotkeyService::GetSingleton()->ApplyGameKeys();  // QuickLoot / moreHUD keys
		Log::INFO("settings menu: game ready - {} value(s) applied, {} taken from the new-game defaults", kRows.size(),
					 filled);
	}

	// ============================================================ co-save ====
	// Record SVAL v1: u32 count, then count x (u16 id length, id bytes, f32).
	// Ids the table no longer knows are kept, so a later build that brings
	// them back finds them again.
	void Store::OnSave(SKSE::SerializationInterface* a_intfc)
	{
		auto* self = GetSingleton();
		std::scoped_lock lock(self->_lock);
		if (!a_intfc->OpenRecord(kRecord, kVersion)) {
			Log::ERROR("settings menu: could not open the co-save record");
			return;
		}
		const auto count = static_cast<std::uint32_t>(self->_save.size());
		a_intfc->WriteRecordData(count);
		for (const auto& [id, v] : self->_save) {
			const auto len = static_cast<std::uint16_t>(id.size());
			a_intfc->WriteRecordData(len);
			a_intfc->WriteRecordData(id.data(), len);
			a_intfc->WriteRecordData(v);
		}
	}

	// one SVAL record (the co-save hands this store only those)
	void Store::OnLoad(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version, std::uint32_t)
	{
		auto* self = GetSingleton();
		if (a_version != kVersion) {
			Log::WARN("settings menu: co-save record version {} not understood - skipped", a_version);
			return;
		}
		std::uint32_t count = 0;
		if (!a_intfc->ReadRecordData(count)) {
			return;
		}
		std::scoped_lock lock(self->_lock);
		for (std::uint32_t i = 0; i < count; ++i) {
			std::uint16_t len = 0;
			if (!a_intfc->ReadRecordData(len)) {
				break;
			}
			std::string id(len, '\0');
			float       v = 0.0f;
			if (!a_intfc->ReadRecordData(id.data(), len) || !a_intfc->ReadRecordData(v)) {
				break;
			}
			if (const auto* row = FindRow(id)) {
				v = Snap(*row, v);
			}
			self->_save[id] = v;
			++self->_read;
		}
	}

	void Store::OnLoaded()
	{
		auto* self = GetSingleton();
		Log::INFO("settings menu: {} value(s) read from the save", self->_read);
		self->_read = 0;
	}

	void Store::OnRevert(SKSE::SerializationInterface*)
	{
		auto* self = GetSingleton();
		std::scoped_lock lock(self->_lock);
		self->_save.clear();
		self->_read = 0;
	}
}
