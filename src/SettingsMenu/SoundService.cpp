#include "SettingsMenu/SoundService.h"

#include "SettingsMenu/SettingsMenuTable.h"
#include "SettingsMenu/SettingsMenu.h"

#include <charconv>
#include <chrono>
#include <cstdlib>
#include <algorithm>
#include <format>

#include <Windows.h>

namespace SKYBSettingsMenu
{
	namespace
	{
		constexpr const char* kSection = "AudioMenu";

		// "Plugin.esm:XXXXXX" -> the form; an editor ID otherwise
		RE::BGSSoundCategory* Resolve(std::string_view a_target)
		{
			const auto colon = a_target.rfind(':');
			if (colon != std::string_view::npos && a_target.find('.') < colon) {
				const auto plugin = a_target.substr(0, colon);
				const auto hex = a_target.substr(colon + 1);
				RE::FormID local = 0;
				const auto r = std::from_chars(hex.data(), hex.data() + hex.size(), local, 16);
				auto*      dh = RE::TESDataHandler::GetSingleton();
				if (r.ec != std::errc{} || !dh) {
					return nullptr;
				}
				return dh->LookupForm<RE::BGSSoundCategory>(local, plugin);
			}
			return RE::TESForm::LookupByEditorID<RE::BGSSoundCategory>(std::string(a_target));
		}

		// SkyrimPrefs.ini, where the game keeps the menu volumes ([AudioMenu] uIDN = a category's form ID, fValN its
		// volume). They are not settings the game registers (the prefs collection has no uIDN / fValN), so
		// the file itself is read and written.
		const char* PrefsPath()
		{
			auto* prefs = RE::INIPrefSettingCollection::GetSingleton();
			return prefs && prefs->subKey[0] ? prefs->subKey : nullptr;
		}

		// the N of the uIDN holding this form ID; -1 when none, and a_free the first unused N
		int FileIndex(RE::FormID a_id, int* a_free = nullptr)
		{
			const char* path = PrefsPath();
			if (!path) {
				return -1;
			}
			char buf[64];
			for (int i = 0; i < 64; ++i) {
				GetPrivateProfileStringA(kSection, std::format("uID{}", i).c_str(), "", buf, sizeof(buf), path);
				if (!buf[0]) {
					if (a_free) {
						*a_free = i;
					}
					return -1;
				}
				if (static_cast<RE::FormID>(std::strtoul(buf, nullptr, 10)) == a_id) {
					return i;
				}
			}
			return -1;
		}

		std::optional<float> FileVolume(RE::FormID a_id)
		{
			const int i = FileIndex(a_id);
			const char* path = PrefsPath();
			if (i < 0 || !path) {
				return std::nullopt;
			}
			char buf[64];
			GetPrivateProfileStringA(kSection, std::format("fVal{}", i).c_str(), "", buf, sizeof(buf), path);
			if (!buf[0]) {
				return std::nullopt;
			}
			return static_cast<float>(std::atof(buf));
		}

