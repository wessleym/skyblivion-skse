#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The settings menu's volume rows (applier "sound"): one of the game's sound categories each, the ones its
// own audio settings list. A row's target names the category: "Plugin.esm:XXXXXX" (its form ID in that
// plugin) or its editor ID.
//
// The value is the category's menu volume, set the way the journal's audio sliders set it: SetCategoryVolume, which
// also stores it in the game's own SkyrimPrefs.ini [AudioMenu] pair for that category, then the audio manager's
// volume-changed message, so sounds already playing (music included) change at once. The master row sets the audio
// manager's master volume and the game's fAudioMasterVolume setting. When the menu closes after a change, the game's
// prefs writer saves the file (as the journal's SaveSettings does); the game reads it back at the next start.
// Each change plays a short sound of the category (not for music, which is heard anyway).
namespace SKYBSettingsMenu
{
	class SoundService : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		// while the settings menu is open the samples' categories do not pause with the game (otherwise only UI and
		// footstep samples are heard); their flags go back when it closes
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
											  RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

		static SoundService* GetSingleton();

		void Register();  // kDataLoaded: resolves the rows' categories, logs the game's menu categories

		[[nodiscard]] std::optional<float> Get(std::string_view a_target) const;
		void                               Set(std::string_view a_target, float a_value);
		// the category's own name (often a translation key such as "$Effects"); nullptr when unresolved
		[[nodiscard]] const char*          Label(std::string_view a_target) const;
		// the category's default menu volume, when resolved
		[[nodiscard]] std::optional<float> Default(std::string_view a_target) const;

	private:
		SoundService() = default;

		[[nodiscard]] RE::BGSSoundCategory* Find(std::string_view a_target) const;
		void                                PlaySample(std::string_view a_target);
		void                                SetMaster(float a_value);

		std::unordered_map<std::string, RE::BGSSoundCategory*> _byTarget;
		std::unordered_map<std::string, RE::BGSSoundDescriptorForm*> _demo;  // a short sound of each row's category
		// rows' categories under the one the game pauses in menus, moved from it while the settings menu is open
		std::vector<RE::BGSSoundCategory*>                                  _unpause;
		std::unordered_map<RE::BGSSoundCategory*, RE::BGSSoundCategory*>    _savedParent;
		// a volume changed since the menu opened: the prefs are written when it closes
		bool                                                   _dirty = false;
	};
}
