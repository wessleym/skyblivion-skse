#include "SettingsMenu/SoundService.h"

#include "Platform/FormCache.h"
#include "SettingsMenu/SettingsMenuTable.h"
#include "SettingsMenu/SettingsMenu.h"

#include <charconv>
#include <chrono>
#include <cstdlib>
#include <algorithm>
#include <format>

namespace SKYBSettingsMenu
{
	namespace
	{
		constexpr const char* kSection = "AudioMenu";

		// "Plugin.esm:XXXXXX" -> the form; an editor ID otherwise (through the form table: once each)
		RE::BGSSoundCategory* Resolve(std::string_view a_target)
		{
			const auto colon = a_target.rfind(':');
			if (colon != std::string_view::npos && a_target.find('.') < colon) {
				const auto plugin = a_target.substr(0, colon);
				const auto hex = a_target.substr(colon + 1);
				RE::FormID local = 0;
				const auto r = std::from_chars(hex.data(), hex.data() + hex.size(), local, 16);
				if (r.ec != std::errc{}) {
					return nullptr;
				}
				return FormCache::GetLocal<RE::BGSSoundCategory>(local, plugin, "settings menu (sound row)");
			}
			return FormCache::Get<RE::BGSSoundCategory>(a_target, "settings menu (sound row)");
		}

		// SkyrimPrefs.ini [AudioMenu] is the game's own: fAudioMasterVolume and eight uIDN / fValN pairs (a menu
		// category's form ID and its volume), all registered settings of the prefs collection. A menu category's
		// SetCategoryVolume writes its pair itself (SkyrimSE 1.7.104: BGSSoundCategory's SetCategoryVolume, 0x1403298D0,
		// finds or takes the pair by form ID and stores the value there), and the category takes the pair's value
		// back when the data loads (0x140329640). So nothing here touches the file: the journal's SaveSettings writes the
		// collection (OpenHandle, WriteAllSettings, CloseHandle), and so does SavePrefs when the settings menu closes.
		void SavePrefs()
		{
			auto* prefs = RE::INIPrefSettingCollection::GetSingleton();
			if (!prefs) {
				return;
			}
			if (prefs->OpenHandle(true)) {
				prefs->WriteAllSettings();
				prefs->CloseHandle();
				Log::INFO("settings menu: volumes saved (SkyrimPrefs.ini, the game's own write)");
			} else {
				Log::WARN("settings menu: SkyrimPrefs.ini could not be opened; the volumes last until the game closes");
			}
		}

		// What the journal's audio sliders do after a change (OptionChange, AE id 53310): the audio manager is told
		// that volumes changed, and every sound playing takes its new volume at once (music included).
		void VolumesChanged()
		{
			if (auto* audio = RE::BSAudioManager::GetSingleton()) {
				audio->ComposeMessage(RE::SOUND_MSG::VolumeChanged);
			}
		}

		const char* NameOf(const RE::BGSSoundCategory* a_cat)
		{
			const char* n = a_cat ? a_cat->GetFullName() : nullptr;
			return n && *n ? n : nullptr;
		}

		bool InGame()
		{
			auto* ui = RE::UI::GetSingleton();
			auto* player = RE::PlayerCharacter::GetSingleton();
			return player && player->GetParentCell() && !(ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME));
		}

		bool Within(const RE::BGSSoundCategory* a_cat, const RE::BGSSoundCategory* a_root)
		{
			for (int depth = 0; a_cat && depth < 16; ++depth, a_cat = a_cat->parentCategory) {
				if (a_cat == a_root) {
					return true;
				}
			}
			return false;
		}

		// samples chosen by hand (Skyblivion's own short sounds), by row target, as plugin and form ID (the
		// game keeps no editor IDs for sound descriptors, so they cannot be found by name)
		struct NamedSample
		{
			const char* plugin;
			RE::FormID  id;
		};
		const std::unordered_map<std::string, NamedSample> kNamedSamples{
			{ "SKYBAudioCategoryCombat", { "Skyblivion.esm", 0x073689 } },    // SKYBWpnLongswordImpactFleshNpcSD: a hit
			{ "SKYBAudioCategoryWorld", { "Skyblivion.esm", 0x02D544 } },     // SKYBObjSwitchMetalMechanicalSD
			{ "SKYBAudioCategoryAmbience", { "Skyblivion.esm", 0x0316D2 } },  // SKYBAmbAyleidDropSD: a drip (the thunder is too long)
		};
		// the category the game pauses while a menu is open (Combat and Ambience sit under it; their samples would be
		// heard only once the menu closed). While the settings menu is open those two hang from its parent instead.
		constexpr RE::FormID  kPausedCategory = 0x09F254;
		constexpr const char* kMasterTarget = "master";
		constexpr const char* kMasterSetting = "fAudioMasterVolume:AudioMenu";