		bool WriteFileVolume(RE::FormID a_id, float a_value)
		{
			const char* path = PrefsPath();
			if (!path) {
				return false;
			}
			int free = -1;
			int i = FileIndex(a_id, &free);
			if (i < 0) {
				if (free < 0) {
					return false;
				}
				i = free;
				WritePrivateProfileStringA(kSection, std::format("uID{}", i).c_str(), std::to_string(a_id).c_str(), path);
			}
			return WritePrivateProfileStringA(kSection, std::format("fVal{}", i).c_str(), std::format("{:.4f}", a_value).c_str(),
											  path) != 0;
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

		// the record's static volume multiplier (VNAM, 0..65535; CommonLib's getter divides as integers). The
		// category's volume reads back as menu volume x this (music: 0.7), so a row divides it out.
		float StaticMult(const RE::BGSSoundCategory* a_cat)
		{
			const float m = static_cast<float>(a_cat->staticMult) / 65535.0f;
			return m > 0.001f ? m : 1.0f;
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
		// the music row's test: Skyblivion's reveal sting (one track, ducks the music, then ends by itself)
		constexpr const char*  kMusicTarget = "Skyrim.esm:071E64";
		constexpr RE::FormID   kRevealSting = 0x00249D;
		constexpr const char*  kRevealPlugin = "Skyblivion.esm";

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
					const auto f = FileVolume(cat->GetFormID());
					Log::INFO("settings menu: the game's audio list has {:08X} '{}' (category {}, SkyrimPrefs.ini {})",
							  cat->GetFormID(), NameOf(cat) ? NameOf(cat) : "", cat->GetCategoryVolume(),
							  f ? std::format("{}", *f) : std::string("none"));
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
			if (const auto named = kNamedSamples.find(r.key); named != kNamedSamples.end() && dh) {
				pick = dh->LookupForm<RE::BGSSoundDescriptorForm>(named->second.id, named->second.plugin);
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
				_demo[r.key] = pick->GetFormID();
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
		auto* cat = Find(a_target);
		if (!cat) {
			return std::nullopt;
		}
		// in a game: what the row was last set to, else the category's volume (what is heard); on the title
		// screen the file is what the next game takes
		if (InGame()) {
			if (const auto it = _set.find(std::string(a_target)); it != _set.end()) {
				return it->second;
			}
			return cat->GetCategoryVolume() / StaticMult(cat);
		}
		if (const auto f = FileVolume(cat->GetFormID())) {
			return *f;
		}
		return cat->GetCategoryVolume();
	}

	void SoundService::Set(std::string_view a_target, float a_value)
	{
		if (a_target == kMasterTarget) {
			SetMaster(a_value);
			return;
		}
		auto* cat = Find(a_target);
		if (!cat) {
			return;
		}
		const float before = cat->GetCategoryVolume();
		_set[std::string(a_target)] = a_value;
		cat->SetCategoryVolume(a_value);
		// and through the game's own SoundCategory.SetVolume, which mods use to change a volume while it plays
		if (auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton()) {
			auto*      policy = vm->GetObjectHandlePolicy();
			const auto handle = policy ? policy->GetHandleForObject(RE::BGSSoundCategory::FORMTYPE, cat) : 0;
			if (handle) {
				RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none;
				vm->DispatchMethodCall(handle, "SoundCategory", "SetVolume", RE::MakeFunctionArguments(static_cast<float>(a_value)),
									   none);
			}
		}
		const bool saved = WriteFileVolume(cat->GetFormID(), a_value);
		Log::INFO("settings menu: volume {:08X} {} -> {} (reads back {}, record 48h {}; {})", cat->GetFormID(), before,
				  a_value, cat->GetCategoryVolume(), cat->unk48, saved ? "saved to SkyrimPrefs.ini" : "not saved: no SkyrimPrefs.ini path");
		if (_demo.contains(std::string(a_target))) {
			PlaySample(a_target);
		} else {
			if (a_target == kMusicTarget) {
				PlayMusicTest();
			}
			Nudge();
		}
	}

	// The master volume: the audio manager's own field, which the game multiplies into every sound, set and
	// then taken in the way the music is (Nudge); kept as SkyrimPrefs.ini [AudioMenu] fAudioMasterVolume, which the
	// game reads at start. The mastering voice's level is logged too, to see which of the two the game uses.
	void SoundService::SetMaster(float a_value)
	{
		auto* audio = RE::BSAudioManager::GetSingleton();
		if (!audio) {
			return;
		}
		const float before = audio->masterVolume;
		audio->masterVolume = a_value;
		bool saved = false;
		if (const char* path = PrefsPath()) {
			saved = WritePrivateProfileStringA(kSection, "fAudioMasterVolume", std::format("{:.4f}", a_value).c_str(), path) != 0;
		}
		float voice = -1.0f;
		if (auto* xa = static_cast<RE::BSXAudio2Audio*>(RE::BSAudioManager::QPlatformInstance()); xa && xa->masteringVoice) {
			xa->masteringVoice->GetVolume(&voice);
		}
		Log::INFO("settings menu: master volume {} -> {} (mastering voice {}; {})", before, a_value, voice,
				  saved ? "saved to SkyrimPrefs.ini" : "not saved");
		Nudge();
		const auto it = _demo.find("Skyrim.esm:064451");
		if (it != _demo.end()) {
			PlaySample("Skyrim.esm:064451");
		}
	}

	// the reveal sting as the music row's test, at most once in eight seconds (it is several seconds long)
	void SoundService::PlayMusicTest()
	{
		static auto last = std::chrono::steady_clock::time_point{};
		const auto  now = std::chrono::steady_clock::now();
		if (last != std::chrono::steady_clock::time_point{} && now - last < std::chrono::seconds(8)) {
			return;
		}
		auto* dh = RE::TESDataHandler::GetSingleton();
		auto* music = dh ? dh->LookupForm<RE::BGSMusicType>(kRevealSting, kRevealPlugin) : nullptr;
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!music || !vm) {
			return;
		}
		last = now;
		auto*      policy = vm->GetObjectHandlePolicy();
		const auto handle = policy ? policy->GetHandleForObject(RE::BGSMusicType::FORMTYPE, music) : 0;
		if (handle) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none;
			vm->DispatchMethodCall(handle, "MusicType", "Add", RE::MakeFunctionArguments(), none);
		}
	}

	// The music takes a new volume only when the interface volume is changed after it (a silent sound start
	// does not do it). A row without a sample of its own (music) sets the interface
	// category to the volume it already has, through the game's SoundCategory.SetVolume: nothing changes but the
	// music follows.
	void SoundService::Nudge()
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		auto* ui = dh ? dh->LookupForm<RE::BGSSoundCategory>(0x064451, "Skyrim.esm") : nullptr;
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!ui || !vm) {
			return;
		}
		const float v = ui->GetCategoryVolume();
		ui->SetCategoryVolume(v);
		auto*      policy = vm->GetObjectHandlePolicy();
		const auto handle = policy ? policy->GetHandleForObject(RE::BGSSoundCategory::FORMTYPE, ui) : 0;
		if (handle) {
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none;
			vm->DispatchMethodCall(handle, "SoundCategory", "SetVolume", RE::MakeFunctionArguments(static_cast<float>(v)), none);
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
		auto* form = RE::TESForm::LookupByID<RE::BGSSoundDescriptorForm>(it->second);
		if (!audio || !form) {
			return;
		}
		RE::BSSoundHandle handle;
		if (!audio->GetSoundHandle(handle, static_cast<RE::BSISoundDescriptor*>(form)) || !handle.IsValid()) {
			Log::INFO("settings menu: sample {:08X} could not be started", it->second);
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
