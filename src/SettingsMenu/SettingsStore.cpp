// Moved from SKYBUIDemo.dll into Skyblivion.dll, 2026-10-05 (fold-in, UI integration workspace
// ledger L-0056). Source of record until then: game\native\SKYBUIDemo\src in that workspace.
#include "SettingsMenu/SettingsStore.h"

#include <charconv>
#include <cstring>

#include "SettingsMenu/CombatService.h"
#include "SettingsMenu/EconomyService.h"
#include "Hotkeys/HotkeyService.h"
#include "SettingsMenu/InterfaceAdapters.h"
#include "Degradation/Settings.h"
#include "Settings/SettingsService.h"
#include "Storage/CoSave.h"

namespace SKYBSettingsMenu
{
	namespace
	{
		// the record in the plugin's co-save (Storage/CoSave); until 2026-10-05 SKYBUIDemo.dll kept it
		// under its own id 'SKSM', which saves made since then no longer carry
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
		// Safety net: 'coc' from the title screen starts a game WITHOUT the
		// kPostLoadGame / kNewGame messages (seen in game 2026-09-29), which
		// left the menu editing the new-game defaults in game. When a loading
		// screen closes with a world and no title screen, make sure the game
		// is set up. On an ordinary load kPostLoadGame got there first and
		// this does nothing.
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

	void Store::Apply(const RowDef& a_row, float a_value)
	{
		switch (a_row.applier) {
		case Applier::kDegradation:
			// the same DLL since the fold-in (L-0058): a direct call, no SKSE message
			if (!SKYBDeg::Settings::SetOverride(a_row.section, a_row.key, a_value)) {
				Log::WARN("settings menu: degradation has no key [{}] {}", a_row.section, a_row.key);
			}
			break;
		case Applier::kCombat:
			CombatService::GetSingleton()->Set(a_row.key, a_value);
			break;
		case Applier::kGlobalVar:
			InterfaceAdapters::GetSingleton()->SetGlobal(a_row.key, a_value);
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
		case Applier::kSettingsIni:
			// the value already sits in the file (Set); mirror it into the
			// game (the achievement toast gate global)
			Settings::SettingsService::ApplyToGame();
			break;
		case Applier::kNone:
		default:
			break;
		}
	}

	void Store::OnGameReady()
	{
		std::size_t filled = 0;
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
				++filled;
			}
		}
		EconomyService::GetSingleton()->OnGameReady();  // the player's price perk
		for (const auto& r : kRows) {
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