		// a looping sound by Bethesda's naming (...LP, ...LPM, ...Loop): no use as a short sample
		bool LooksLooping(std::string_view a_edid)
		{
			return a_edid.ends_with("LP") || a_edid.ends_with("LPM") || a_edid.find("Loop") != std::string_view::npos ||
				   a_edid.find("LP_") != std::string_view::npos;
		}
	}

	SoundService* SoundService::GetSingleton()
	{
		static SoundService singleton;
		return &singleton;
	}

	void SoundService::Register()
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		if (dh) {
			for (auto* cat : dh->GetFormArray<RE::BGSSoundCategory>()) {
				if (cat && cat->flags.all(RE::BGSSoundCategory::Flag::kShouldAppearOnMenu)) {
					Log::INFO("settings menu: the game's audio list has {:08X} '{}' (menu volume {})",
							  cat->GetFormID(), NameOf(cat) ? NameOf(cat) : "", cat->volumeMult);
				}
			}
		}
		for (const auto& r : kRows) {
			if (r.applier != Applier::kSound || std::string_view(r.key) == kMasterTarget) {
				continue;
			}
			auto* cat = Resolve(r.key);
			if (!cat) {
				Log::WARN("settings menu: {}: no sound category {} - the row does nothing", r.id, r.key);
				continue;
			}
			_byTarget[r.key] = cat;
			// a short sample of the category for the slider; none for music, which is heard anyway
			const char* name = NameOf(cat);
			if (name && std::string_view(name) == "$Music") {
				continue;
			}
			// a named short sound for the rows whose first sound found is long or silent (World, Combat,
			// Ambience); else a plain sound of the category: a standard sound with no
			// conditions (a conditioned one may not play from the menu) that does not loop
			RE::BGSSoundDescriptorForm* pick = nullptr;
			if (const auto named = kNamedSamples.find(r.key); named != kNamedSamples.end()) {
				pick = FormCache::GetLocal<RE::BGSSoundDescriptorForm>(named->second.id, named->second.plugin, "settings menu (sample)");
			}
			if (!pick && dh) {
				for (auto* d : dh->GetFormArray<RE::BGSSoundDescriptorForm>()) {
					if (!d || !d->soundDescriptor || !Within(d->soundDescriptor->category, cat)) {
						continue;
					}
					auto* def = skyrim_cast<RE::BGSStandardSoundDef*>(d->soundDescriptor);
					if (!def || def->conditions || def->lengthCharacteristics.looping.any(RE::BGSStandardSoundDef::LengthCharacteristics::Looping::kLoop)) {
						continue;
					}
					const char* edid = d->GetFormEditorID();
					if (edid && LooksLooping(edid)) {
						continue;
					}
					pick = d;
					break;
				}
			}
			// no plain sound (Voice: every one has conditions): the first standard one that does not loop
			if (!pick && dh) {
				for (auto* d : dh->GetFormArray<RE::BGSSoundDescriptorForm>()) {
					auto* def = d && d->soundDescriptor && Within(d->soundDescriptor->category, cat) ?
									skyrim_cast<RE::BGSStandardSoundDef*>(d->soundDescriptor) :
									nullptr;
					if (def && !def->lengthCharacteristics.looping.any(RE::BGSStandardSoundDef::LengthCharacteristics::Looping::kLoop)) {
						pick = d;
						break;
					}
				}
			}
			if (cat->parentCategory && cat->parentCategory->GetFormID() == kPausedCategory) {
				_unpause.push_back(cat);
			}
			if (pick) {
				_demo[r.key] = pick;  // the record itself: a form cannot change mid-session
				std::string chain;
				for (auto* c = pick->soundDescriptor->category; c && chain.size() < 400; c = c->parentCategory) {
					chain += std::format(" {:08X}", c->GetFormID());
				}
				Log::INFO("settings menu: {} sample {:08X} {} (categories{})", r.id, pick->GetFormID(),
						  pick->GetFormEditorID() ? pick->GetFormEditorID() : "", chain);
			} else {
				Log::INFO("settings menu: {} has no short sample to play", r.id);
			}
		}
		Log::INFO("settings menu: {} volume row(s) resolved", _byTarget.size());
		if (auto* ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
		}
	}

	RE::BSEventNotifyControl SoundService::ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
														RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
	{
		if (!a_event || a_event->menuName != SettingsMenu::MENU_NAME) {
			return RE::BSEventNotifyControl::kContinue;
		}
		const bool opening = a_event->opening;
		SKSE::GetTaskInterface()->AddTask([this, opening] {
			if (opening) {
				for (auto* c : _unpause) {
					if (!_savedParent.contains(c) && c->parentCategory) {
						_savedParent[c] = c->parentCategory;
						c->parentCategory = c->parentCategory->parentCategory;
					}
				}
				if (!_savedParent.empty()) {
					Log::INFO("settings menu: {} sound categor(ies) out of the menu pause while the menu is open", _savedParent.size());
				}
			} else {
				for (auto& [c, parent] : _savedParent) {
					c->parentCategory = parent;
				}
				_savedParent.clear();
				if (_dirty) {
					_dirty = false;
					SavePrefs();
				}
			}
		});
		return RE::BSEventNotifyControl::kContinue;
	}

	RE::BGSSoundCategory* SoundService::Find(std::string_view a_target) const
	{
		const auto it = _byTarget.find(std::string(a_target));
		return it != _byTarget.end() ? it->second : nullptr;
	}

	std::optional<float> SoundService::Get(std::string_view a_target) const
	{
		if (a_target == kMasterTarget) {
			if (auto* audio = RE::BSAudioManager::GetSingleton()) {
				return audio->masterVolume;
			}
			return std::nullopt;
		}
		// the menu volume itself: what the journal's slider shows and the pair holds (GetCategoryVolume is the volume
		// heard: times the record's static multiplier and every parent's volume)
		auto* cat = Find(a_target);
		return cat ? std::optional<float>(cat->volumeMult) : std::nullopt;
	}

	// As the journal's slider (OptionChange): the category's menu volume, which also stores it in the game's own
	// [AudioMenu] pair, then the audio manager's volume-changed message so what is playing follows. Saved to
	// SkyrimPrefs.ini by the game's writer when the menu closes.
	void SoundService::Set(std::string_view a_target, float a_value)
	{
		a_value = std::clamp(a_value, 0.0f, 1.0f);
		if (a_target == kMasterTarget) {
			SetMaster(a_value);
			return;
		}
		auto* cat = Find(a_target);
		if (!cat) {
			return;
		}
		const float before = cat->volumeMult;
		cat->SetCategoryVolume(a_value);
		VolumesChanged();
		_dirty = true;
		Log::INFO("settings menu: volume {:08X} {} -> {} (heard at {})", cat->GetFormID(), before, cat->volumeMult,
				  cat->GetCategoryVolume());
		PlaySample(a_target);
	}

	// The master volume, as the journal's Master slider: the audio manager's own field, the volume-changed message,
	// and the game's fAudioMasterVolume setting (saved with the rest when the menu closes).
	void SoundService::SetMaster(float a_value)
	{
		auto* audio = RE::BSAudioManager::GetSingleton();
		if (!audio) {
			return;
		}
		const float before = audio->masterVolume;
		audio->masterVolume = a_value;
		VolumesChanged();
		auto* prefs = RE::INIPrefSettingCollection::GetSingleton();
		auto* setting = prefs ? prefs->GetSetting(kMasterSetting) : nullptr;
		if (setting && setting->GetType() == RE::Setting::Type::kFloat) {
			setting->data.f = a_value;
			_dirty = true;
		} else {
			Log::WARN("settings menu: no {} setting; the master volume lasts until the game closes", kMasterSetting);
		}
		Log::INFO("settings menu: master volume {} -> {}", before, a_value);
		const auto it = _demo.find("Skyrim.esm:064451");
		if (it != _demo.end()) {
			PlaySample("Skyrim.esm:064451");
		}
	}

	void SoundService::PlaySample(std::string_view a_target)
	{
		const auto it = _demo.find(std::string(a_target));
		if (it == _demo.end()) {
			return;
		}
		// not more than about three a second while a slider is held
		static auto last = std::chrono::steady_clock::time_point{};
		const auto  now = std::chrono::steady_clock::now();
		if (now - last < std::chrono::milliseconds(300)) {
			return;
		}
		last = now;
		// started at the player and following them (most samples are placed sounds; played with no place they
		// sound at the world's origin and are not heard - only the interface and footstep ones are)
		auto* audio = RE::BSAudioManager::GetSingleton();
		RE::BGSSoundDescriptorForm* form = it->second;
		if (!audio || !form) {
			return;
		}
		RE::BSSoundHandle handle;
		if (!audio->GetSoundHandle(handle, static_cast<RE::BSISoundDescriptor*>(form)) || !handle.IsValid()) {
			Log::INFO("settings menu: sample {:08X} could not be started", form->GetFormID());
			return;
		}
		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			handle.SetPosition(player->GetPosition());
			if (auto* node = player->Get3D()) {
				handle.SetObjectToFollow(node);
			}
		}
		handle.Play();
	}

	const char* SoundService::Label(std::string_view a_target) const
	{
		return NameOf(Find(a_target));
	}

	std::optional<float> SoundService::Default(std::string_view a_target) const
	{
		if (a_target == kMasterTarget) {
			return 1.0f;
		}
		auto* cat = Find(a_target);
		if (!cat) {
			return std::nullopt;
		}
		// the record's default menu volume, 0..65535 (CommonLib's GetDefaultMenuValue divides as integers: 0)
		return static_cast<float>(cat->defaultMenuValue) / 65535.0f;
	}
}
